import { test } from 'node:test'
import assert from 'node:assert/strict'
import {
  fromOrca,
  defaultConfig,
  makeSnapshot,
  publicId,
  validateConfig,
  copilotQuota
} from '../src/quota-snapshot.js'
const now = 1800000000
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
      cursor: { status: 'ok', updatedAt: now, monthly: { isUnlimited: true } },
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
