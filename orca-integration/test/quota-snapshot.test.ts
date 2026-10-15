import { test } from 'node:test'
import assert from 'node:assert/strict'
import {
  fromOrca,
  defaultConfig,
  makeSnapshot,
  publicId,
  validateConfig,
  copilotQuota,
  resetText
} from '../src/quota-snapshot.js'
const now = 1800000000
test('reset durations use compact units and retain unknown or elapsed states', () => {
  assert.equal(resetText(now + (5 * 24 + 3) * 3600, now), '5d 3h')
  assert.equal(resetText(now + (2 * 60 + 41) * 60, now), '2h 41m')
  assert.equal(resetText(now + 41 * 60, now), '41m')
  assert.equal(resetText(null, now), '시각 미확인')
  assert.equal(resetText(now, now), '리셋 후 확인 대기')
})

test('display refresh time advances while provider collection time is unchanged', () => {
  const accounts = fromOrca({ rateLimits: { grok: {
    status: 'ok', updatedAt: now, weekly: { usedPercent: 20 }
  } } }, now)
  const config = defaultConfig()
  config.accounts = [{ id: accounts[0].id, alias: '', plan: '', enabled: true }]
  const first = makeSnapshot(accounts, config, now)
  const next = makeSnapshot(accounts, config, now + 60)
  assert.notEqual(first.refreshed, next.refreshed)
  assert.equal(next.refreshed, new Date((now + 60) * 1000).toLocaleTimeString('ko-KR', { hour12: false }))
  assert.equal(first.accounts[0].updated, next.accounts[0].updated)
  assert.equal(first.accounts[0].updated_at, next.accounts[0].updated_at)
})

test('unavailable provider placeholders are not signed-in accounts', () => {
  const raw = {
    codex: { systemDefault: { hasAuth: true }, accounts: [] },
    claude: { accounts: [], activeAccountId: null },
    rateLimits: {
      codex: { status: 'unavailable', session: null, weekly: null },
      claude: { status: 'unavailable', session: null, weekly: null }
    }
  }
  const accounts = fromOrca(raw, now)
  assert.equal(accounts.length, 1)
  assert.equal(accounts[0].provider, 'codex')
  assert.equal(accounts[0].state, 'unknown')
  // A configured account stays selectable while its usage source is unavailable.
  const managed = fromOrca(
    {
      ...raw,
      claude: { accounts: [{ id: 'signed-in-claude' }] }
    },
    now
  )
  assert.equal(managed.filter((a) => a.provider === 'claude').length, 1)
})
test('inactive accounts, privacy allowlist, unknown/unlimited and source expiry', () => {
  const raw = {
    authToken: 'SECRET',
    codex: {
      systemDefault: { hasAuth: true, email: 'private@example.com' },
      accounts: [
        {
          id: 'inactive-secret',
          label: 'private@example.com',
          auth: { refresh_token: 'HIDDEN' }
        }
      ]
    },
    rateLimits: {
      codex: {
        status: 'ok',
        updatedAt: now,
        session: { usedPercent: 10, resetsAt: now + 3600 }
      },
      inactiveCodexAccounts: [
        {
          accountId: 'inactive-secret',
          rateLimits: {
            status: 'ok',
            updatedAt: now - 1900,
            weekly: { usedPercent: 70 }
          }
        }
      ],
      cursor: {
        status: 'ok', updatedAt: now,
        buckets: [{ name: 'Cursor Models', isUnlimited: true }]
      },
      grok: { status: 'unavailable' }
    }
  }
  const accounts = fromOrca(raw, now)
  assert.equal(accounts.length, 3)
  assert.equal(accounts[0].pools[0].remaining, 90)
  assert.equal(accounts[1].state, 'stale')
  assert.equal(accounts[2].pools[0].remaining, -2)
  const config = defaultConfig()
  config.accounts = [
    {
      id: accounts[0].id,
      alias: 'Business Premium',
      plan: 'Business',
      enabled: true
    }
  ]
  const snapshot = makeSnapshot(accounts, config, now)
  const json = JSON.stringify(snapshot)
  for (const secret of [
    'SECRET',
    'HIDDEN',
    'private@example.com',
    'inactive-secret',
    'refresh_token',
    'authToken'
  ])
    assert.ok(!json.includes(secret))
  assert.equal(snapshot.accounts[0].enabled, true)
  assert.equal(snapshot.accounts[1].enabled, false)
  assert.equal(snapshot.accounts[1].pools[0].remaining, -1)
  assert.equal(snapshot.accounts[1].updated_at, null)
  assert.equal(
    makeSnapshot(accounts, config, now + 3601).accounts[0].state,
    'stale'
  )
  assert.equal(
    fromOrca({ codex: { systemDefault: { hasAuth: true } } }, now)[0].pools[0]
      .remaining,
    -1
  )
})
test('Cursor displays only model budgets in fixed order and ignores aggregate expiry', () => {
  const accounts = fromOrca({ rateLimits: { cursor: {
    status: 'ok', updatedAt: now,
    session: { usedPercent: 100 }, weekly: { usedPercent: 100 },
    monthly: { usedPercent: 100, resetsAt: now - 1 },
    buckets: [
      { name: 'Extra', usedPercent: 100 },
      { name: 'Other Models', usedPercent: 53, resetsAt: now + 7200 },
      { name: 'Cursor Models', usedPercent: 18, resetsAt: now + 3600 }
    ]
  } } }, now)
  const account = accounts[0]
  assert.equal(account.state, 'ready')
  assert.deepEqual(account.pools.map((p) => [p.name, p.remaining, p.resets_at]), [
    ['Cursor Models', 82, now + 3600],
    ['Other Models', 47, now + 7200]
  ])
  const config = defaultConfig()
  config.accounts = [{ id: account.id, alias: '', plan: '', enabled: true }]
  assert.deepEqual(makeSnapshot(accounts, config, now).accounts[0].pools, account.pools)
  assert.equal(makeSnapshot(accounts, config, now + 3601).accounts[0].state, 'stale')
})

