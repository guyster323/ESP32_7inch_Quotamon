import { test } from 'node:test'
import assert from 'node:assert/strict'
import { spawn } from 'node:child_process'
import { join } from 'node:path'
import { BleHost } from '../src/ble-host.js'
import { QuotamonController } from '../src/quotamon-controller.js'
import { defaultConfig, publicId, record } from '../src/quota-snapshot.js'
import { startPortableServer } from '../src/portable-server.js'
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
