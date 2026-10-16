import { spawn } from 'node:child_process'
import { existsSync, mkdirSync, renameSync, writeFileSync } from 'node:fs'
import { readFile } from 'node:fs/promises'
import { join, resolve, dirname } from 'node:path'
import { createInterface } from 'node:readline'
import { BleHost } from './ble-host.js'
import {
  QuotamonController,
  type AccountSource
} from './quotamon-controller.js'
import { RunningOrcaSource } from './running-orca-source.js'
import { readCopilotQuota } from './copilot-quota-source.js'
import type { RuntimeMetrics } from './runtime-metrics.js'
import {
  defaultConfig,
  record,
  validateConfig,
  type Config,
  type DisplayAccount
} from './quota-snapshot.js'
import {
  startPortableServer,
  type BridgePreferences
} from './portable-server.js'

const emit = (value: unknown): void => {
  process.stdout.write(JSON.stringify(value) + '\n')
}
const args = process.argv.slice(2)
const option = (name: string): string | undefined => {
  const i = args.indexOf(name)
  return i < 0 ? undefined : args[i + 1]
}
const root = resolve(option('--root') || dirname(process.argv[1]))
const helper = resolve(option('--helper') || join(root, 'Quotamon.exe'))
const configFile = resolve(
  option('--config') ||
    join(process.env.APPDATA || root, 'Quotamon', 'settings.json')
)
let preferences: BridgePreferences = { copilot: true, startup: false }
let config: Config = defaultConfig()
let stopping = false,
  restart: ReturnType<typeof setTimeout> | undefined
let controller: QuotamonController,
  radio: boolean | null = null
let activeHost: BleHost | undefined
let copilotCached: DisplayAccount | undefined,
  copilotPending: Promise<DisplayAccount> | undefined
async function copilot(): Promise<DisplayAccount> {
  if (
    copilotCached?.updated_at &&
    Date.now() / 1000 - copilotCached.updated_at < 30
  )
    return copilotCached
  if (copilotPending) return copilotPending
  copilotPending = readCopilotQuota(process.env.APPDATA || '')
  try {
    const result = await copilotPending
    if (source.isAvailable()) copilotCached = result
    return result
  } finally {
    copilotPending = undefined
  }
}
async function load(): Promise<void> {
  if (!existsSync(configFile)) return
  const value: unknown = JSON.parse(await readFile(configFile, 'utf8'))
  if (!record(value)) throw Error('Invalid settings')
  config = validateConfig(value.config)
  if (
    !record(value.preferences) ||
    typeof value.preferences.copilot !== 'boolean'
  )
    throw Error('Invalid preferences')
  preferences = {
    copilot: value.preferences.copilot,
    startup: args.includes('--startup-enabled')
  }
}
function persist(next: Config): void {
  const text = JSON.stringify({ config: next, preferences }, null, 2) + '\n'
  mkdirSync(dirname(configFile), { recursive: true })
  writeFileSync(configFile + '.tmp', text, { mode: 0o600 })
  renameSync(configFile + '.tmp', configFile)
  config = next
}
const source = new RunningOrcaSource()
const accountSource: AccountSource = {
  getSnapshot: () => source.getSnapshot(),
  refreshIfStale: () => source.refreshIfStale(),
  onChanged: (listener) => source.onChanged(listener),
  isAvailable: () => source.isAvailable()
}
let wasAvailable = false
source.onChanged(() => {
  const available = source.isAvailable()
  const firstConnection = available && !wasAvailable
  wasAvailable = available
  if (!available) copilotCached = undefined
  if (firstConnection && controller) void controller.refresh().catch(() => {})
})
function configureCopilot(): void {
  const installed = existsSync(
    join(
      process.env.APPDATA || '',
      'npm/node_modules/@github/copilot/node_modules/@github/copilot-win32-x64/copilot.exe'
    )
  )
  if (preferences.copilot && installed) accountSource.copilot = copilot
  else {
    delete accountSource.copilot
    copilotCached = undefined
  }
}
function createController(): void {
  const host = new BleHost(helper, (path) =>
    spawn(path, ['--ble-resource'], {
      windowsHide: true,
      stdio: 'pipe',
      env: {
        SystemRoot: process.env.SystemRoot,
        windir: process.env.windir,
        TEMP: process.env.TEMP,
        TMP: process.env.TMP
      }
    })
  )
  activeHost = host
  radio = null
  host.on('event', (value) => {
    if (record(value) && value.type === 'adapter')
      radio =
        value.available === true &&
        value.central === true &&
        value.low_energy === true
  })
  host.on('fault', () => {
    if (stopping || restart || activeHost !== host) return
    restart = setTimeout(() => {
      restart = undefined
      if (stopping || activeHost !== host) return
      activeHost = undefined
      controller.dispose()
      createController()
    }, 5000)
  })
  controller = new QuotamonController(host, accountSource, persist, config)
  controller.on('runtime-metrics', (metrics: RuntimeMetrics) => {
    const path = join(dirname(configFile), 'runtime-metrics.json')
    try {
      writeFileSync(path + '.tmp', JSON.stringify({ received_at: new Date().toISOString(), ...metrics }) + '\n', { mode: 0o600 })
      renameSync(path + '.tmp', path)
    } catch { /* Diagnostics must not interrupt display data or settings. */ }
  })
  void host.request('probe').catch(() => {
    if (activeHost === host) radio = false
  })
  if (source.isAvailable()) void controller.refresh().catch(() => {})
}
let nextNative = 0
const nativePending = new Map<
  string,
  { resolve(): void; reject(): void; timer: ReturnType<typeof setTimeout> }
