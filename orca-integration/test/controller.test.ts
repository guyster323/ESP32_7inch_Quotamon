import { test } from 'node:test'
import assert from 'node:assert/strict'
import { spawn } from 'node:child_process'
import { join } from 'node:path'
import { BleHost } from '../src/ble-host.js'
import { fragment } from '../src/ble-protocol.js'
import { QuotamonController } from '../src/quotamon-controller.js'
import { defaultConfig, publicId, record } from '../src/quota-snapshot.js'
const fixture = () =>
  spawn(process.execPath, [join(__dirname, 'ble-resource-fixture.js')], {
    windowsHide: true,
    stdio: 'pipe',
    env: { SystemRoot: process.env.SystemRoot }
  })

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
      receivedCount = 0,
      received: unknown
    let sourceChanged = () => {}
    host.on('event', (event) => {
      if (record(event) && event.type === 'received') {
        received = event.snapshot
        receivedCount++
      }
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
        onChanged: (listener) => { sourceChanged = listener; return () => {} }
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
      assert.ok(c.state().next_snapshot_at! > Date.now() + 57000)
      const previousDeadline = c.state().next_snapshot_at!
      sourceChanged()
      await new Promise((r) => setTimeout(r, 1100))
      assert.equal(receivedCount, 1, 'cache changes must wait for the configured send interval')
      assert.equal(c.state().next_snapshot_at, previousDeadline)
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
      assert.equal(c.state().next_snapshot_at, previousDeadline + 30000)
      assert.throws(() => c.save(config))
      assert.ok(persisted >= 2)
      await c.disconnect(true)
      assert.equal(c.state().config.address, null)
      assert.equal(c.state().connected, false)
      assert.equal(c.state().next_snapshot_at, null)
    } finally {
      c.dispose()
    }
  }
)
test('source loss clears remembered default quota', async () => {
  const host = new BleHost('fixture', fixture)
  const collected = Math.floor(Date.now() / 1000)
  const email = 'default.member@example.com'
  let available = true
  let hostId: string | null = null
  let listener = () => {}
  const source = (liveUsed: number) => ({
    codex: {
      systemDefault: {
        hasAuth: true,
        authKind: 'oauth',
        email,
        providerAccountId: 'org-same'
      },
      accounts: [
        {
          id: 'team',
          workspaceLabel: 'Team',
          email: 'team.member@example.com',
          providerAccountId: 'org-same'
        }
      ],
      activeAccountId: hostId,
      activeAccountIdsByRuntime: { host: hostId, wsl: {} }
    },
    rateLimits: {
      codexTarget: { runtime: 'host', wslDistro: null },
      codex: {
        provider: 'codex',
        status: 'ok',
        updatedAt: collected,
        session: { usedPercent: liveUsed, resetsAt: (collected + 3600) * 1000 }
      },
      inactiveCodexAccounts: []
    }
  })
  let snap = source(11)
  const controller = new QuotamonController(
    host,
    {
      getSnapshot: () => snap,
      isAvailable: () => available,
      refreshIfStale: async () => {},
      onChanged: (fn) => {
        listener = fn
        return () => {}
      }
    },
    () => {}
  )
  try {
    const id = publicId('codex', 'system-default')
    assert.equal(controller.state().accounts.find((a) => a.id === id)?.pools[0].remaining, 89)
    hostId = 'team'
    snap = source(15)
    listener()
    const stale = controller.state().accounts.find((a) => a.id === id)
    assert.equal(stale?.state, 'stale')
    assert.equal(stale?.updated_at, collected)
    assert.equal(stale?.pools[0].remaining, 89)
    available = false
    listener()
    assert.equal(controller.state().accounts.length, 0)
    available = true
    listener()
    const cleared = controller.state().accounts.find((a) => a.id === id)
    assert.equal(cleared?.pools[0].remaining, -1)
    assert.equal(JSON.stringify(controller.state()).includes(email), false)
  } finally {
    controller.dispose()
  }
})

test('an offline device settings notification leaves saved accounts unchanged', async () => {
  const host = new BleHost('fixture', fixture)
  const home = publicId('codex', 'system-default')
  const team = publicId('codex', 'team')
  const company = publicId('copilot', 'active')
  const config = defaultConfig()
  config.accounts = [
    { id: home, alias: '기본', plan: 'Business Premium', enabled: true },
    { id: team, alias: '팀', plan: 'Business standard', enabled: true },
    { id: company, alias: '회사', plan: '', enabled: false }
  ]
  let available = true
  let listener = () => {}
  let persisted = 0
  const controller = new QuotamonController(
    host,
    {
      getSnapshot: () => ({
        codex: { systemDefault: { hasAuth: true }, accounts: [] }
      }),
      isAvailable: () => available,
      refreshIfStale: async () => {},
      onChanged: (fn) => {
        listener = fn
        return () => {}
      }
    },
    () => {
      persisted++
    },
    config
  )
  try {
    available = false
    listener()
    assert.equal(controller.state().accounts.length, 0)
    const body = Buffer.from(
      JSON.stringify({
        type: 'settings',
        base_revision: controller.state().config.revision,
        display: { ...controller.state().config.display, poll_seconds: 120 },
        accounts: []
      })
    )
    for (const frame of fragment(2, 1, body))
      host.emit('event', {
        type: 'notification',
        frame: Buffer.from(frame).toString('base64')
      })
    assert.equal(persisted, 0)
    assert.equal(controller.state().config.revision, 0)
    assert.equal(controller.state().config.display.poll_seconds, 60)
    assert.equal(controller.state().error, 'Orca 사용량 정보 대기 중')
    assert.deepEqual(
      controller.state().config.accounts.map((row) => [row.id, row.alias, row.plan, row.enabled]),
      [
        [home, '기본', 'Business Premium', true],
        [team, '팀', 'Business standard', true],
        [company, '회사', '', false]
      ]
    )
  } finally {
    controller.dispose()
  }
})
