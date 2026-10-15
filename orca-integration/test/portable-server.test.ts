import { test } from 'node:test'
import assert from 'node:assert/strict'
import { spawn } from 'node:child_process'
import { join } from 'node:path'
import { EventEmitter, once } from 'node:events'
import { get } from 'node:http'
import { setImmediate as nextTurn } from 'node:timers/promises'
import { BleHost } from '../src/ble-host.js'
import { QuotamonController } from '../src/quotamon-controller.js'
import { defaultConfig, publicId, record } from '../src/quota-snapshot.js'
import { startPortableServer } from '../src/portable-server.js'

test('web updates once two seconds before each acknowledged send deadline, with immediate status changes',
  { timeout: 10000 }, async (t) => {
    t.mock.timers.enable({ apis: ['Date', 'setTimeout', 'setInterval'], now: 1800000000000 })
    let current = {
      connected: true, next_snapshot_at: Date.now() + 60000,
      pairingPin: null as string | null, devices: [], config: defaultConfig(),
      accounts: [] as unknown[], error: null
    }
    const controller = Object.assign(new EventEmitter(), { state: () => current })
    const server = await startPortableServer({
      controller: () => controller as unknown as QuotamonController,
      sourceAvailable: () => true, radio: () => true,
      preferences: () => ({ copilot: false, startup: false }),
      setPreferences: async () => {}, webRoot: '', assetsRoot: ''
    })
    const url = new URL(server.url)
    let request: ReturnType<typeof get> | undefined
    try {
      assert.equal((await fetch(url.origin + '/api/events')).status, 403)
      assert.equal((await fetch(url.origin + '/api/events', {
        headers: { 'X-Quotamon-Key': url.hash.slice(1), Origin: 'http://evil.invalid' }
      })).status, 403)
      const response = await new Promise<import('node:http').IncomingMessage>((resolve) => {
        request = get(url.origin + '/api/events', {
          headers: { 'X-Quotamon-Key': url.hash.slice(1) }
        }, resolve)
      })
      let output = ''
      response.setEncoding('utf8')
      response.on('data', (chunk) => { output += chunk })
      await once(response, 'data')
      const count = () => (output.match(/data: refresh/g) || []).length
      assert.equal(count(), 1) // Initial page load.
      current.accounts = [{ quota: 'changed' }]
      controller.emit('state')
      t.mock.timers.tick(57999)
      await nextTurn()
      assert.equal(count(), 1) // No 2-second polling or quota-driven redraw.
      let update = once(response, 'data')
      t.mock.timers.tick(1)
      await update
      assert.equal(count(), 2)
      t.mock.timers.tick(2000)
      current.next_snapshot_at = Date.now() + 60000 // ACK starts the next interval.
      controller.emit('state')
      t.mock.timers.tick(57999)
      await nextTurn()
      assert.equal(count(), 2)
      update = once(response, 'data')
      t.mock.timers.tick(1)
      await update
      assert.equal(count(), 3)
      current.config = { ...current.config, revision: 1,
        display: { ...current.config.display, poll_seconds: 90 } }
      current.next_snapshot_at = Date.now() + 90000
      update = once(response, 'data')
      controller.emit('state')
      await update
      assert.equal(count(), 4) // Settings are visible immediately.
      t.mock.timers.tick(87999)
      await nextTurn()
      assert.equal(count(), 4)
      update = once(response, 'data')
      t.mock.timers.tick(1)
      await update
      assert.equal(count(), 5)
      current.pairingPin = '123456'
      update = once(response, 'data')
      controller.emit('state')
      await update
      assert.equal(count(), 6) // Pairing must not wait for a transfer.
      controller.emit('disposed')
      await once(response, 'end')
      assert.equal(controller.listenerCount('state'), 0)
    } finally {
      request?.destroy()
      await server.close()
      t.mock.timers.reset()
    }
  })

