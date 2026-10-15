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

function codexSwitchSnapshot(selection: {
  host: string | null
  wsl?: Record<string, string | null>
  target?: { runtime: 'host' | 'wsl'; wslDistro: string | null } | null
  liveUsed?: number
  omitHost?: boolean
}): unknown {
  const liveUsed = selection.liveUsed ?? 73
  return {
    codex: {
      systemDefault: {
        hasAuth: true,
        authKind: 'oauth',
        email: 'default.member@example.com',
        providerAccountId: 'org-same',
        workspaceLabel: 'Business'
      },
      accounts: [
        {
          id: 'personal',
          workspaceLabel: 'Personal',
          email: 'personal.member@example.com',
          providerAccountId: 'pid-personal'
        },
        {
          id: 'team',
          workspaceLabel: 'Team',
          email: 'team.member@example.com',
          providerAccountId: 'org-same'
        }
      ],
      activeAccountId: 'team',
      activeAccountIdsByRuntime: {
        ...(selection.omitHost ? {} : { host: selection.host }),
        wsl: selection.wsl ?? {}
      }
    },
    rateLimits: {
      ...(selection.target === undefined
        ? { codexTarget: { runtime: 'host', wslDistro: null } }
        : selection.target === null
          ? { codexTarget: null }
          : { codexTarget: selection.target }),
      codex: {
        provider: 'codex',
        status: 'ok',
        updatedAt: now,
        session: { usedPercent: liveUsed, resetsAt: now + 3600 },
        weekly: { usedPercent: 10, resetsAt: now + 86400 }
      },
      inactiveCodexAccounts: [
        {
          accountId: 'personal',
          rateLimits: {
            provider: 'codex',
            status: 'ok',
            updatedAt: now,
            session: { usedPercent: 22, resetsAt: now + 3600 },
            weekly: { usedPercent: 5, resetsAt: now + 86400 }
          }
        }
      ]
    }
  }
}

test('host runtime selection assigns live Codex quota to Team and not the default slot', () => {
  const accounts = fromOrca(
    codexSwitchSnapshot({ host: 'team', liveUsed: 73 }),
    now
  )
  const byId = new Map(accounts.map((a) => [a.id, a]))
  const fallback = byId.get(publicId('codex', 'system-default'))
  const team = byId.get(publicId('codex', 'team'))
  const personal = byId.get(publicId('codex', 'personal'))
  assert.ok(fallback)
  assert.ok(team)
  assert.equal(team.pools[0].remaining, 27)
  assert.equal(team.pools[1].remaining, 90)
  assert.equal(fallback.pools[0].remaining, -1)
  assert.equal(personal?.pools[0].remaining, 78)
  assert.equal(fallback.id, publicId('codex', 'system-default'))
  const json = JSON.stringify(accounts)
  for (const secret of [
    'default.member@example.com',
    'team.member@example.com',
    'personal.member@example.com',
    'org-same',
    'pid-personal'
  ])
    assert.equal(json.includes(secret), false)
})

test('explicit null host selection wins over a stale activeAccountId', () => {
  const accounts = fromOrca(
    codexSwitchSnapshot({ host: null, liveUsed: 73 }),
    now
  )
  const byId = new Map(accounts.map((a) => [a.id, a]))
  const fallback = byId.get(publicId('codex', 'system-default'))
  const team = byId.get(publicId('codex', 'team'))
  assert.equal(fallback?.pools[0].remaining, 27)
  assert.notEqual(team?.pools[0].remaining, 27)
})

test('named WSL selection owns the live quota and the host default does not', () => {
  const accounts = fromOrca(
    codexSwitchSnapshot({
      host: null,
      wsl: { Ubuntu: 'personal' },
      target: { runtime: 'wsl', wslDistro: 'Ubuntu' },
      liveUsed: 40
    }),
    now
  )
  const byId = new Map(accounts.map((a) => [a.id, a]))
  assert.equal(byId.get(publicId('codex', 'personal'))?.pools[0].remaining, 60)
  assert.equal(byId.get(publicId('codex', 'system-default'))?.pools[0].remaining, -1)
  assert.notEqual(byId.get(publicId('codex', 'team'))?.pools[0].remaining, 60)
})

test('a modern target with no owner stays unowned even when every managed row is inactive', () => {
  const accounts = fromOrca(
    {
      codex: {
        systemDefault: { hasAuth: true },
        accounts: [{ id: 'team', workspaceLabel: 'Team' }]
      },
      rateLimits: {
        codexTarget: { runtime: 'host', wslDistro: null },
        codex: {
          provider: 'codex',
          status: 'ok',
          updatedAt: now,
          session: { usedPercent: 73, resetsAt: now + 3600 }
        },
        inactiveCodexAccounts: [
          {
            accountId: 'team',
            rateLimits: {
              provider: 'codex',
              status: 'ok',
              updatedAt: now,
              session: { usedPercent: 22, resetsAt: now + 3600 }
            }
          }
        ]
      }
    },
    now
  )
  const byId = new Map(accounts.map((a) => [a.id, a]))
  assert.equal(byId.get(publicId('codex', 'system-default'))?.pools[0].remaining, -1)
  assert.equal(byId.get(publicId('codex', 'team'))?.pools[0].remaining, 78)
})

