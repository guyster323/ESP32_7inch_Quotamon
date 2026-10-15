import { createHash } from 'node:crypto'
export type Provider = 'codex' | 'cursor' | 'claude' | 'grok' | 'copilot'
export interface DisplaySettings {
  poll_seconds: number
  brightness_percent: number
  show_reset: boolean
  show_updated: boolean
  show_credits: boolean
  show_session: boolean
}
export interface Selection {
  id: string
  alias: string
  plan: string
  enabled: boolean
}
export interface Config {
  revision: number
  address: string | null
  display: DisplaySettings
  accounts: Selection[]
}
export const defaultConfig = (): Config => ({
  revision: 0,
  address: null,
  display: {
    poll_seconds: 60,
    brightness_percent: 70,
    show_reset: true,
    show_updated: true,
    show_credits: true,
    show_session: true
  },
  accounts: []
})
export interface Pool {
  name: string
  remaining: number
  reset: string
  resets_at: number | null
}
export interface DisplayAccount {
  id: string
  provider: Provider
  title: string
  profile: string
  plan: string
  enabled: boolean
  pools: Pool[]
  state: 'ready' | 'stale' | 'unknown'
  updated: string
  updated_at: number | null
  source_note: string
  has_amounts: boolean
  has_session: boolean
  monthly_used?: number
  monthly_limit?: number
}
export interface Snapshot {
  protocol: 'quotamon.v1'
  produced_at: number
  refreshed: string
  config_revision: number
  connected: boolean
  mock: boolean
  accounts: DisplayAccount[]
  display: DisplaySettings
}
export function record(v: unknown): v is Record<string, unknown> {
  return typeof v === 'object' && v !== null && !Array.isArray(v)
}
const object = (v: unknown): Record<string, unknown> => (record(v) ? v : {})
const rows = (v: unknown): unknown[] => (Array.isArray(v) ? v : [])
const names: Record<Provider, string> = {
  codex: 'Codex',
  cursor: 'Cursor',
  claude: 'Claude',
  grok: 'Grok',
  copilot: 'Copilot'
}
export const safeLabel = (v: unknown, fallback: string, limit = 64): string =>
  typeof v === 'string' && v.trim() && !v.includes('@')
    ? v.normalize('NFC').replace(/\s+/g, ' ').trim().slice(0, limit)
    : fallback
export const publicId = (provider: Provider, id: string): string =>
  `${provider}-${createHash('sha256').update(id).digest('hex').slice(0, 16)}`
export function storedPlan(plan: string): string {
  const label = safeLabel(plan, '')
  return !label || label === '요금제 미확인' ? '' : label
}
function observedPlan(q: Record<string, unknown>): string {
  const raw = q.planType ?? q.plan
  return typeof raw === 'string'
    ? storedPlan(raw) || '요금제 미확인'
    : '요금제 미확인'
}
const finite = (v: unknown): v is number =>
  typeof v === 'number' && Number.isFinite(v)