test('Windows pairing failure gives a native Bluetooth recovery instruction', async () => {
  const host = new BleHost('fixture', () => spawn(process.execPath,
    [join(__dirname, 'ble-resource-fixture.js')], {
      windowsHide: true, stdio: 'pipe',
      env: { ...process.env, BLE_FIXTURE_PAIRING_FAILURE: '1' }
    }))
  const controller = new QuotamonController(host, {
    getSnapshot: () => ({}), refreshIfStale: async () => {},
    onChanged: () => () => {}, isAvailable: () => true
  }, () => {})
  const root = join(__dirname, '..', '..', '..')
  const server = await startPortableServer({
    controller: () => controller, sourceAvailable: () => true,
    preferences: () => ({ copilot: true, startup: false }),
    setPreferences: async () => {}, radio: () => true,
    webRoot: join(root, 'portable', 'web'), assetsRoot: join(root, 'assets', 'providers')
  })
  const url = new URL(server.url)
  const headers = { 'X-Quotamon-Key': url.hash.slice(1), Origin: url.origin, 'Content-Type': 'application/json' }
  try {
    const response = await fetch(url.origin + '/api/connect', {
      method: 'POST', headers, body: JSON.stringify({ address: '12345' })
    })
    assert.equal(response.status, 200)
    let state: any
    for (let attempt = 0; attempt < 50; attempt++) {
      state = await (await fetch(url.origin + '/api/state', { headers })).json()
      if (state.ui_error) break
      await new Promise((resolve) => setTimeout(resolve, 20))
    }
    assert.equal(state.connected, false)
    assert.match(state.ui_error, /Windows Bluetooth/)
    assert.match(state.ui_error, /6자리/)
    assert.equal(state.config.address, null)
  } finally { await server.close(); controller.dispose() }
})
test(
  'portable API protects mutations and handles selection/revision/source loss',
  { timeout: 12000 },
  async () => {
    const host = new BleHost('fixture', () =>
      spawn(process.execPath, [join(__dirname, 'ble-resource-fixture.js')], {
        windowsHide: true,
        stdio: 'pipe'
      })
    )
    let available = true,
      received: unknown
    host.on('event', (e) => {
      if (record(e) && e.type === 'received') received = e.snapshot
    })
    const c = new QuotamonController(
      host,
      {
        getSnapshot: () => ({
          codex: {
            systemDefault: { hasAuth: true, email: 'secret@example.com' }
          },
          rateLimits: {
            codex: {
              status: 'ok',
              updatedAt: Date.now(),
              weekly: { usedPercent: 30 }
            }
          }
        }),
        refreshIfStale: async () => {},
        onChanged: () => () => {},
        isAvailable: () => available
      },
      () => {}
    )
    const root = join(__dirname, '..', '..', '..')
    const server = await startPortableServer({
      controller: () => c,
      sourceAvailable: () => available,
      preferences: () => ({ copilot: true, startup: false }),
      setPreferences: async () => {},
      radio: () => true,
      webRoot: join(root, 'portable', 'web'),
      assetsRoot: join(root, 'assets', 'providers')
    })
    const url = new URL(server.url),
      origin = url.origin,
      token = url.hash.slice(1)
    const headers = {
      'X-Quotamon-Key': token,
      'Content-Type': 'application/json',
      Origin: origin
    }
    const post = (route: string, value: unknown) =>
      fetch(origin + '/api/' + route, {
        method: 'POST',
        headers,
        body: JSON.stringify(value)
      })
    try {
      assert.equal((await fetch(origin + '/api/state')).status, 403)
      assert.equal(
        (
          await fetch(origin + '/api/save', {
            method: 'POST',
            headers: { ...headers, Origin: 'http://evil.invalid' },
            body: '{}'
          })
        ).status,
        403
      )
      const response = await fetch(origin + '/api/state', { headers })
      const text = await response.text()
      assert.ok(!text.includes('secret@example.com'))
      assert.equal(
        (await fetch(origin + '/assets/../private.json')).status,
        404
      )
      assert.equal((await fetch(origin + '/')).status, 200)
      assert.equal((await fetch(origin + '/assets/copilot.png')).status, 200)
      const config = defaultConfig()
      config.accounts = [
        {
          id: publicId('codex', 'system-default'),
          alias: 'Desk',
          plan: 'Plus',
          enabled: true
        }
      ]
      assert.equal((await post('save', config)).status, 200)
      assert.equal((await post('save', config)).status, 409)
      await post('connect', { address: '12345' })
      await new Promise((r) => setTimeout(r, 2100))
      assert.ok(record(received))
      assert.equal(received.mock, false)
      assert.equal(c.state().connected, true)
      available = false
      await new Promise((r) => setTimeout(r, 1100))
      assert.equal(c.state().connected, false)
      assert.equal((await post('connect', { address: '12345' })).status, 409)
      assert.equal(c.state().config.address, '12345')
    } finally {
      await server.close()
      c.dispose()
    }
  }
)
