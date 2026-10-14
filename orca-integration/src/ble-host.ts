import { createInterface } from 'node:readline'
import type { ChildProcessWithoutNullStreams } from 'node:child_process'
import { EventEmitter } from 'node:events'
import { record } from './quota-snapshot.js'
export type SpawnResource = (path: string) => ChildProcessWithoutNullStreams
export class BleHost extends EventEmitter {
  private child: ChildProcessWithoutNullStreams
  private next = 0
  private pending = new Map<
    string,
    {
      resolve: () => void
      reject: (reason: Error) => void
      timer: ReturnType<typeof setTimeout>
    }
  >()
  constructor(path: string, spawn: SpawnResource) {
    super()
    this.child = spawn(path)
    const lines = createInterface({ input: this.child.stdout })
    lines.on('line', (line) => {
      if (line.length > 8192) {
        this.emit('fault', 'helper-output-bound')
        this.dispose()
        return
      }
      try {
        const event: unknown = JSON.parse(line)
        if (!record(event) || typeof event.type !== 'string') return
        if (event.type === 'result' && typeof event.id === 'string') {
          const p = this.pending.get(event.id)
          if (!p) return
          clearTimeout(p.timer)
          this.pending.delete(event.id)
          if (event.ok === true) p.resolve()
          else
            p.reject(
              Error(
                typeof event.code === 'string'
                  ? event.code
                  : 'BLE operation failed'
              )
            )
        } else this.emit('event', event)
      } catch {
        this.emit('fault', 'helper-output-invalid')
      }
    })
    const fault = (): void => {
      for (const p of this.pending.values()) {
        clearTimeout(p.timer)
        p.reject(Error('BLE resource stopped'))
      }
      this.pending.clear()
      this.emit('fault', 'helper-stopped')
    }
    this.child.on('error', fault)
    this.child.on('exit', () => {
      lines.close()
      fault()
    })
    this.child.stdin.on('error', fault)
    this.child.stdout.on('error', fault)
    this.child.stderr.on('data', () => {})
    this.child.stderr.on('error', fault)
  }
  request(
    command: string,
    fields: Record<string, unknown> = {},
    timeout = 15000
  ): Promise<void> {
    const id = String(++this.next)
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id)
        reject(Error('BLE command timeout'))
      }, timeout)
      this.pending.set(id, { resolve, reject, timer })
      this.child.stdin.write(
        JSON.stringify({ ...fields, id, command }) + '\n',
        (error) => {
          if (error) {
            clearTimeout(timer)
            this.pending.delete(id)
            reject(error)
          }
        }
      )
    })
  }
  dispose(): void {
    this.child.stdin.end()
    this.child.kill()
  }
}
