import { createServer, type IncomingMessage } from 'node:http'
import { randomBytes, timingSafeEqual } from 'node:crypto'
import { readFile } from 'node:fs/promises'
import { join } from 'node:path'
import { record } from './quota-snapshot.js'
import type { QuotamonController } from './quotamon-controller.js'

export interface BridgePreferences {
  copilot: boolean
  startup: boolean
}
export interface PortableUi {
  controller(): QuotamonController
  sourceAvailable(): boolean
  preferences(): BridgePreferences
  setPreferences(value: BridgePreferences): Promise<void>
  radio(): boolean | null
  webRoot: string
  assetsRoot: string
}
async function body(req: IncomingMessage): Promise<unknown> {
  const buffers: Buffer[] = []
  let size = 0
  for await (const chunk of req) {
    size += chunk.length
    if (size > 65536) throw Error('Request too large')
    buffers.push(chunk)
  }
  return JSON.parse(Buffer.concat(buffers).toString('utf8') || '{}')
}
export async function startPortableServer(ui: PortableUi, port = 0) {
  const token = randomBytes(32).toString('hex')
  let host = '',
    origin = '',
    uiError: string | null = null,
    connecting = false
  const observers = new Set<() => void>()
  const notify = (): void => { for (const observer of observers) observer() }
  const server = createServer((req, res) => {
    void (async () => {
      res.setHeader('Cache-Control', 'no-store')
      res.setHeader('X-Content-Type-Options', 'nosniff')
      res.setHeader('Referrer-Policy', 'no-referrer')
      res.setHeader(
        'Content-Security-Policy',
        "default-src 'self'; img-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'"
      )
      if (req.headers.host !== host) {
        res.writeHead(403)
        res.end()
        return
      }
      const path = (req.url || '').split('?')[0]
      const json = (status: number, value: unknown): void => {
        res.writeHead(status, {
          'Content-Type': 'application/json; charset=utf-8'
        })
        res.end(JSON.stringify(value))
      }
      if (path.startsWith('/api/')) {
        const key = req.headers['x-quotamon-key']
        if (
          typeof key !== 'string' ||
          !/^[a-f0-9]{64}$/.test(key) ||
          !timingSafeEqual(Buffer.from(key), Buffer.from(token))
        ) {
          json(403, { error: '설정 페이지를 트레이에서 다시 열어주세요.' })
          return
        }
        if (req.headers.origin && req.headers.origin !== origin) {
          json(403, { error: 'Invalid origin' })
          return
        }
        if (path === '/api/events' && req.method === 'GET') {
          const controller = ui.controller()
          res.writeHead(200, { 'Content-Type': 'text/event-stream; charset=utf-8' })
          let status = '', scheduledAt: number | null = null
          let timer: ReturnType<typeof setTimeout> | undefined
          const refresh = (): void => { res.write('data: refresh\n\n') }
          const changed = (force = false): void => {
            const { accounts: _accounts, next_snapshot_at, ...current } = controller.state()
            const nextStatus = JSON.stringify({
              ...current, source: ui.sourceAvailable(), radio: ui.radio(),
              preferences: ui.preferences(), connecting, uiError
            })
            if (force || nextStatus !== status) {
              status = nextStatus
              refresh()
            }
            // Re-arm only after an ACK or a change to the configured interval.
            // Source quota changes alone do not redraw the web dashboard.
            const next = next_snapshot_at === null ? null : next_snapshot_at - 2000
            if (next === scheduledAt) return
            scheduledAt = next
            if (timer) clearTimeout(timer)
            timer = undefined
            if (next !== null && next > Date.now())
              timer = setTimeout(refresh, next - Date.now())
          }
          const onState = (): void => changed()
          const onAction = (): void => changed(true)
          const disposed = (): void => { res.end() }
          const keepalive = setInterval(() => { res.write(': keepalive\n\n') }, 30000)
          controller.on('state', onState)
          controller.once('disposed', disposed)
          observers.add(onAction)
          res.once('close', () => {
            if (timer) clearTimeout(timer)
            clearInterval(keepalive)
            controller.off('state', onState)
            controller.off('disposed', disposed)
            observers.delete(onAction)
          })
          changed()
          return
        }
        if (path === '/api/state' && req.method === 'GET') {
          json(200, {
            ...ui.controller().state(),
            source_available: ui.sourceAvailable(),
            preferences: ui.preferences(),
            radio_available: ui.radio(),
            connecting,
            ui_error: uiError
          })
          return
        }
        if (
          req.method !== 'POST' ||
          req.headers.origin !== origin ||
          !req.headers['content-type']?.startsWith('application/json')
        ) {
          json(403, { error: 'Invalid request' })
          return
        }
        try {
          const input = await body(req),
            controller = ui.controller()
          switch (path) {
            case '/api/scan':
              await controller.scan()
              break
            case '/api/connect':
              if (
                !record(input) ||
                typeof input.address !== 'string' ||
                connecting
              )
                throw Error('Invalid connection')
              if (!ui.sourceAvailable()) throw Error('Orca not ready')
              connecting = true
              uiError = null
              notify()
              void controller
                .connect(input.address)
                .catch((error: unknown) => {
                  uiError =
                    error instanceof Error && error.message.startsWith('pairing-')
                      ? 'Windows Bluetooth 설정에서 Quotamon을 먼저 페어링한 뒤 이 화면에서 다시 연결하세요. 양쪽의 같은 6자리 번호를 승인해야 합니다.'
                      : '연결하지 못했습니다. 기기의 BLE 연결 시작과 양쪽 번호 승인을 확인하세요.'
                })
                .finally(() => {
                  connecting = false
                  notify()
                })
              break
            case '/api/confirm':
              if (
                !record(input) ||
                typeof input.pin !== 'string' ||
                typeof input.accept !== 'boolean'
              )
                throw Error('Invalid confirmation')
              await controller.confirm(input.pin, input.accept)
              break
            case '/api/save':
              controller.save(input)
              break
            case '/api/disconnect':
              await controller.disconnect(
                record(input) && input.forget === true
              )
              break
            case '/api/refresh':
              void controller.refresh()
                .catch(() => { uiError = '사용량 갱신을 확인할 수 없습니다.' })
                .finally(notify)
              break
            case '/api/preferences':
              if (
                !record(input) ||
                typeof input.copilot !== 'boolean' ||
                typeof input.startup !== 'boolean'
              )
                throw Error('Invalid preference')
              await ui.setPreferences({
                copilot: input.copilot,
                startup: input.startup
              })
              break
            default:
              json(404, { error: 'Unknown action' })
              return
          }
          uiError = null
          notify()
          json(200, { ok: true })
        } catch {
          json(409, {
            error:
              '처리하지 못했습니다. 최신 상태와 연결을 확인한 뒤 다시 시도하세요.'
          })
        }
        return
      }
      if (req.method !== 'GET') {
        res.writeHead(405)
        res.end()
        return
      }
      const files: Record<string, { file: string; type: string }> = {
        '/': {
          file: join(ui.webRoot, 'index.html'),
          type: 'text/html; charset=utf-8'
        },
        '/app.js': {
          file: join(ui.webRoot, 'app.js'),
          type: 'text/javascript; charset=utf-8'
        },
        '/style.css': {
          file: join(ui.webRoot, 'style.css'),
          type: 'text/css; charset=utf-8'
        },
        '/favicon.svg': {
          file: join(ui.webRoot, 'favicon.svg'),
          type: 'image/svg+xml'
        }
      }
      const asset = /^\/assets\/(codex|cursor|copilot|claude|grok)\.png$/.exec(
        path
      )
      const item =
        files[path] ||
        (asset
          ? {
              file: join(ui.assetsRoot, asset[1], 'idle_00.png'),
              type: 'image/png'
            }
          : undefined)
      if (!item) {
        res.writeHead(404)
        res.end()
        return
      }
      const content = await readFile(item.file)
      res.writeHead(200, { 'Content-Type': item.type })
      res.end(content)
    })().catch(() => {
      if (!res.headersSent) res.writeHead(500)
      res.end()
    })
  })
  await new Promise<void>((resolve, reject) => {
    server.once('error', reject)
    server.listen(port, '127.0.0.1', () => resolve())
  })
  const address = server.address()
  if (!address || typeof address === 'string')
    throw Error('No local settings port')
  host = `127.0.0.1:${address.port}`
  origin = `http://${host}`
  return {
    url: `${origin}/#${token}`,
    close: () =>
      new Promise<void>((resolve) => {
        server.closeAllConnections()
        server.close(() => resolve())
      })
  }
}
