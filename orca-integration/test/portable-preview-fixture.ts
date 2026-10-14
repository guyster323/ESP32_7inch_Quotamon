// Development preview only; not imported by the portable product bundle.
import { spawn } from 'node:child_process'
import { join } from 'node:path'
import { BleHost } from '../src/ble-host.js'
import { QuotamonController } from '../src/quotamon-controller.js'
import { copilotQuota } from '../src/quota-snapshot.js'
import { startPortableServer } from '../src/portable-server.js'
const now = Date.now()
const quota = (usedPercent: number) => ({
  status: 'ok',
  updatedAt: now,
  planType: 'Plus',
  weekly: { usedPercent, resetsAt: now + 86400000 }
})
const host = new BleHost('fixture', () =>
  spawn(process.execPath, [join(__dirname, 'ble-resource-fixture.js')], {
    windowsHide: true,
    stdio: 'pipe',
    env: { ...process.env, BLE_FIXTURE_PAIRING: '1' }
  })
)
let preferences = { copilot: true, startup: false }
let available = true
const listeners = new Set<() => void>()
const controller = new QuotamonController(
  host,
  {
    getSnapshot: () => {
      if (!available) throw Error('Orca unavailable fixture')
      return {
        codex: { systemDefault: { hasAuth: true } },
        rateLimits: {
          codex: quota(36),
          claude: quota(23),
          cursor: { ...quota(49), planType: 'Ultra' },
          grok: quota(14)
        }
      }
    },
    refreshIfStale: async () => {},
    onChanged: (listener) => {
      listeners.add(listener)
      return () => {
        listeners.delete(listener)
      }
    },
    isAvailable: () => available,
    copilot: async () =>
      copilotQuota({
        quotaSnapshots: {
          premium_interactions: {
            usedRequests: 1430,
            entitlementRequests: 6500,
            remainingPercentage: 78
          }
        }
      })
  },
  () => {}
)
async function main(): Promise<void> {
  await controller.refresh()
  const root = join(__dirname, '..', '..', '..')
  const server = await startPortableServer({
    controller: () => controller,
    sourceAvailable: () => available,
    preferences: () => preferences,
    setPreferences: async (next) => {
      preferences = next
    },
    radio: () => true,
    webRoot: join(root, 'portable', 'web'),
    assetsRoot: join(root, 'assets', 'providers')
  })
  process.stdout.write(JSON.stringify({ url: server.url }) + '\n')
  process.stdin.resume()
  process.stdin.on('data', (chunk) => {
    if (chunk.toString().trim() === 'offline') {
      available = false
      for (const listener of listeners) listener()
    }
  })
  process.stdin.on('end', () => {
    controller.dispose()
    void server.close().then(() => process.exit(0))
  })
}
void main()