>()
function setStartup(enabled: boolean): Promise<void> {
  const id = String(++nextNative)
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      nativePending.delete(id)
      reject(Error('Launcher unavailable'))
    }, 5000)
    nativePending.set(id, {
      resolve,
      reject: () => reject(Error('Startup setting failed')),
      timer
    })
    emit({ type: 'startup', id, enabled })
  })
}
const input = createInterface({ input: process.stdin })
input.on('line', (line) => {
  if (line.length > 8192) return
  try {
    const value: unknown = JSON.parse(line)
    if (!record(value) || typeof value.id !== 'string') return
    const pending = nativePending.get(value.id)
    if (!pending) return
    clearTimeout(pending.timer)
    nativePending.delete(value.id)
    if (value.ok === true) pending.resolve()
    else pending.reject()
  } catch {
    /* Private launcher channel; reject malformed messages silently. */
  }
})

async function main(): Promise<void> {
  await load()
  preferences.startup = args.includes('--startup-enabled')
  configureCopilot()
  createController()
  const server = await startPortableServer({
    controller: () => controller,
    sourceAvailable: () => source.isAvailable(),
    preferences: () => preferences,
    async setPreferences(next) {
      if (next.startup !== preferences.startup) await setStartup(next.startup)
      const changed = next.copilot !== preferences.copilot
      preferences = next
      configureCopilot()
      persist(config)
      if (changed) {
        if (restart) {
          clearTimeout(restart)
          restart = undefined
        }
        activeHost = undefined
        controller.dispose()
        createController()
        await controller.refresh()
      }
    },
    radio: () => radio,
    webRoot: join(root, 'web'),
    assetsRoot: join(root, 'assets')
  })
  const stop = async (): Promise<void> => {
    if (stopping) return
    stopping = true
    if (restart) clearTimeout(restart)
    for (const pending of nativePending.values()) {
      clearTimeout(pending.timer)
      pending.reject()
    }
    nativePending.clear()
    source.dispose()
    controller.dispose()
    await server.close()
    process.exit(0)
  }
  process.on('SIGTERM', () => {
    void stop()
  })
  process.on('SIGINT', () => {
    void stop()
  })
  input.on('close', () => {
    void stop()
  })
  emit({ type: 'ready', url: server.url })
  await source.start()
  if (source.isAvailable()) await controller.refresh()
}
void main().catch(() => {
  emit({
    type: 'error',
    message: '시작하지 못했습니다. 포터블 폴더와 설정 파일을 확인하세요.'
  })
  source.dispose()
  if (controller) controller.dispose()
  process.exit(1)
})
