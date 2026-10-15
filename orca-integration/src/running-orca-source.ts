import { execFile } from 'node:child_process'
import { EventEmitter } from 'node:events'
import { readFile } from 'node:fs/promises'
import { join } from 'node:path'
import { createConnection } from 'node:net'
import { randomUUID, createHash } from 'node:crypto'
import { record } from './quota-snapshot.js'
import type { AccountSource } from './quotamon-controller.js'

export type OrcaQuery = () => Promise<unknown>
export function installedOrcaCli(): string {
  return (
    process.env.ORCA_CLI_COMMAND ||
    join(
      process.env.LOCALAPPDATA || '',
      'Programs',
      'orca',
      'resources',
      'bin',
      'orca.exe'
    )
  )
}
export function readRunningOrca(cli = installedOrcaCli()): Promise<unknown> {
  const env = { ...process.env }
  for (const key of [
    'ORCA_PAIRING_CODE',
    'ORCA_REMOTE_PAIRING',
    'ORCA_ENVIRONMENT'
  ])
    delete env[key]
  return new Promise((resolve, reject) => {
    execFile(
      cli,
      ['account', 'list', '--json'],
      {
        windowsHide: true,
        timeout: 20000,
        maxBuffer: 1024 * 1024,
        env
      },
      (error, stdout) => {
        // Never forward provider responses or child stderr into logs/UI.
        if (error) {
          reject(Error('Orca 연결 대기'))
          return
        }
        try {
          const envelope: unknown = JSON.parse(stdout)
          if (
            !record(envelope) ||
            envelope.ok !== true ||
            !record(envelope.result) ||
            !record(envelope.result.rateLimits)
          )
            throw Error('Invalid account snapshot')
          resolve(envelope.result)
        } catch {
          reject(Error('Orca 사용량 응답을 확인할 수 없습니다.'))
        }
      }
    )
  })
}

// Existing owner-local IPC, only to refresh Orca's own cache. The runtime token
// stays inside this function and is never used in a UI/device/config snapshot.
export async function refreshRunningOrca(): Promise<unknown> {
  const base =
    process.env.ORCA_USER_DATA_PATH || join(process.env.APPDATA || '', 'orca')
  const text = await readFile(join(base, 'orca-runtime.json'), 'utf8')
  if (Buffer.byteLength(text) > 1024 * 1024)
    throw Error('Invalid runtime metadata')
  const metadata: unknown = JSON.parse(text.replace(/^\uFEFF/, ''))
  if (
    !record(metadata) ||
    typeof metadata.authToken !== 'string' ||
    typeof metadata.runtimeId !== 'string'
  )
    throw Error('Orca 연결 대기')
  const transports = Array.isArray(metadata.transports)
    ? metadata.transports
    : [metadata.transport]
  const transport = transports.find((v) => record(v) && v.kind === 'named-pipe')
  if (
    !record(transport) ||
    typeof transport.endpoint !== 'string' ||
    !transport.endpoint.startsWith('\\\\.\\pipe\\')
  )
    throw Error('No local Orca pipe')
  const endpoint = transport.endpoint
  const id = randomUUID(),
    token = metadata.authToken,
    runtimeId = metadata.runtimeId
  return new Promise((resolve, reject) => {
    const socket = createConnection(endpoint)
    let buffer = ''
    let bytes = 0
    const timer = setTimeout(() => fail(), 30000)
    function fail(): void {
      clearTimeout(timer)
      socket.destroy()
      reject(Error('Orca 갱신 응답 대기'))
    }
    socket.on('connect', () =>
      socket.write(
        JSON.stringify({
          id,
          authToken: token,
          method: 'accounts.list',
          params: { refreshUsage: true }
        }) + '\n'
      )
    )
    socket.setEncoding('utf8')
    socket.on('data', (chunk) => {
      bytes += Buffer.byteLength(chunk)
      if (bytes > 1024 * 1024) {
        fail()
        return
      }
      buffer += chunk
      while (buffer.includes('\n')) {
        const at = buffer.indexOf('\n'),
          line = buffer.slice(0, at)
        buffer = buffer.slice(at + 1)
        try {
          const response: unknown = JSON.parse(line)
          if (!record(response)) {
            fail()
            return
          }
          if (response._keepalive) continue
          if (
            response.id !== id ||
            response.ok !== true ||
            !record(response.result) ||
            !record(response._meta) ||
            response._meta.runtimeId !== runtimeId
          ) {
            fail()
            return
          }
          clearTimeout(timer)
          socket.destroy()
          resolve(response.result)
          return
        } catch {
          fail()
          return
        }
      }
    })
    socket.on('error', fail)
    socket.on('end', fail)
  })
}