test('inactive quota from another provider and a WSL account selected on the host are rejected', () => {
  const accounts = fromOrca(
    {
      codex: {
        systemDefault: { hasAuth: true },
        accounts: [
          {
            id: 'team',
            workspaceLabel: 'Team',
            managedHomeRuntime: 'wsl',
            wslDistro: 'Ubuntu'
          }
        ],
        activeAccountIdsByRuntime: { host: 'team', wsl: {} }
      },
      rateLimits: {
        codexTarget: { runtime: 'host', wslDistro: null },
        codex: {
          provider: 'codex',
          status: 'ok',
          updatedAt: now,
          session: { usedPercent: 73, resetsAt: now + 3600 }
        },
        inactiveCodexAccounts: [
          {
            accountId: 'team',
            rateLimits: {
              provider: 'claude',
              status: 'ok',
              updatedAt: now,
              session: { usedPercent: 22, resetsAt: now + 3600 }
            }
          }
        ]
      }
    },
    now
  )
  const byId = new Map(accounts.map((a) => [a.id, a]))
  assert.equal(byId.get(publicId('codex', 'system-default'))?.pools[0].remaining, -1)
  assert.equal(byId.get(publicId('codex', 'team'))?.pools[0].remaining, -1)
})

test('an unnamed WSL target uses the single selected distro and ignores an explicit null default', () => {
  const unique = fromOrca(
    codexSwitchSnapshot({
      host: null,
      wsl: { Ubuntu: 'personal' },
      target: { runtime: 'wsl', wslDistro: null },
      liveUsed: 40
    }),
    now
  )
  const blocked = fromOrca(
    codexSwitchSnapshot({
      host: null,
      wsl: { __default__: null, Ubuntu: 'personal' },
      target: { runtime: 'wsl', wslDistro: null },
      liveUsed: 40
    }),
    now
  )
  const uniqueIds = new Map(unique.map((a) => [a.id, a]))
  const blockedIds = new Map(blocked.map((a) => [a.id, a]))
  assert.equal(uniqueIds.get(publicId('codex', 'personal'))?.pools[0].remaining, 60)
  assert.equal(
    uniqueIds.get(publicId('codex', 'system-default'))?.pools[0].remaining,
    -1
  )
  assert.equal(blockedIds.get(publicId('codex', 'personal'))?.pools[0].remaining, 78)
  assert.equal(
    blockedIds.get(publicId('codex', 'system-default'))?.pools[0].remaining,
    -1
  )
})

test('configured slots keep order and plans when source rows reverse or disappear', () => {
  const personal = publicId('codex', 'personal')
  const team = publicId('codex', 'team')
  const copilot = publicId('copilot', 'active')
  const source = (order: string[]) =>
    fromOrca(
      {
        codex: {
          systemDefault: { hasAuth: true },
          accounts: order.map((id) => ({ id }))
        },
        rateLimits: {
          codexTarget: { runtime: 'host' },
          activeAccountIdsByRuntime: { host: 'team' },
          codex: {
            status: 'ok',
            updatedAt: now,
            planType: 'business',
            session: { usedPercent: 10, resetsAt: now + 3600 }
          },
          inactiveCodexAccounts: [
            {
              accountId: 'personal',
              rateLimits: {
                status: 'ok',
                updatedAt: now,
                planType: 'plus',
                session: { usedPercent: 30, resetsAt: now + 3600 }
              }
            }
          ]
        }
      },
      now
    )
  const forward = source(['personal', 'team'])
  const reverse = source(['team', 'personal'])
  assert.equal(forward.find((a) => a.id === personal)?.profile, 'Codex / 관리 계정')
  assert.equal(reverse.find((a) => a.id === personal)?.profile, 'Codex / 관리 계정')
  assert.deepEqual(
    forward.filter((a) => a.id === personal || a.id === team).map((a) => a.id),
    [personal, team]
  )
  assert.deepEqual(
    reverse.filter((a) => a.id === personal || a.id === team).map((a) => a.id),
    [team, personal]
  )
  const config = validateConfig({
    ...defaultConfig(),
    accounts: [
      { id: personal, alias: '개인', plan: '요금제 미확인', enabled: true },
      { id: copilot, alias: '회사', plan: '', enabled: true },
      { id: team, alias: '팀', plan: 'Business standard', enabled: true }
    ]
  })
  assert.equal(config.accounts[0].plan, '')
  assert.equal(config.accounts[2].plan, 'Business standard')
  const missing = makeSnapshot(reverse, config, now)
  assert.deepEqual(
    missing.accounts.slice(0, 3).map((a) => [a.id, a.profile, a.plan]),
    [
      [personal, '개인', 'plus'],
      [copilot, '회사', '요금제 미확인'],
      [team, '팀', 'Business standard']
    ]
  )
  assert.equal(missing.accounts[1].state, 'unknown')
  assert.equal(missing.accounts[1].pools[0].remaining, -1)
  const recovered = makeSnapshot(
    [
      ...reverse,
      copilotQuota(
        {
          quotaSnapshots: {
            premium_interactions: {
              usedRequests: 1,
              entitlementRequests: 10,
              remainingPercentage: 90
            }
          }
        },
        now
      )
    ],
    config,
    now
  )
  assert.equal(recovered.accounts[1].state, 'ready')
  assert.equal(recovered.accounts[1].pools[0].remaining, 90)
  assert.deepEqual(
    recovered.accounts.map((a) => a.id),
    missing.accounts.map((a) => a.id)
  )
})