test('missing Cursor model budgets remain unknown instead of using Monthly', () => {
  for (const buckets of [undefined, [{ name: 'Other Models', usedPercent: 100 }]]) {
    const [account] = fromOrca({ rateLimits: { cursor: {
      status: 'ok', updatedAt: now, monthly: { isUnlimited: true }, buckets
    } } }, now)
    assert.deepEqual(account.pools.map((p) => [p.name, p.remaining]), [
      ['Cursor Models', -1], ['Other Models', buckets ? 0 : -1]
    ])
    assert.equal(account.state, buckets ? 'ready' : 'unknown')
  }
  const [grok] = fromOrca({ rateLimits: { grok: {
    status: 'ok', updatedAt: now, monthly: { usedPercent: 20 }
  } } }, now)
  assert.equal(grok.pools[0].name, 'MONTHLY')
  assert.equal(grok.pools[0].remaining, 80)
})

test('settings preserve IDs/order, reject invalid selections and do not assume 6500 for every Copilot plan', () => {
  const config = defaultConfig()
  config.accounts = [
    { id: publicId('grok', 'active'), alias: 'Grok', plan: '', enabled: true }
  ]
  assert.deepEqual(validateConfig(config), config)
  assert.throws(() =>
    validateConfig({
      ...config,
      accounts: [...config.accounts, ...config.accounts]
    })
  )
  assert.throws(() =>
    validateConfig({
      ...config,
      display: { ...config.display, poll_seconds: 0 }
    })
  )
  const cp = copilotQuota(
    {
      quotaSnapshots: {
        premium_interactions: {
          entitlementRequests: 6500,
          usedRequests: 0,
          remainingPercentage: 100
        }
      }
    },
    now
  )
  assert.equal(cp.pools[0].remaining, 100)
  assert.equal(cp.has_amounts, false)
  assert.ok(cp.source_note.includes('단위 확인'))
  assert.equal(copilotQuota({}, now).pools[0].remaining, -1)
  assert.equal(
    copilotQuota(
      { quotaSnapshots: { premium_interactions: { entitlementRequests: -1 } } },
      now
    ).pools[0].remaining,
    -2
  )
  const tiny = fromOrca(
    {
      codex: { systemDefault: { hasAuth: true } },
      rateLimits: {
        codex: {
          status: 'ok',
          updatedAt: now,
          session: { usedPercent: 99.6, windowMinutes: 300 }
        }
      }
    },
    now
  )
  assert.ok(tiny[0].pools[0].remaining > 0 && tiny[0].pools[0].remaining < 1)
})
