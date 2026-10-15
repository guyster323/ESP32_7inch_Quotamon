import { createInterface } from 'node:readline'
import { fragment, Reassembler } from '../src/ble-protocol.js'
const emit = (value: unknown): void => {
  process.stdout.write(JSON.stringify(value) + '\n')
}
const rx = new Reassembler()
let pendingConnect: string | undefined
const completeConnection = (id: string): void => {
  emit({ type: 'connected', value_size: 244, address: '12345' })
  emit({ type: 'result', id, ok: true })
}
createInterface({ input: process.stdin }).on('line', (line) => {
  const request = JSON.parse(line)
  if (request.command === 'scan')
    emit({
      type: 'device',
      address: '12345',
      name: 'Quotamon · Preview',
      rssi: process.env.BLE_FIXTURE_PAIRED === '1' ? 0 : -52,
      paired: process.env.BLE_FIXTURE_PAIRED === '1'
    })
  if (request.command === 'connect' && process.env.BLE_FIXTURE_PAIRING_FAILURE === '1') {
    emit({ type: 'result', id: request.id, ok: false, code: 'pairing-Failed' })
    return
  }
  if (
    request.command === 'connect' &&
    process.env.BLE_FIXTURE_PAIRING === '1'
  ) {
    pendingConnect = request.id
    emit({ type: 'pairing', pin: '632814' })
    return
  }
  if (request.command === 'confirm' && pendingConnect) {
    const id = pendingConnect
    pendingConnect = undefined
    if (request.accept === true) completeConnection(id)
    else emit({ type: 'result', id, ok: false, code: 'pairing-rejected' })
  }
  if (request.command === 'connect')
    emit({ type: 'connected', value_size: 244, address: request.address })
  if (request.command === 'disconnect') emit({ type: 'disconnected' })
  if (request.command === 'write') {
    const message = rx.push(Buffer.from(request.frame, 'base64'))
    if (message?.kind === 1) {
      emit({
        type: 'received',
        snapshot: JSON.parse(Buffer.from(message.body).toString('utf8'))
      })
      // ACK arrives before the write result: exercise the real notification race.
      for (const f of fragment(
        2,
        0,
        Buffer.from(
          JSON.stringify({ type: 'ack', sequence: message.sequence })
        ),
        244
      ))
        emit({ type: 'notification', frame: Buffer.from(f).toString('base64') })
    }
  }
  emit({ type: 'result', id: request.id, ok: true })
})
