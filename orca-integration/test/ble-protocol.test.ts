import { test } from 'node:test'
import assert from 'node:assert/strict'
import { fragment, Reassembler, crc32 } from '../src/ble-protocol.js'
test('known CRC and BLE MTU fragmentation, corruption, missing/out-of-order parts', () => {
  assert.equal(crc32(Buffer.from('123456789')), 0xcbf43926)
  assert.equal(
    Buffer.from(fragment(1, 0x1234, Buffer.from('abc'))[0]).toString('hex'),
    '1103341200000700616263c2412435'
  )
  const body = Buffer.alloc(24000)
  for (let i = 0; i < body.length; i++) body[i] = i % 256
  for (const size of [20, 244, 512]) {
    const frames = fragment(1, 65535, body, size),
      rx = new Reassembler()
    let output
    for (const f of frames) output = rx.push(f)
    assert.ok(output)
    assert.equal(output.sequence, 65535)
    assert.deepEqual(output.body, new Uint8Array(body))
    frames.at(-1)![9] ^= 1
    assert.throws(() => {
      for (const f of frames) rx.push(f)
    })
    rx.reset()
    rx.push(frames[0])
    assert.throws(() => rx.push(frames[2]))
    assert.throws(() =>
      rx.push(Uint8Array.of(0x11, 3, 0, 0, 0, 0, 255, 255, 0))
    )
  }
})