export function timestamp(v: unknown): number | null {
  const n = finite(v)
    ? v > 1e11
      ? v / 1000
      : v
    : typeof v === 'string' && /([zZ]|[+-]\d\d:\d\d)$/.test(v)
      ? Date.parse(v) / 1000
      : NaN
  return Number.isFinite(n) && n >= 0 && n <= 4102444800 ? n : null
}
export function resetText(at: number | null, now: number): string {
  if (at === null) return '시각 미확인'
  if (at <= now) return '리셋 후 확인 대기'
  const m = Math.ceil((at - now) / 60)
  return m >= 1440
    ? `${Math.floor(m / 1440)}d ${Math.floor((m % 1440) / 60)}h`
    : m >= 60
      ? `${Math.floor(m / 60)}h ${m % 60}m`
      : `${m}m`
}
function normalize(
  provider: Provider,
  id: string,
  label: unknown,
  raw: unknown,
  now: number
): DisplayAccount {
  const q = object(raw),
    pools: Pool[] = []
  const add = (v: unknown, name: unknown): void => {
    if (!record(v) || pools.length >= 8) return
    const used = v.usedPercent,
      reset = timestamp(v.resetsAt)
    const remaining =
      v.isUnlimited === true || v.unlimited === true
        ? -2
        : finite(used) && used >= 0 && used <= 100
          ? 100 - used
          : -1
    pools.push({
      name: safeLabel(name, 'PLAN', 24),
      remaining,
      reset: resetText(reset, now),
      resets_at: reset
    })
  }
  if (provider === 'cursor') {
    // Keep the two model budgets in display order; Monthly is an aggregate.
    for (const name of ['Cursor Models', 'Other Models']) {
      const bucket = rows(q.buckets).find(
        (v) => safeLabel(object(v).name, '').toLowerCase() === name.toLowerCase()
      )
      add(bucket ?? {}, name)
    }
  } else {
    const minutes = object(q.session).windowMinutes
    add(
      q.session,
      finite(minutes) && minutes > 0 && minutes % 60 === 0
        ? `${minutes / 60}H`
        : 'SESSION'
    )
    add(q.weekly, 'WEEKLY')
    add(q.fableWeekly, 'FABLE WEEKLY')
    add(q.monthly, 'MONTHLY')
    for (const bucket of rows(q.buckets)) add(bucket, object(bucket).name)
  }
  const known = pools.some((p) => p.remaining !== -1),
    updated = timestamp(q.updatedAt)
  let state: DisplayAccount['state'] =
    known && q.status === 'ok' ? 'ready' : 'unknown'
  if (
    known &&
    (q.status !== 'ok' ||
      object(q.usageMetadata).failureKind === 'rate-limited' ||
      updated === null ||
      now - updated > 1800 ||
      updated - now > 300 ||
      pools.some((p) => p.resets_at !== null && p.resets_at <= now))
  )
    state = 'stale'
  const profile = safeLabel(label, names[provider])
  return {
    id: publicId(provider, id),
    provider,
    title: profile.startsWith(names[provider])
      ? profile
      : `${names[provider]} / ${profile}`,
    profile,
    plan: observedPlan(q),
    enabled: false,
    pools: pools.length
      ? pools
      : [
          { name: 'PLAN', remaining: -1, reset: '시각 미확인', resets_at: null }
        ],
    state,
    updated:
      updated === null
        ? '미확인'
        : new Date(updated * 1000).toLocaleTimeString('ko-KR', {
            hour12: false
          }),
    updated_at: updated,
    source_note:
      state === 'stale'
        ? '오래된 정보'
        : state === 'unknown'
          ? '사용량 확인 불가'
          : '',
    has_amounts: false,
    has_session: false
  }
}
type Owner =
  | { kind: 'managed'; id: string }
  | { kind: 'system-default' }
  | { kind: 'none' }
const hasOwn = (value: object, key: string): boolean =>
  Object.prototype.hasOwnProperty.call(value, key)
