import { test } from 'node:test'
import assert from 'node:assert/strict'
import { parseRuntimeMetrics } from '../src/runtime-metrics.js'
import { EventEmitter } from 'node:events'
import { BleHost } from '../src/ble-host.js'
import { QuotamonController } from '../src/quotamon-controller.js'
import { fragment } from '../src/ble-protocol.js'

const sample = {
  type: 'runtime_stats', schema: 1, uptime_ms: 60000, window_ms: 5010,
  page: -1, visible_characters: 5, ble_connected: true, cpu_valid: true,
  cpu_percent: [12.3, 41.2], cpu_average_percent: 26.75, refresh_frames: 25,
  lvgl_refresh_fps: 4.99, render_mean_ms: 12.5, render_max_ms: 24,
  pixels_per_refresh: 18000, psram_free_bytes: 700000, psram_minimum_free_bytes: 650000,
  psram_largest_free_block_bytes: 600000, internal_free_bytes: 120000, internal_minimum_free_bytes: 110000
}
test('runtime diagnostics preserve counters and discard unrelated private peer fields', () => {
  const parsed = parseRuntimeMetrics({ ...sample, accounts: ['private'], token: 'private', address: 'private' })
  assert.deepEqual(parsed, sample)
  assert.notEqual(parsed?.cpu_percent, sample.cpu_percent)
})

test('runtime diagnostics accept up to 32MiB of free PSRAM', () => {
  const limit = 32 * 1024 * 1024
  for (const bytes of [16 * 1024 * 1024, limit])
    assert.equal(parseRuntimeMetrics({ ...sample, psram_free_bytes: bytes })?.psram_free_bytes, bytes)
  assert.equal(parseRuntimeMetrics({ ...sample, psram_free_bytes: limit + 1 }), undefined)
})
test('runtime diagnostics reject malformed CPU, heap and schema values', () => {
  for (const patch of [{ schema: 2 }, { cpu_percent: [10, 101] }, { cpu_percent: [NaN, 0] },
    { cpu_valid: 'yes' }, { page: 100 }, { window_ms: 0 }, { visible_characters: 6 },
    { psram_free_bytes: -1 }, { psram_free_bytes: Infinity }, { psram_minimum_free_bytes: 800000 },
    { psram_largest_free_block_bytes: 800000 }, { type: 'ack' }])
    assert.equal(parseRuntimeMetrics({ ...sample, ...patch }), undefined)
})
test('fragmented runtime reports reach the diagnostics listener without refreshing the dashboard', async () => {
  const host = Object.assign(new EventEmitter(), { request: async () => {}, dispose: () => {} })
  const controller = new QuotamonController(host as unknown as BleHost, {
    getSnapshot: () => ({}), refreshIfStale: async () => {}, onChanged: () => () => {}
  }, () => {})
  let updates = 0
  const received: unknown[] = []
  controller.on('state', () => { updates++ })
  controller.on('runtime-metrics', value => { received.push(value) })
  try {
    for (const frame of fragment(2, 41, Buffer.from(JSON.stringify({ ...sample, token: 'discard' })), 20))
      host.emit('event', { type: 'notification', frame: Buffer.from(frame).toString('base64') })
    await new Promise(resolve => setImmediate(resolve))
    assert.deepEqual(received, [sample])
    assert.equal(updates, 0)
  } finally { controller.dispose() }
})
