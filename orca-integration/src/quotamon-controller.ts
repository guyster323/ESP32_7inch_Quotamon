import { EventEmitter } from 'node:events'
import { BleHost } from './ble-host.js'
import { fragment, Reassembler } from './ble-protocol.js'
import { OrcaAccountProjector } from './orca-account-projector.js'
import { parseRuntimeMetrics } from './runtime-metrics.js'
import { type PairedPresence, parsePairedPresence, unknownPresence } from './paired-presence.js'
import {
  Config,
  DisplayAccount,
  Snapshot,
  defaultConfig,
  makeSnapshot,
  record,
  validateConfig
} from './quota-snapshot.js'
export interface AccountSource {
  getSnapshot(): unknown
  refreshIfStale(): Promise<void>
  onChanged(listener: () => void): () => void
  isAvailable?(): boolean
  copilot?(): Promise<DisplayAccount>
}
export interface ControllerState {
  connected: boolean
  next_snapshot_at: number | null
  pairingPin: string | null
  devices: { address: string; name: string; rssi: number; paired?: boolean }[]
  presence: PairedPresence
  config: Config
  accounts: DisplayAccount[]
  error: string | null
}
// Shared by the portable bridge and optional Orca source integration.
export class QuotamonController extends EventEmitter {
  private config: Config
  private presence: PairedPresence = unknownPresence()
  private connected = false
  private devices = new Map<
    string,
    { address: string; name: string; rssi: number; paired?: boolean }
  >()
  private pairingPin: string | null = null
  private fault: string | null = null
  private cachedCopilot: DisplayAccount | undefined
  private cachedSource: DisplayAccount[] = []
  private readonly projector = new OrcaAccountProjector()
  private sequence = 0
  private valueSize = 20
  private receive = new Reassembler()
  private sending = false
  private dirty = false
  private lastSent = 0
  private lastHeartbeat = 0
  private lastRefresh = 0
  private lastAttempt = 0
  private connecting = false
  private refreshPending: Promise<void> | undefined
  private ack:
    | { sequence: number; resolve: () => void; reject: (e: Error) => void }
    | undefined
  private timer: ReturnType<typeof setInterval>
  private unsubscribe: () => void
  private stopped = false
  constructor(
    private readonly host: BleHost,
    private readonly source: AccountSource,
    private readonly persist: (config: Config) => void,
    config = defaultConfig()
  ) {
    super()
    this.config = validateConfig(config)
    this.updateSource()
    host.on('event', (event: unknown) => {
      void this.onEvent(event).catch(() => this.fail('BLE event rejected'))
    })
    host.on('fault', () => this.fail('Bluetooth 중계 연결 대기'))
    this.unsubscribe = source.onChanged(() => {
      if (source.isAvailable?.() === false) this.lastRefresh = 0
      this.updateSource()
      // Cache quota changes until the configured send deadline. Settings,
      // connection and explicit refresh still request an immediate snapshot.
      this.publish()
    })
    this.timer = setInterval(() => {
      void this.tick().catch(() => this.fail('BLE transfer failed'))
    }, 1000)
    this.timer.unref()
  }
  state(): ControllerState {
    return {
      connected: this.connected,
      next_snapshot_at: this.connected && this.lastSent
        ? this.lastSent + this.config.display.poll_seconds * 1000
        : null,
      pairingPin: this.pairingPin,
      devices: [...this.devices.values()],
      presence: this.presence,
      config: this.config,
      accounts: this.accounts(),
      error: this.fault
    }
  }
  private accounts(): DisplayAccount[] {
    if (this.source.isAvailable?.() === false) return []
    const a = [...this.cachedSource]
    if (this.cachedCopilot) a.push(this.cachedCopilot)
    return makeSnapshot(a, this.config, Date.now() / 1000, true).accounts
  }
  private updateSource(): void {
    try {
      if (this.source.isAvailable?.() === false) throw Error('Orca 연결 대기')
      this.cachedSource = this.projector.project(this.source.getSnapshot())
      if (this.fault === 'Orca 사용량 정보 대기 중') this.fault = null
    } catch {
      this.projector.clear()
      this.cachedSource = []
      this.cachedCopilot = undefined
      this.fault = 'Orca 사용량 정보 대기 중'
    }
  }
  private publish(): void {
    this.emit('state', this.state())
  }
  private fail(message: string): void {
    if (this.stopped) return
    this.connected = false
    this.presence = unknownPresence()
    this.sending = false
    this.fault = message
    this.receive.reset()
    this.ack?.reject(Error(message))
    this.ack = undefined
    this.publish()
  }
  async scan(): Promise<void> {
    this.devices.clear()
    this.fault = null
    await this.host.request('scan')
    this.publish()
  }
  async connect(address: string): Promise<void> {
    if (!/^\d{1,20}$/.test(address) || this.connecting)
      throw Error('Invalid device')
    this.connecting = true
    this.lastAttempt = Date.now()
    try {
      await this.host.request('connect', { address }, 95000)
      if (this.stopped) return
      this.config = { ...this.config, address }
      this.persist(this.config)
      this.publish()
    } finally {
      this.connecting = false
    }
  }
  async confirm(pin: string, accept: boolean): Promise<void> {
    if (pin !== this.pairingPin || !/^\d{6}$/.test(pin))
      throw Error('Pairing challenge expired')
    await this.host.request('confirm', { pin, accept })
    this.pairingPin = null
    this.publish()
  }
  async disconnect(forget = false): Promise<void> {
    if (forget) {
      this.config = { ...this.config, address: null }
      this.persist(this.config)
    }
    await this.host.request('disconnect')
    this.connected = false
    this.presence = unknownPresence()
    this.publish()
  }
  save(input: unknown): void {
    const next = validateConfig(input)
    if (next.revision !== this.config.revision)
      throw Error('Settings changed; reload before saving')
    // Device address is changed only by the explicit connect action.
    const committed = {
      ...next,
      address: this.config.address,
      revision: this.config.revision + 1
    }
    this.persist(committed)
    this.config = committed
    this.dirty = true
    this.publish()
  }
  async refresh(): Promise<void> {
    if (this.refreshPending) return this.refreshPending
    if (Date.now() - this.lastRefresh < 30000) return
    this.lastRefresh = Date.now()
    this.refreshPending = (async () => {
      await this.source.refreshIfStale()
      if (this.stopped) return
      this.updateSource()
      if (this.source.isAvailable?.() === false) {
        this.cachedCopilot = undefined
        this.dirty = true
        this.publish()
        return
      }
      if (this.source.copilot) {
        try {
          const copilot = await this.source.copilot()
          if (this.stopped) return
          if (this.source.isAvailable?.() === false) {
            this.cachedCopilot = undefined
            return
          }
          this.cachedCopilot = copilot
        } catch {
          if (this.cachedCopilot)
            this.cachedCopilot = {
              ...this.cachedCopilot,
              state: 'stale',
              source_note: 'Copilot 조회 실패'
            }
        }
      }
      this.dirty = true
      this.publish()
    })()
    try {
      await this.refreshPending
    } finally {
      this.refreshPending = undefined
    }
  }
  private async onEvent(event: unknown): Promise<void> {
    if (this.stopped) return
    if (!record(event)) return
    if (event.type === 'adapter') {
      this.publish()
      return
    }
    if (
      event.type === 'device' &&
      typeof event.address === 'string' &&
      /^\d{1,20}$/.test(event.address) &&
      typeof event.name === 'string' &&
      typeof event.rssi === 'number'
    ) {
      if (this.devices.size < 20 || this.devices.has(event.address))
        this.devices.set(event.address, {
          address: event.address,
          name: event.name.slice(0, 64) || 'Quotamon',
          rssi: event.rssi,
          paired: event.paired === true || this.devices.get(event.address)?.paired === true
        })
      this.publish()
    } else if (
      event.type === 'pairing' &&
      typeof event.pin === 'string' &&
      /^\d{6}$/.test(event.pin)
    ) {
      this.pairingPin = event.pin
      this.publish()
    } else if (event.type === 'pairing-expired') {
      this.pairingPin = null
      this.publish()
    } else if (event.type === 'connected') {
      this.connected = true
      this.presence = unknownPresence()
      this.fault = null
      this.receive.reset()
      this.valueSize =
        typeof event.value_size === 'number'
          ? Math.max(20, Math.min(512, event.value_size))
          : 20
      this.dirty = true
      this.publish()
      void this.refresh().catch(() => {})
    } else if (event.type === 'disconnected') {
      this.fail(
        this.source.isAvailable?.() === false
          ? 'Orca 연결 대기'
          : '디스플레이 연결 대기'
      )
    } else if (
      event.type === 'notification' &&
      typeof event.frame === 'string' &&
      event.frame.length < 1024
    ) {
      const message = this.receive.push(Buffer.from(event.frame, 'base64'))
      if (!message || message.kind !== 2) return
      let request: Record<string, unknown>
      try {
        const value: unknown = JSON.parse(Buffer.from(message.body).toString('utf8'))
        if (!record(value)) return
        request = value
      } catch { return }
      if (request.type === 'paired_presence_status') {
        const presence = parsePairedPresence(request)
        if (presence) { this.presence = presence; this.publish() }
      } else if (request.type === 'runtime_stats') {
        const metrics = parseRuntimeMetrics(request)
        if (metrics) this.emit('runtime-metrics', metrics)
      } else if (request.type === 'ack' && request.sequence === this.ack?.sequence) {
        this.ack?.resolve()
        this.ack = undefined
      } else if (request.type === 'refresh') {
        await this.refresh()
        this.dirty = true
      } else if (
        request.type === 'settings' &&
        this.source.isAvailable?.() === false
      ) {
        this.dirty = true
        this.fault = 'Orca 사용량 정보 대기 중'
        this.publish()
      } else if (
        request.type === 'settings' &&
        request.base_revision === this.config.revision &&
        record(request.display) &&
        Array.isArray(request.accounts)
      ) {
        const selections = new Map(
          request.accounts.filter(record).map((v) => [v.id, v.enabled])
        )
        const accounts = this.accounts().map((a) => ({
          id: a.id,
          alias: this.config.accounts.find((s) => s.id === a.id)?.alias ?? '',
          plan: this.config.accounts.find((s) => s.id === a.id)?.plan ?? '',
          enabled: selections.get(a.id) === true
        }))
        // Older firmware omits the optional locale/schedule fields. Preserve
        // them when that firmware changes an existing display preference.
        this.save({ ...this.config, display: {
          ...this.config.display, ...request.display
        }, accounts })
      } else if (request.type === 'settings') {
        this.dirty = true
        this.fault = '설정이 변경되었습니다. 최신 값을 다시 확인하세요.'
        this.publish()
      }
    }
  }
  private async tick(): Promise<void> {
    if (this.stopped) return
    const now = Date.now()
    if (this.source.isAvailable?.() === false) {
      if (this.connected && !this.connecting) {
        this.connected = false
        this.receive.reset()
        this.ack?.reject(Error('Orca 연결 대기'))
        this.ack = undefined
        await this.host.request('disconnect')
        this.fault = 'Orca 연결 대기'
        this.publish()
      }
      return
    }
    if (!this.connected) {
      if (
        this.config.address &&
        !this.connecting &&
        now - this.lastAttempt > 30000
      )
        void this.connect(this.config.address).catch(() =>
          this.fail('BLE 재연결 대기')
        )
      return
    }
    if (now - this.lastRefresh > 15 * 60 * 1000 && !this.refreshPending)
      void this.refresh().catch(() => {
        this.fault = '사용량 갱신 실패'
        this.publish()
      })
    if (this.sending) return
    this.sending = true
    try {
      if (
        this.dirty ||
        now - this.lastSent >= this.config.display.poll_seconds * 1000
      ) {
        this.dirty = false
        const snapshot: Snapshot = makeSnapshot(this.accounts(), this.config)
        const sequence = ++this.sequence & 65535
        // Install the ACK waiter before the final GATT write can trigger a notification.
        let ackTimer: ReturnType<typeof setTimeout> | undefined
        const acknowledged = new Promise<void>((resolve, reject) => {
          this.ack = { sequence, resolve, reject }
        })
        // Prevent an unhandled rejection if the connection fails while writes are in flight.
        void acknowledged.catch(() => {})
        const started = Date.now()
        try {
          for (const frame of fragment(
            1,
            sequence,
            Buffer.from(JSON.stringify(snapshot)),
            this.valueSize
          )) {
            if (Date.now() - started > 30000)
              throw Error('Snapshot transfer deadline')
            await this.host.request('write', {
              frame: Buffer.from(frame).toString('base64')
            })
          }
          ackTimer = setTimeout(() => {
            this.ack?.reject(Error('Snapshot ACK timeout'))
            this.ack = undefined
          }, 10000)
          await acknowledged
          this.lastSent = Date.now()
          this.lastHeartbeat = this.lastSent
          this.publish()
        } finally {
          if (ackTimer) clearTimeout(ackTimer)
          this.ack = undefined
        }
      } else if (now - this.lastHeartbeat >= 10000) {
        const heartbeat = Buffer.alloc(8)
        heartbeat.writeBigUInt64LE(BigInt(now))
        for (const frame of fragment(
          3,
          ++this.sequence,
          heartbeat,
          this.valueSize
        ))
          await this.host.request('write', {
            frame: Buffer.from(frame).toString('base64')
          })
        this.lastHeartbeat = Date.now()
      }
    } finally {
      this.sending = false
    }
  }
  dispose(): void {
    this.stopped = true
    this.emit('disposed')
    clearInterval(this.timer)
    this.unsubscribe()
    this.projector.clear()
    this.cachedSource = []
    this.ack?.reject(Error('중계 종료'))
    this.host.dispose()
  }
}
