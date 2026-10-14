import { access } from 'node:fs/promises'
import { join } from 'node:path'
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
function isQuotaSdk(value: unknown): value is QuotaSdk {
  return (
    record(value) &&
    typeof value.CopilotClient === 'function' &&
    record(value.RuntimeConnection) &&
    typeof value.RuntimeConnection.forStdio === 'function'
  )
}
// Optional read-only quota RPC in the user's existing Copilot CLI. No inference session is created.
export async function readCopilotQuota(
  appData: string
): Promise<DisplayAccount> {
  const root = join(
    appData,
    'npm',
    'node_modules',
    '@github',
    'copilot',
    'node_modules',
    '@github',
    'copilot-win32-x64'
  )
  const modulePath = join(root, 'copilot-sdk', 'index.js'),
    cli = join(root, 'copilot.exe')
  let client: QuotaClient | undefined,
    timer: ReturnType<typeof setTimeout> | undefined
  try {
    await access(modulePath)
    await access(cli)
    const sdk: unknown = await import(
      /* @vite-ignore */ pathToFileURL(modulePath).href
    )
    if (!isQuotaSdk(sdk)) throw Error('Unsupported Copilot SDK')
    client = new sdk.CopilotClient({
      connection: sdk.RuntimeConnection.forStdio({ path: cli }),
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