function managedIds(accounts: Record<string, unknown>): Set<string> {
  const ids = new Set<string>()
  for (const row of rows(accounts.accounts)) {
    if (record(row) && typeof row.id === 'string' && row.id) ids.add(row.id)
  }
  return ids
}
function usableQuota(
  value: unknown,
  provider: Provider
): Record<string, unknown> | undefined {
  if (!record(value)) return undefined
  if (value.provider !== undefined && value.provider !== provider) return undefined
  return value
}
function laneKey(distro: unknown): string {
  return typeof distro === 'string' && distro.trim() ? distro.trim() : '__default__'
}
function managedRow(
  accounts: Record<string, unknown>,
  id: string
): Record<string, unknown> | undefined {
  for (const row of rows(accounts.accounts)) {
    if (record(row) && row.id === id) return row
  }
  return undefined
}
function runtimeCompatible(
  row: Record<string, unknown>,
  provider: 'codex' | 'claude',
  runtime: 'host' | 'wsl',
  wslKey: string
): boolean {
  const field = provider === 'codex' ? 'managedHomeRuntime' : 'managedAuthRuntime'
  if (!hasOwn(row, field)) return true
  if (runtime === 'host') return row[field] !== 'wsl'
  if (row[field] !== 'wsl') return false
  return laneKey(row.wslDistro) === wslKey
}
// Orca targets name a runtime lane, not an account. Explicit null on the host
// lane is that host's system default. A WSL null is that distro's own home.
function activeOwner(
  accounts: Record<string, unknown>,
  rates: Record<string, unknown>,
  provider: 'codex' | 'claude'
): Owner {
  const targetKey = provider === 'codex' ? 'codexTarget' : 'claudeTarget'
  const inactiveKey =
    provider === 'codex' ? 'inactiveCodexAccounts' : 'inactiveClaudeAccounts'
  const present = hasOwn(rates, targetKey)
  const raw = present ? rates[targetKey] : undefined
  const target =
    raw === undefined && !present
      ? { runtime: 'host', wslDistro: null }
      : raw
  if (
    !record(target) ||
    (target.runtime !== 'host' && target.runtime !== 'wsl') ||
    (target.runtime === 'host' && target.wslDistro != null)
  )
    return { kind: 'none' }
  const ids = managedIds(accounts)
  const selection = record(accounts.activeAccountIdsByRuntime)
    ? accounts.activeAccountIdsByRuntime
    : null
  const accept = (id: unknown, runtime: 'host' | 'wsl', wslKey: string): Owner => {
    if (typeof id !== 'string' || !ids.has(id)) return { kind: 'none' }
    const row = managedRow(accounts, id)
    return row && runtimeCompatible(row, provider, runtime, wslKey)
      ? { kind: 'managed', id }
      : { kind: 'none' }
  }
  if (target.runtime === 'wsl') {
    const key = laneKey(target.wslDistro)
    const wsl = selection && record(selection.wsl) ? selection.wsl : null
    if (!wsl) return { kind: 'none' }
    if (hasOwn(wsl, key)) {
      if (wsl[key] === null) return { kind: 'none' }
      return accept(wsl[key], 'wsl', key)
    }
    if (key !== '__default__') return { kind: 'none' }
    const selected = [
      ...new Set(
        Object.values(wsl).filter(
          (id): id is string => typeof id === 'string' && id.length > 0
        )
      )
    ]
    if (selected.length !== 1) return { kind: 'none' }
    const selectedKey =
      Object.entries(wsl).find(([, value]) => value === selected[0])?.[0] ??
      '__default__'
    return accept(selected[0], 'wsl', selectedKey)
  }
  let id: unknown
  let explicit = false
  if (selection && hasOwn(selection, 'host')) {
    id = selection.host
    explicit = true
  } else if (hasOwn(accounts, 'activeAccountId')) {
    id = accounts.activeAccountId
    explicit = true
  }
  if (id === null) return { kind: 'system-default' }
  if (typeof id === 'string') return accept(id, 'host', '__default__')
  if (
    !explicit &&
    record(raw) &&
    hasOwn(raw, 'accountId') &&
    typeof raw.accountId === 'string'
  )
    return accept(raw.accountId, 'host', '__default__')
  const legacy =
    !present && selection === null && !hasOwn(accounts, 'activeAccountId')
  if (legacy) {
    const inactive = new Set<string>()
    for (const row of rows(rates[inactiveKey])) {
      if (record(row) && typeof row.accountId === 'string')
        inactive.add(row.accountId)
    }
    if ([...ids].every((managedId) => inactive.has(managedId)))
      return { kind: 'system-default' }
  }
  return { kind: 'none' }
}
// Only explicitly constructed display fields leave Orca; raw account/auth objects never cross BLE.
export function fromOrca(
  raw: unknown,
  now = Date.now() / 1000
): DisplayAccount[] {
  const root = object(raw),
    rates = object(root.rateLimits),
    result: DisplayAccount[] = []
  for (const provider of ['codex', 'claude'] as const) {
    const accounts = object(root[provider])
    const inactive = rows(
      rates[
        provider === 'codex'
          ? 'inactiveCodexAccounts'
          : 'inactiveClaudeAccounts'
      ]
    )
    const owner = activeOwner(accounts, rates, provider)
    const live = usableQuota(rates[provider], provider)
    const base = object(accounts.systemDefault),
      quota = owner.kind === 'system-default' ? live : undefined
    if (
      base.hasAuth === true ||
      (record(quota) && quota.status !== 'unavailable')
    )
      result.push(
        normalize(
          provider,
          'system-default',
          `${names[provider]} / 기본`,
          quota,
          now
        )
      )
    rows(accounts.accounts).forEach((v) => {
      if (!record(v) || typeof v.id !== 'string') return
      const match = inactive.find((a) => object(a).accountId === v.id)
      const q =
        owner.kind === 'managed' && owner.id === v.id
          ? live
          : usableQuota(object(match).rateLimits, provider)
      result.push(
        normalize(
          provider,
          v.id,
          v.workspaceLabel ?? v.label ?? `${names[provider]} / 관리 계정`,
          q,
          now
        )
      )
    })
  }
  for (const provider of ['cursor', 'grok'] as const) {
    const q = rates[provider]
    if (
      rates[`${provider}AuthConfigured`] === true ||
      (record(q) &&
        (q.status !== 'unavailable' ||
          q.session ||
          q.weekly ||
          q.monthly ||
          rows(q.buckets).length))
    )
      result.push(normalize(provider, 'active', names[provider], q, now))
  }
  return result.slice(0, 20)
}
export function copilotQuota(
  raw: unknown,
  now = Date.now() / 1000
): DisplayAccount {
  const q = object(object(object(raw).quotaSnapshots).premium_interactions)
  const used = q.usedRequests,
    limit = q.entitlementRequests,
    percent = q.remainingPercentage
  const valid =
    finite(used) &&
    used >= 0 &&
    finite(limit) &&
    limit > 0 &&
    finite(percent) &&
    percent >= 0 &&
    percent <= 100
  const unlimited = q.isUnlimitedEntitlement === true || limit === -1
  const next = new Date(now * 1000)
  next.setUTCMonth(next.getUTCMonth() + 1, 1)
  next.setUTCHours(0, 0, 0, 0)
  const out = normalize('copilot', 'active', 'Copilot', {}, now)
  out.pools = [
    {
      name: 'MONTHLY',
      remaining: unlimited ? -2 : valid ? percent : -1,
      reset: resetText(next.getTime() / 1000, now),
      resets_at: next.getTime() / 1000
    }
  ]
  out.state = valid || unlimited ? 'ready' : 'unknown'
  out.updated_at = valid || unlimited ? now : null
  out.updated =
    valid || unlimited
      ? new Date(now * 1000).toLocaleTimeString('ko-KR', { hour12: false })
      : '미확인'
  // SDK calls these "requests"; AIC interpretation is not assumed without independent confirmation.
  out.source_note = valid
    ? 'Copilot SDK · 단위 확인 필요'
    : 'Copilot 사용량 원천 미연결'
  return out
}
export function validateConfig(value: unknown): Config {
  if (
    !record(value) ||
    !record(value.display) ||
    !Array.isArray(value.accounts) ||
    value.accounts.length > 20
  )
    throw Error('Invalid settings')
  const d = value.display
  if (
    !Number.isInteger(d.poll_seconds) ||
    !finite(d.poll_seconds) ||
    d.poll_seconds < 15 ||
    d.poll_seconds > 3600 ||
    !Number.isInteger(d.brightness_percent) ||
    !finite(d.brightness_percent) ||
    d.brightness_percent < 10 ||
    d.brightness_percent > 100
  )
    throw Error('Settings bounds')
  for (const key of [
    'show_reset',
    'show_updated',
    'show_credits',
    'show_session'
  ])
    if (typeof d[key] !== 'boolean') throw Error('Settings flag')
  if (
    !Number.isInteger(value.revision) ||
    !finite(value.revision) ||
    value.revision < 0 ||
    value.revision > 2147483646 ||
    (value.address !== null &&
      (typeof value.address !== 'string' || !/^\d{1,20}$/.test(value.address)))
  )
    throw Error('Settings identity')
  const accounts: Selection[] = [],
    ids = new Set<string>()
  for (const row of value.accounts) {
    if (
      !record(row) ||
      typeof row.id !== 'string' ||
      !/^(codex|cursor|claude|grok|copilot)-[0-9a-f]{16}$/.test(row.id) ||
      ids.has(row.id) ||
      typeof row.enabled !== 'boolean' ||
      typeof row.alias !== 'string' ||
      typeof row.plan !== 'string'
    )
      throw Error('Account settings')
    ids.add(row.id)
    accounts.push({
      id: row.id,
      alias: safeLabel(row.alias, ''),
      plan: storedPlan(row.plan),
      enabled: row.enabled
    })
  }
  return {
    revision: value.revision,
    address: value.address,
    accounts,
    display: {
      poll_seconds: d.poll_seconds,
      brightness_percent: d.brightness_percent,
      show_reset: d.show_reset === true,
      show_updated: d.show_updated === true,
      show_credits: d.show_credits === true,
      show_session: d.show_session === true
    }
  }
}
function configuredUnknown(selected: Selection): DisplayAccount {
  const prefix = selected.id.split('-')[0]
  const provider: Provider =
    prefix === 'claude' ||
    prefix === 'cursor' ||
    prefix === 'grok' ||
    prefix === 'copilot'
      ? prefix
      : 'codex'
  const profile = selected.alias || names[provider]
  return {
    id: selected.id,
    provider,
    title: profile.startsWith(names[provider])
      ? profile
      : `${names[provider]} / ${profile}`,
    profile,
    plan: storedPlan(selected.plan) || '요금제 미확인',
    enabled: selected.enabled,
    pools: [{ name: 'PLAN', remaining: -1, reset: '시각 미확인', resets_at: null }],
    state: 'unknown',
    updated: '미확인',
    updated_at: null,
    source_note: '사용량 확인 불가',
    has_amounts: false,
    has_session: false
  }
}
export function makeSnapshot(
  accounts: DisplayAccount[],
  config: Config,
  now = Date.now() / 1000,
  includeUnselectedQuota = false
): Snapshot {
  const byId = new Map(accounts.map((a) => [a.id, a])),
    ordered: DisplayAccount[] = []
  for (const selected of config.accounts) {
    const a = byId.get(selected.id) ?? configuredUnknown(selected)
    if (byId.has(selected.id)) byId.delete(selected.id)
    const profile = selected.alias || a.profile
    ordered.push({
      ...a,
      profile,
      title: profile.startsWith(names[a.provider])
        ? profile
        : `${names[a.provider]} / ${profile}`,
      plan: storedPlan(selected.plan) || a.plan,
      enabled: selected.enabled
    })
  }
  ordered.push(...byId.values())
  const fresh = ordered.slice(0, 20).map((a) => ({
    ...a,
    state:
      a.state === 'ready' &&
      (a.updated_at === null ||
        now - a.updated_at > 1800 ||
        a.pools.some((p) => p.resets_at !== null && p.resets_at <= now))
        ? ('stale' as const)
        : a.state,
    pools: a.pools.map((p) => ({ ...p, reset: resetText(p.resets_at, now) }))
  }))
  return {
    protocol: 'quotamon.v1',
    produced_at: now,
    refreshed: new Date(now * 1000).toLocaleTimeString('ko-KR', { hour12: false }),
    config_revision: config.revision,
    connected: true,
    mock: false,
    accounts: fresh.map((a): DisplayAccount => {
      if (a.enabled || includeUnselectedQuota) return a
      return {
        id: a.id,
        provider: a.provider,
        title: a.title,
        profile: a.profile,
        plan: a.plan,
        enabled: false,
        pools: [
          { name: 'PLAN', remaining: -1, reset: '시각 미확인', resets_at: null }
        ],
        state: 'unknown',
        updated: '미확인',
        updated_at: null,
        source_note: '비선택 계정 · 수치 전송 없음',
        has_amounts: false,
        has_session: false
      }
    }),
    display: config.display
  }
}
