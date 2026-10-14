// Read-only preflight. Raw account responses stay in memory; output contains counts only.
import { execFile, spawn } from 'node:child_process'
import { promisify } from 'node:util'
import { existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { resolve } from 'node:path'
import { createInterface } from 'node:readline'
import {
  fromOrca,
  defaultConfig,
  makeSnapshot
} from '../orca-integration/dist/src/quota-snapshot.js'
import {
  fragment,
  Reassembler
} from '../orca-integration/dist/src/ble-protocol.js'
import { readCopilotQuota } from '../orca-integration/dist/src/copilot-quota-source.js'

const run = promisify(execFile)
const root = fileURLToPath(new URL('../', import.meta.url))
const orca =
  process.env.ORCA_CLI_COMMAND ||
  (process.platform === 'win32'
    ? resolve(
        process.env.LOCALAPPDATA || '',
        'Programs/orca/resources/bin/orca.exe'
      )
    : process.platform === 'linux'
      ? 'orca-ide'
      : 'orca')

async function query(args) {
  const { stdout } = await run(orca, args, {
    timeout: 20000,
    maxBuffer: 1024 * 1024,
    windowsHide: true
  })
  const envelope = JSON.parse(stdout)
  if (envelope.ok !== true || !envelope.result) throw Error('orca-query-failed')
  return envelope.result
}

async function radio(scan) {
  const helper = resolve(root, 'dist/orca-resource/Quotamon.Ble.exe')
  if (process.platform !== 'win32' || !existsSync(helper))
    return { checked: false }
  const child = spawn(helper, [], {
    windowsHide: true,
    stdio: ['pipe', 'pipe', 'pipe']
  })
  const lines = createInterface({ input: child.stdout })
  const devices = new Set()
  let adapter = null
  let failed = false
  child.stderr.on('data', () => {})
  child.on('error', () => {
    failed = true
  })
  child.stdin.on('error', () => {
    failed = true
  })
  lines.on('line', (line) => {
    try {
      const event = JSON.parse(line)
      if (event.type === 'adapter')
        adapter = {
          available: event.available === true,
          central: event.central === true,
          low_energy: event.low_energy === true
        }
      if (event.type === 'device' && typeof event.address === 'string')
        devices.add(event.address)
      if (event.type === 'result' && event.ok !== true) failed = true
    } catch {
      failed = true
    }
  })
  try {
    child.stdin.write(JSON.stringify({ id: 'probe', command: 'probe' }) + '\n')
    if (scan)
      child.stdin.write(JSON.stringify({ id: 'scan', command: 'scan' }) + '\n')
    await new Promise((resolve) => setTimeout(resolve, scan ? 21000 : 2500))
    return {
      checked: adapter !== null && !failed,
      adapter,
      ...(scan ? { scan_seconds: 20, quotamon_advertisers: devices.size } : {}),
      connected: false
    }
  } finally {
    lines.close()
    child.stdin.end()
    child.kill()
  }
}

try {
  const status = await query(['status', '--json'])
  if (!status.app?.running || status.runtime?.state !== 'ready')
    throw Error('orca-not-ready')
  const source = await query(['account', 'list', '--json'])
  const accounts = fromOrca(source)
  const copilot =
    process.argv.includes('--copilot') && process.platform === 'win32'
  if (copilot) accounts.push(await readCopilotQuota(process.env.APPDATA || ''))
  const config = defaultConfig()
  // Ephemeral verification selection: never saved to Orca or transmitted to a device.
  config.accounts = accounts.map((a) => ({
    id: a.id,
    alias: '',
    plan: '',
    enabled: true
  }))
  const body = Buffer.from(JSON.stringify(makeSnapshot(accounts, config)))
  const frames = fragment(1, 1, body, 20)
  const receiver = new Reassembler()
  let result
  for (const frame of frames) result = receiver.push(frame) || result
  if (!result || !Buffer.from(result.body).equals(body))
    throw Error('wire-roundtrip-failed')
  const providers = {}
  for (const account of accounts) {
    const row = (providers[account.provider] ||= {
      accounts: 0,
      ready: 0,
      stale: 0,
      unknown: 0
    })
    row.accounts++
    row[account.state]++
  }
  console.log(
    JSON.stringify(
      {
        app_version: status.runtime.appVersion,
        runtime_ready: true,
        source: 'running-orca-cached-quota',
        copilot_sdk_checked: copilot,
        providers,
        wire: {
          bytes: body.length,
          value_size: 20,
          frames: frames.length,
          roundtrip: true
        },
        ble: await radio(process.argv.includes('--scan')),
        hardware_transfer_verified: false
      },
      null,
      2
    )
  )
} catch {
  // Do not expose subprocess stderr, provider responses, account labels, or tokens.
  console.error(
    'Running Orca verification failed. Check Orca is ready and the integration core is built.'
  )
  process.exitCode = 1
}
