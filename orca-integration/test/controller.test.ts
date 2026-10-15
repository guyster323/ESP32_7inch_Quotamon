import { test } from 'node:test'
import assert from 'node:assert/strict'
import { spawn } from 'node:child_process'
import { join } from 'node:path'
import { BleHost } from '../src/ble-host.js'
import { QuotamonController } from '../src/quotamon-controller.js'
import { defaultConfig, publicId, record } from '../src/quota-snapshot.js'

test('a saved Windows pairing is selectable without a radio signal reading', async () => {
  const host = new BleHost('fixture', () => spawn(process.execPath,
    [join(__dirname, 'ble-resource-fixture.js')], {
      windowsHide: true, stdio: 'pipe',
      env: { ...process.env, BLE_FIXTURE_PAIRED: '1' }
    }))
  let savedAddress: string | null = null
  const controller = new QuotamonController(host, {
    getSnapshot: () => ({}), refreshIfStale: async () => {},
    onChanged: () => () => {}
  }, (config) => { savedAddress = config.address })
  try {
    await controller.scan()
    const devices = controller.state().devices
    assert.equal(devices.length, 1)
    assert.equal(devices[0].paired, true)
    assert.equal(devices[0].rssi, 0)
    await controller.connect(devices[0].address)
    assert.equal(controller.state().connected, true)
    assert.equal(savedAddress, devices[0].address)
  } finally { controller.dispose() }
})
test(
  'Orca lifecycle, stored selection, immediate ACK, reconnect and revision conflict',
  { timeout: 8000 },
  async () => {
    const host = new BleHost('fixture', () =>
      spawn(process.execPath, [join(__dirname, 'ble-resource-fixture.js')], {
        windowsHide: true,
        stdio: 'pipe',
        env: { SystemRoot: process.env.SystemRoot }
      })
    )
    const config = defaultConfig()
    config.accounts = [
      {
        id: publicId('codex', 'system-default'),
        alias: 'Business Premium',
        plan: 'Business',
        enabled: true
      }
    ]
    let refreshes = 0,
      persisted = 0,
      received: unknown
    host.on('event', (event) => {
      if (record(event) && event.type === 'received') received = event.snapshot
    })
    const c = new QuotamonController(
      host,
      {
        getSnapshot: () => ({
          codex: { systemDefault: { hasAuth: true } },
          rateLimits: {
            codex: {
              status: 'ok',
              updatedAt: Date.now(),
              session: { usedPercent: 23, resetsAt: Date.now() + 3600000 }
            }
          }
        }),
        refreshIfStale: async () => {
          refreshes++
        },
        onChanged: () => () => {}
      },
      () => {
        persisted++
      },
      config
    )
    try {
      await c.connect('12345')
      assert.equal(c.state().connected, true)
      await new Promise((r) => setTimeout(r, 1800))
      assert.ok(record(received))
      assert.equal(received.protocol, 'quotamon.v1')
      const rows = received.accounts
      assert.ok(Array.isArray(rows))
      assert.equal(rows[0].enabled, true)
      assert.ok(refreshes <= 1)
      c.save({
        ...c.state().config,
        display: { ...config.display, poll_seconds: 90 }
      })
      assert.equal(c.state().config.revision, 1)
      assert.throws(() => c.save(config))
      assert.ok(persisted >= 2)
      await c.disconnect(true)
      assert.equal(c.state().config.address, null)
      assert.equal(c.state().connected, false)
    } finally {
      c.dispose()
    }
  }
)
