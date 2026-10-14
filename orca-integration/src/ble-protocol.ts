export const BLE_SERVICE = '7155e001-9df0-4b75-9d9b-796d6f6e6974'
export const BLE_SNAPSHOT = '7155e002-9df0-4b75-9d9b-796d6f6e6974'
export const BLE_CONTROL = '7155e003-9df0-4b75-9d9b-796d6f6e6974'
export const MAX_MESSAGE = 32768
export function crc32(bytes: Uint8Array): number {
  let crc = 0xffffffff
  for (const byte of bytes) {
    crc ^= byte
    for (let i = 0; i < 8; i++) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1))
  }
  return ~crc >>> 0
}
export function fragment(
  kind: number,
  sequence: number,
  body: Uint8Array,
  valueSize = 20
): Uint8Array[] {
  if (
    !Number.isInteger(kind) ||
    kind < 1 ||
    kind > 3 ||
    body.length + 4 > MAX_MESSAGE ||
    valueSize < 9 ||
    valueSize > 512
  )
    throw Error('BLE message bounds')
  const bytes = new Uint8Array(body.length + 4)
  bytes.set(body)
  new DataView(bytes.buffer).setUint32(body.length, crc32(body), true)
  const result: Uint8Array[] = []
  for (let offset = 0; offset < bytes.length; ) {
    const take = Math.min(valueSize - 8, bytes.length - offset),
      frame = new Uint8Array(take + 8),
      view = new DataView(frame.buffer)
    frame[0] = 0x10 | kind
    frame[1] = (offset === 0 ? 1 : 0) | (offset + take === bytes.length ? 2 : 0)
    view.setUint16(2, sequence & 65535, true)
    view.setUint16(4, offset, true)
    view.setUint16(6, bytes.length, true)
    frame.set(bytes.subarray(offset, offset + take), 8)
    result.push(frame)
    offset += take
  }
  return result
}
export class Reassembler {
  private bytes: number[] = []
  private sequence = -1
  private kind = 0
  private total = 0
  reset(): void {
    this.bytes = []
    this.sequence = -1
  }
  push(
    frame: Uint8Array
  ): { kind: number; sequence: number; body: Uint8Array } | undefined {
    const fail = (): never => {
      this.reset()
      throw Error('Invalid BLE frame')
    }
    if (
      frame.length <= 8 ||
      frame[0] >>> 4 !== 1 ||
      (frame[0] & 15) < 1 ||
      (frame[0] & 15) > 3 ||
      frame[1] & ~3
    )
      fail()
    const view = new DataView(frame.buffer, frame.byteOffset, frame.byteLength),
      seq = view.getUint16(2, true),
      off = view.getUint16(4, true),
      total = view.getUint16(6, true)
    if (total < 4 || total > MAX_MESSAGE || off + frame.length - 8 > total)
      fail()
    if (frame[1] & 1) {
      if (off !== 0) fail()
      this.reset()
      this.sequence = seq
      this.kind = frame[0] & 15
      this.total = total
    }
    if (
      this.sequence !== seq ||
      this.kind !== (frame[0] & 15) ||
      this.total !== total ||
      off !== this.bytes.length
    )
      fail()
    this.bytes.push(...frame.subarray(8))
    if (!(frame[1] & 2)) {
      if (this.bytes.length === total) fail()
      return
    }
    if (this.bytes.length !== total) fail()
    const bytes = Uint8Array.from(this.bytes),
      body = bytes.subarray(0, total - 4)
    if (crc32(body) !== new DataView(bytes.buffer).getUint32(total - 4, true))
      fail()
    const result = { kind: this.kind, sequence: seq, body }
    this.reset()
    return result
  }
}
