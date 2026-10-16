import { test } from 'node:test'
import assert from 'node:assert/strict'
import { defaultConfig, validateConfig, makeSnapshot } from '../src/quota-snapshot.js'
import { languages } from '../src/languages.js'
import { EventEmitter } from 'node:events'
import { BleHost } from '../src/ble-host.js'
import { QuotamonController } from '../src/quotamon-controller.js'
import { fragment } from '../src/ble-protocol.js'

test('legacy settings migrate to Korean with screen scheduling disabled', () => {
  const config = defaultConfig()
  const old = structuredClone(config) as unknown as { display: Record<string, unknown> }
  for (const key of ['language', 'sleep_enabled', 'sleep_start_minute', 'sleep_end_minute']) delete old.display[key]
  assert.deepEqual(validateConfig(old), config)
})
test('five languages and overnight screen schedules survive validation, storage and snapshots', () => {
  for (const language of languages) {
    const config = defaultConfig()
    config.display = { ...config.display, language, sleep_enabled: true, sleep_start_minute: 1425, sleep_end_minute: 375 }
    const restored = validateConfig(JSON.parse(JSON.stringify(config)))
    assert.deepEqual(restored, config)
    const snapshot = makeSnapshot([], restored, 1800000000)
    assert.deepEqual(snapshot.display, config.display)
    assert.equal(snapshot.utc_offset_minutes, -new Date(1800000000000).getTimezoneOffset())
  }
})
test('invalid locales, schedule types, equal endpoints and out-of-day values are rejected', () => {
  for (const display of [
    { language: 'fr' }, { language: '' }, { language: null }, { sleep_enabled: 'true' },
    { sleep_start_minute: -1 }, { sleep_end_minute: 1440 }, { sleep_start_minute: 23.5 },
    { sleep_end_minute: null }, { sleep_start_minute: NaN },
    { sleep_enabled: true, sleep_start_minute: 600, sleep_end_minute: 600 }
  ]) {
    const config = defaultConfig()
    assert.throws(() => validateConfig({ ...config, display: { ...config.display, ...display } }))
  }
})
test('device locale/schedule changes synchronize to the PC and legacy edits preserve them', async () => {
  const host = Object.assign(new EventEmitter(), { request: async () => {}, dispose: () => {} })
  let saved = defaultConfig(), commits = 0
  const controller = new QuotamonController(host as unknown as BleHost, {
    getSnapshot: () => ({}), refreshIfStale: async () => {}, onChanged: () => () => {}, isAvailable: () => true
  }, value => { saved = value; commits++ })
  async function notify(display: unknown, revision: number) {
    const body = Buffer.from(JSON.stringify({ type: 'settings', base_revision: revision, display, accounts: [] }))
    for (const frame of fragment(2, revision + 1, body, 20)) host.emit('event', { type: 'notification', frame: Buffer.from(frame).toString('base64') })
    await new Promise(resolve => setImmediate(resolve))
  }
  try {
    const display = { ...saved.display, language: 'ja', sleep_enabled: true, sleep_start_minute: 1380, sleep_end_minute: 420 }
    await notify(display, 0)
    assert.equal(commits, 1)
    assert.deepEqual(saved.display, display)
    const legacy = { poll_seconds: 90, brightness_percent: 70, show_reset: true, show_updated: true, show_credits: true, show_session: true }
    await notify(legacy, 1)
    assert.equal(commits, 2)
    assert.equal(saved.display.language, 'ja')
    assert.equal(saved.display.sleep_enabled, true)
    assert.equal(saved.display.poll_seconds, 90)
    await notify({ ...display, language: 'de' }, 0) // Stale device draft.
    assert.equal(commits, 2)
    assert.equal(saved.display.language, 'ja')
  } finally { controller.dispose() }
})
