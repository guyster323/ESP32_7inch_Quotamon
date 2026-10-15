import { test } from 'node:test'
import assert from 'node:assert/strict'
import { OrcaAccountProjector } from '../src/orca-account-projector.js'
import { publicId } from '../src/quota-snapshot.js'

const now = 1800000000
const fallback = publicId('codex', 'system-default')
const teamId = publicId('codex', 'team')

function quota(
  sessionUsed: number,
  weeklyUsed: number,
  updatedAt: number,
  planType?: string
) {
  return {
    provider: 'codex',
    status: 'ok',
    updatedAt,
    ...(planType ? { planType } : {}),
    session: { usedPercent: sessionUsed, resetsAt: now + 3600 },
    weekly: { usedPercent: weeklyUsed, resetsAt: now + 86400 }
  }
}
function snapshot(
  host: string | null,
  email: string,
  liveUsed: number,
  updatedAt: number,
  inactive: boolean,
  workspaceAccountId?: string,
  planType?: string
) {
  return {
    codex: {
      systemDefault: {
        hasAuth: true,
        authKind: 'oauth',
        email,
        providerAccountId: 'org-same',
        ...(workspaceAccountId ? { workspaceAccountId } : {}),
        workspaceLabel: 'Business'
      },
      accounts: inactive || host === 'team'
        ? [
            {
              id: 'team',
              workspaceLabel: 'Team',
              email: 'team.member@example.com',
              providerAccountId: 'org-same'
            }
          ]
        : [],
      activeAccountId: host,
      activeAccountIdsByRuntime: { host, wsl: {} }
    },
    rateLimits: {
      codexTarget: { runtime: 'host', wslDistro: null },
      codex: quota(liveUsed, host === 'team' ? 25 : 21, updatedAt, planType),
      inactiveCodexAccounts:
        host === 'team'
          ? []
          : [
              {
                accountId: 'team',
                rateLimits: quota(44, 54, updatedAt)
              }
            ]
    }
  }
}

test('default quota stays on the default card as stale data after Team becomes active', () => {
  const projector = new OrcaAccountProjector()
  const first = projector.project(
    snapshot(null, 'default.member@example.com', 11, now, true, undefined, 'plus'),
    now
  )
  const owned = first.find((account) => account.id === fallback)
  assert.equal(owned?.pools[0].remaining, 89)
  assert.equal(owned?.pools[1].remaining, 79)
  const second = projector.project(
    snapshot('team', 'default.member@example.com', 15, now + 50, true),
    now + 50
  )
  const stale = second.find((account) => account.id === fallback)
  const team = second.find((account) => account.id === teamId)
  assert.equal(stale?.state, 'stale')
  assert.equal(stale?.plan, 'plus')
  assert.equal(stale?.source_note, '마지막 확인값')
  assert.equal(stale?.updated_at, now)
  assert.equal(stale?.pools[0].remaining, 89)
  assert.equal(stale?.pools[1].remaining, 79)
  assert.equal(team?.pools[0].remaining, 85)
  assert.notEqual(team?.pools[0].remaining, stale?.pools[0].remaining)
  const json = JSON.stringify(second)
  for (const secret of [
    'default.member@example.com',
    'team.member@example.com',
    'org-same',
    'providerAccountId'
  ])
    assert.equal(json.includes(secret), false)
  assert.equal(JSON.stringify(projector).includes('default.member@example.com'), false)
})

test('same workspace and a different member invalidates the default cache', () => {
  const projector = new OrcaAccountProjector()
  projector.project(snapshot(null, 'default.member@example.com', 11, now, true), now)
  const changed = projector.project(
    snapshot('team', 'other.member@example.com', 15, now + 50, true),
    now + 50
  )
  const card = changed.find((account) => account.id === fallback)
  assert.equal(card?.id, fallback)
  assert.equal(card?.pools[0].remaining, -1)
  assert.equal(card?.updated_at, null)
  assert.equal(card?.source_note, '기본 계정 변경됨')
  assert.equal(changed.find((account) => account.id === teamId)?.pools[0].remaining, 85)
})

test('clearing the projector and a disappeared managed row forget remembered quotas', () => {
  const projector = new OrcaAccountProjector()
  projector.project(snapshot(null, 'default.member@example.com', 11, now, true), now)
  const removed = projector.project(
    snapshot(null, 'default.member@example.com', 11, now, false),
    now + 5
  )
  assert.equal(removed.some((account) => account.id === teamId), false)
  const returned = projector.project(
    {
      codex: {
        systemDefault: {
          hasAuth: true,
          authKind: 'oauth',
          email: 'default.member@example.com',
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
        activeAccountId: null,
        activeAccountIdsByRuntime: { host: null, wsl: {} }
      },
      rateLimits: {
        codexTarget: { runtime: 'host', wslDistro: null },
        codex: quota(12, 21, now + 10),
        inactiveCodexAccounts: []
      }
    },
    now + 10
  )
  assert.equal(returned.find((account) => account.id === teamId)?.pools[0].remaining, -1)
  projector.clear()
  const wiped = projector.project(
    snapshot('team', 'default.member@example.com', 15, now + 20, true),
    now + 20
  )
  const card = wiped.find((account) => account.id === fallback)
  assert.equal(card?.pools[0].remaining, -1)
  assert.notEqual(card?.source_note, '마지막 확인값')
})

test('a workspace change invalidates cache and a stale reading does not replace ready data', () => {
  const projector = new OrcaAccountProjector()
  projector.project(
    snapshot(null, 'default.member@example.com', 11, now, true, 'ws-1', 'plus'),
    now
  )
  const moved = projector.project(
    snapshot('team', 'default.member@example.com', 15, now + 20, true, 'ws-2', 'plus'),
    now + 20
  )
  assert.equal(moved.find((account) => account.id === fallback)?.pools[0].remaining, -1)
  const ready = new OrcaAccountProjector()
  ready.project(
    snapshot(null, 'default.member@example.com', 11, now, true, undefined, 'plus'),
    now
  )
  const staleReading = ready.project(
    snapshot(null, 'default.member@example.com', 40, now - 2000, true, undefined, 'free'),
    now
  )
  assert.equal(staleReading.find((account) => account.id === fallback)?.pools[0].remaining, 60)
  assert.equal(staleReading.find((account) => account.id === fallback)?.state, 'stale')
  const restored = ready.project(
    snapshot('team', 'default.member@example.com', 15, now + 5, true),
    now + 5
  )
  const card = restored.find((account) => account.id === fallback)
  assert.equal(card?.pools[0].remaining, 89)
  assert.equal(card?.plan, 'plus')
  assert.equal(card?.updated_at, now)
})
