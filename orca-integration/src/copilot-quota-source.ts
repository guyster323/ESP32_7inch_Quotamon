import { existsSync, readdirSync } from 'node:fs'
import { access } from 'node:fs/promises'
import { delimiter, join } from 'node:path'
import { pathToFileURL } from 'node:url'
import { copilotQuota, DisplayAccount, record } from './quota-snapshot.js'
interface QuotaClient {
  start(): Promise<void>
  stop(): Promise<void>
  forceStop(): void
  rpc: {
    account: { getQuota(params: Record<string, never>): Promise<unknown> }
  }
}
interface QuotaSdk {
  CopilotClient: new (options: {
    connection: unknown
    logLevel: string
  }) => QuotaClient
  RuntimeConnection: { forStdio(options: { path: string }): unknown }
}
export interface CopilotInstall {
  cli: string
  sdk: string
}
function isQuotaSdk(value: unknown): value is QuotaSdk {
  return (
    record(value) &&
    typeof value.CopilotClient === 'function' &&
    record(value.RuntimeConnection) &&
    typeof value.RuntimeConnection.forStdio === 'function'
  )
}
function paired(root: string): CopilotInstall | undefined {
  const cli = join(root, 'copilot.exe'),
    sdk = join(root, 'copilot-sdk', 'index.js')
  return existsSync(cli) && existsSync(sdk) ? { cli, sdk } : undefined
}
function versionRank(name: string): [number, number, number] | undefined {
  const parts = /^(\d+)\.(\d+)\.(\d+)$/.exec(name)
  if (!parts) return
  return [Number(parts[1]), Number(parts[2]), Number(parts[3])]
}
function cliOnPath(env: NodeJS.ProcessEnv): string | undefined {
  for (const dir of (env.PATH || '').split(delimiter)) {
    const folder = dir.trim()
    if (!folder) continue
    const candidate = join(folder, 'copilot.exe')
    if (existsSync(candidate)) return candidate
  }
}
// npm -g keeps the executable and SDK together. The Windows installer puts
// copilot.exe on PATH and the matching SDK under LOCALAPPDATA\copilot\pkg.
export function findCopilotInstall(
  appData: string,
  env: NodeJS.ProcessEnv = process.env
): CopilotInstall | undefined {
  const npm = paired(
    join(
      appData,
      'npm',
      'node_modules',
      '@github',
      'copilot',
      'node_modules',
      '@github',
      'copilot-win32-x64'
    )
  )
  if (npm) return npm
  const local = env.LOCALAPPDATA || ''
  if (!local) return
  let versions: string[] = []
  try {
    versions = readdirSync(join(local, 'copilot', 'pkg', 'win32-x64'), {
      withFileTypes: true
    })
      .filter((entry) => entry.isDirectory() && versionRank(entry.name))
      .map((entry) => entry.name)
      .sort((a, b) => {
        const left = versionRank(a) || [0, 0, 0],
          right = versionRank(b) || [0, 0, 0]
        return left[0] - right[0] || left[1] - right[1] || left[2] - right[2]
      })
  } catch {
    return
  }
  const shim = join(local, 'GitHubCopilotCLI', 'copilot.exe')
  const pathCli = existsSync(shim) ? shim : cliOnPath(env)
  for (let i = versions.length - 1; i >= 0; i--) {
    const root = join(local, 'copilot', 'pkg', 'win32-x64', versions[i])
    const sdk = join(root, 'copilot-sdk', 'index.js')
    if (!existsSync(sdk)) continue
    const sibling = paired(root)
    if (sibling) return sibling
    if (pathCli) return { cli: pathCli, sdk }
  }
}
// Optional read-only quota RPC in the user's existing Copilot CLI. No inference session is created.
export async function readCopilotQuota(
  appData: string
): Promise<DisplayAccount> {
  const install = findCopilotInstall(appData)
  let client: QuotaClient | undefined,
    timer: ReturnType<typeof setTimeout> | undefined
  try {
    if (!install) throw Error('Copilot CLI not found')
    await access(install.sdk)
    await access(install.cli)
    const sdk: unknown = await import(
      /* @vite-ignore */ pathToFileURL(install.sdk).href
    )
    if (!isQuotaSdk(sdk)) throw Error('Unsupported Copilot SDK')
    client = new sdk.CopilotClient({
      connection: sdk.RuntimeConnection.forStdio({ path: install.cli }),
      logLevel: 'error'
    })
    const deadline = new Promise<never>((_, reject) => {
      timer = setTimeout(() => reject(Error('Quota RPC timeout')), 25000)
    })
    await Promise.race([client.start(), deadline])
    return copilotQuota(
      await Promise.race([client.rpc.account.getQuota({}), deadline])
    )
  } catch {
    return copilotQuota({})
  } finally {
    if (timer) clearTimeout(timer)
    if (client) {
      const existing = client
      const stopper = setTimeout(() => existing.forceStop(), 3000)
      try {
        await existing.stop()
      } catch {
        existing.forceStop()
      } finally {
        clearTimeout(stopper)
      }
    }
  }
}