export class RunningOrcaSource extends EventEmitter implements AccountSource {
  private snapshot: unknown
  private available = false
  private pollInflight: Promise<void> | undefined
  private refreshInflight: Promise<void> | undefined
  private requestSeq = 0
  private appliedSeq = 0
  private stopped = false
  private timer: ReturnType<typeof setInterval> | undefined
  private lastProviderRefresh = 0
  private fingerprint = ''
  constructor(
    private readonly query: OrcaQuery = () => readRunningOrca(),
    private readonly providerRefresh: OrcaQuery = refreshRunningOrca,
    private readonly intervalMs = 15000
  ) {
    super()
  }
  isAvailable(): boolean {
    return this.available
  }
  getSnapshot(): unknown {
    if (!this.available) throw Error('Orca 연결 대기')
    return this.snapshot
  }
  onChanged(listener: () => void): () => void {
    this.on('changed', listener)
    return () => this.off('changed', listener)
  }
  async start(): Promise<void> {
    if (this.stopped || this.timer) return
    this.timer = setInterval(() => {
      void this.poll()
    }, this.intervalMs)
    this.timer.unref()
    await this.poll()
  }
  async poll(): Promise<void> {
    if (this.stopped) return
    if (this.pollInflight) return this.pollInflight
    const seq = ++this.requestSeq
    const run = Promise.resolve()
      .then(() => this.query())
      .then((snapshot) => {
        this.publish(seq, snapshot)
      })
      .catch(() => {
        this.lose(seq)
      })
    this.pollInflight = run
    try {
      await run
    } finally {
      if (this.pollInflight === run) this.pollInflight = undefined
    }
  }
  async refreshIfStale(): Promise<void> {
    // The regular CLI reads the cache. At most one owner-local cache refresh per
    // 15 minutes prevents a minimized Orca window from freezing quota forever.
    if (this.stopped) return
    if (!this.available) return this.poll()
    if (this.refreshInflight) return this.refreshInflight
    if (Date.now() - this.lastProviderRefresh < 15 * 60000) return this.poll()
    this.lastProviderRefresh = Date.now()
    const seq = ++this.requestSeq
    const run = Promise.resolve()
      .then(() => this.providerRefresh())
      .then((snapshot) => {
        this.publish(seq, snapshot)
      })
      .catch(async () => {
        if (!this.stopped && seq > this.appliedSeq) await this.poll()
      })
    this.refreshInflight = run
    try {
      await run
    } finally {
      if (this.refreshInflight === run) this.refreshInflight = undefined
    }
  }
  private publish(seq: number, snapshot: unknown): void {
    if (this.stopped || seq <= this.appliedSeq) return
    let fingerprint: string
    try {
      const encoded = JSON.stringify(snapshot)
      if (typeof encoded !== 'string') throw Error('fingerprint')
      fingerprint = createHash('sha256').update(encoded).digest('hex')
    } catch {
      this.lose(seq)
      return
    }
    if (this.stopped || seq <= this.appliedSeq) return
    this.appliedSeq = seq
    const changed = !this.available || this.fingerprint !== fingerprint
    this.snapshot = snapshot
    this.available = true
    this.fingerprint = fingerprint
    if (changed) this.emit('changed')
  }
  private lose(seq: number): void {
    if (this.stopped || seq <= this.appliedSeq) return
    this.appliedSeq = seq
    const changed = this.available
    this.snapshot = undefined
    this.available = false
    this.fingerprint = ''
    this.lastProviderRefresh = 0
    if (changed) this.emit('changed')
  }
  dispose(): void {
    this.stopped = true
    if (this.timer) clearInterval(this.timer)
    this.snapshot = undefined
    this.available = false
    this.removeAllListeners()
  }
}
