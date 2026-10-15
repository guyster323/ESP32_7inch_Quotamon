import { createHash } from 'node:crypto'
import {
  DisplayAccount,
  fromOrca,
  publicId,
  record
} from './quota-snapshot.js'

type Slot = { identity: string; account: DisplayAccount }

// In-memory last-known quotas. Identity hashes never leave this object.
export class OrcaAccountProjector {
  private readonly slots = new Map<string, Slot>()
  clear(): void {
    this.slots.clear()
  }
  project(raw: unknown, now = Date.now() / 1000): DisplayAccount[] {
    const fresh = fromOrca(raw, now)
    const identity = identities(raw)
    const seen = new Set(fresh.map((account) => account.id))
    for (const id of this.slots.keys()) if (!seen.has(id)) this.slots.delete(id)
    return fresh.map((account) => this.bind(account, identity.get(account.id) ?? null))
  }
  private bind(account: DisplayAccount, identity: string | null): DisplayAccount {
    const cached = this.slots.get(account.id)
    if (cached && cached.identity !== identity) {
      this.slots.delete(account.id)
      if (!hasUsage(account) && account.id === publicId(account.provider, 'system-default'))
        return unknownIdentity(account)
    }
    if (account.state === 'ready' && identity) {
      this.slots.set(account.id, { identity, account: cloneAccount(account) })
      return account
    }
    const kept = this.slots.get(account.id)
    if (
      !hasUsage(account) &&
      identity &&
      kept?.identity === identity &&
      kept.account.state === 'ready'
    )
      return replay(account, kept.account)
    return account
  }
}

function cloneAccount(account: DisplayAccount): DisplayAccount {
  return { ...account, pools: account.pools.map((pool) => ({ ...pool })) }
}
function hasUsage(account: DisplayAccount): boolean {
  return account.pools.some((pool) => pool.remaining !== -1)
}
function replay(current: DisplayAccount, previous: DisplayAccount): DisplayAccount {
  return {
    ...current,
    plan: current.plan === '요금제 미확인' ? previous.plan : current.plan,
    pools: previous.pools.map((pool) => ({ ...pool })),
    state: 'stale',
    updated: previous.updated,
    updated_at: previous.updated_at,
    source_note: '마지막 확인값'
  }
}
function unknownIdentity(account: DisplayAccount): DisplayAccount {
  return {
    ...account,
    state: 'unknown',
    updated: '미확인',
    updated_at: null,
    source_note: '기본 계정 변경됨'
  }
}
function rows(value: unknown): unknown[] {
  return Array.isArray(value) ? value : []
}
function text(value: unknown): string {
  return typeof value === 'string' ? value.normalize('NFC').trim() : ''
}
function fingerprint(fields: Record<string, unknown>): string | null {
  const member = text(fields.email).toLowerCase()
  if (!member) return null
  return createHash('sha256')
    .update(
      [
        member,
        text(fields.authKind),
        text(fields.providerAccountId),
        text(fields.workspaceAccountId),
        text(fields.organizationUuid)
      ].join('\0')
    )
    .digest('hex')
}
function identities(raw: unknown): Map<string, string | null> {
  const root = record(raw) ? raw : {}
  const out = new Map<string, string | null>()
  for (const provider of ['codex', 'claude'] as const) {
    const block = record(root[provider]) ? root[provider] : null
    if (!block) continue
    const base = record(block.systemDefault) ? block.systemDefault : {}
    out.set(
      publicId(provider, 'system-default'),
      fingerprint({
        email: base.email ?? block.systemDefaultEmail,
        authKind: base.authKind,
        providerAccountId: base.providerAccountId,
        workspaceAccountId: base.workspaceAccountId,
        organizationUuid: base.organizationUuid
      })
    )
    for (const row of rows(block.accounts)) {
      if (!record(row) || typeof row.id !== 'string' || !row.id) continue
      out.set(publicId(provider, row.id), fingerprint(row))
    }
  }
  return out
}
