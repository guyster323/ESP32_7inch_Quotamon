import { record } from './quota-snapshot.js'

export type PresenceState = 'disabled' | 'unpaired' | 'pairing' | 'connected' |
  'disconnected_grace' | 'absent' | 'unavailable'
export interface PairedPresence {
  supported: boolean
  available: boolean
  enabled: boolean
  bonded: boolean
  connected: boolean
  state: PresenceState
  peer_id: string
  name: string
  grace_remaining_seconds: number
  updated_at: number
}
export const unknownPresence = (): PairedPresence => ({ supported: false,
  available: false, enabled: false, bonded: false, connected: false,
  state: 'unavailable', peer_id: '', name: '', grace_remaining_seconds: 0, updated_at: 0 })
export function parsePairedPresence(value: unknown): PairedPresence | undefined {
  if (!record(value) || value.type !== 'paired_presence_status' || value.schema !== 2 ||
      value.capability !== 'paired-presence') return
  for (const key of ['available', 'enabled', 'phone_bonded', 'phone_connected'])
    if (typeof value[key] !== 'boolean') return
  const states = ['disabled', 'unpaired', 'pairing', 'connected', 'disconnected_grace', 'absent', 'unavailable']
  if (typeof value.presence_state !== 'string' || !states.includes(value.presence_state) ||
      typeof value.peer_id !== 'string' || !/^(?:[a-f0-9]{32})?$/.test(value.peer_id) ||
      typeof value.name !== 'string' || Buffer.byteLength(value.name, 'utf8') > 96 ||
      /[\u0000-\u001f\u007f]/u.test(value.name) || Buffer.from(value.name, 'utf8').toString('utf8') !== value.name ||
      !Number.isInteger(value.grace_remaining_seconds) || typeof value.grace_remaining_seconds !== 'number' ||
      value.grace_remaining_seconds < 0 || value.grace_remaining_seconds > 60) return
  const available = value.available === true, enabled = value.enabled === true,
    bonded = value.phone_bonded === true, connected = value.phone_connected === true
  if (bonded !== (value.peer_id.length === 32) || connected && (!bonded || !available) ||
      enabled && !bonded || value.presence_state === 'connected' && !connected ||
      ['absent', 'disconnected_grace'].includes(value.presence_state) && (!available || !enabled || !bonded || connected) ||
      value.presence_state !== 'disconnected_grace' && value.grace_remaining_seconds !== 0) return
  return { supported: true, available, enabled, bonded, connected,
    state: value.presence_state as PresenceState, peer_id: value.peer_id, name: value.name,
    grace_remaining_seconds: value.grace_remaining_seconds, updated_at: Date.now() }
}
