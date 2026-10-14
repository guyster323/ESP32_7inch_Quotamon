import { app, ipcMain } from 'electron'
import {
  existsSync,
  mkdirSync,
  readFileSync,
  renameSync,
  writeFileSync
} from 'node:fs'
import { join } from 'node:path'
import { spawnProcess } from '@orca/process-host'
import { mainProcessState as state } from '../startup/main-process-state'
import { isTrustedUIRenderer } from '../ipc/ui'
import { BleHost } from './ble-host'
import { QuotamonController } from './quotamon-controller'
import { defaultConfig, validateConfig, Config } from './quota-snapshot'
import { readCopilotQuota } from './copilot-quota-source'
let controller: QuotamonController | undefined
let initializing: Promise<QuotamonController> | undefined
function configPath(): string {
  return join(app.getPath('userData'), 'quotamon-settings.json')
}
function readConfig(): Config {
  try {
    return validateConfig(JSON.parse(readFileSync(configPath(), 'utf8')))
  } catch {
    return defaultConfig()
  }
}
function persist(config: Config): void {
  const path = configPath()
  mkdirSync(app.getPath('userData'), { recursive: true })
  writeFileSync(path + '.tmp', JSON.stringify(config, null, 2), { mode: 0o600 })
  renameSync(path + '.tmp', path)
}
async function getController(): Promise<QuotamonController> {
  if (controller) return controller
  if (initializing) return initializing
  initializing = (async () => {
    await app.whenReady()
    await state.firstWindowStartupServicesReady
    if (process.platform !== 'win32')
      throw Error('Quotamon BLE currently supports Windows')
    const runtime = state.runtime
    if (!runtime) throw Error('Orca accounts are not ready')
    const resource = app.isPackaged
      ? join(process.resourcesPath, 'bin', 'Quotamon.Ble.exe')
      : join(app.getAppPath(), 'resources', 'quotamon', 'Quotamon.Ble.exe')
    if (!existsSync(resource))
      throw Error('Build the bundled Quotamon BLE resource first')
    const host = new BleHost(resource, (path) =>
      spawnProcess({
        program: path,
        stdio: 'pipe',
        env: {
          SystemRoot: process.env.SystemRoot,
          windir: process.env.windir,
          TEMP: process.env.TEMP,
          TMP: process.env.TMP
        }
      })
    )
    controller = new QuotamonController(
      host,
      {
        getSnapshot: () => runtime.getAccountsSnapshot(),
        refreshIfStale: () => runtime.refreshAccountsForMobileSubscriber(),
        onChanged: (listener) => runtime.onAccountsChanged(listener),
        copilot: () => readCopilotQuota(app.getPath('appData'))
      },
      persist,
      readConfig()
    )
    void controller.refresh().catch(() => {})
    return controller
  })()
  try {
    return await initializing
  } finally {
    initializing = undefined
  }
}
// Called after Orca startup has finished; no new window, activation, or separate tray application.
export function installQuotamonIntegration(): void {
  const methods = [
    'state',
    'scan',
    'connect',
    'confirm',
    'save',
    'disconnect',
    'refresh'
  ] as const
  for (const method of methods)
    ipcMain.handle(`quotamon:${method}`, async (event, ...args: unknown[]) => {
      if (
        !isTrustedUIRenderer(event.sender) ||
        event.senderFrame !== event.sender.mainFrame
      )
        throw Error('Untrusted Quotamon request')
      const c = await getController()
      switch (method) {
        case 'state':
          return c.state()
        case 'scan':
          return c.scan()
        case 'refresh':
          return c.refresh()
        case 'save':
          return c.save(args[0])
        case 'connect':
          if (typeof args[0] !== 'string') throw Error('Invalid device')
          return c.connect(args[0])
        case 'confirm':
          if (typeof args[0] !== 'string' || typeof args[1] !== 'boolean')
            throw Error('Invalid confirmation')
          return c.confirm(args[0], args[1])
        case 'disconnect':
          if (typeof args[0] !== 'boolean') throw Error('Invalid disconnect')
          return c.disconnect(args[0])
      }
    })
  if (process.platform === 'win32' && readConfig().address)
    void getController().catch(() => {})
  app.once('will-quit', () => controller?.dispose())
}
