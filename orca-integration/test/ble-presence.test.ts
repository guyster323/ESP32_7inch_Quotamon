import { test } from 'node:test'
import assert from 'node:assert/strict'
import { spawn } from 'node:child_process'
import { join } from 'node:path'
import { BleHost } from '../src/ble-host.js'
import { QuotamonController } from '../src/quotamon-controller.js'
import { defaultConfig, validateConfig, makeSnapshot } from '../src/quota-snapshot.js'
import { parsePairedPresence } from '../src/paired-presence.js'
import { fragment } from '../src/ble-protocol.js'
const report = { type: 'paired_presence_status', schema: 2, capability: 'paired-presence',
  available: true, enabled: true, phone_bonded: true, phone_connected: true,
  presence_state: 'connected', peer_id: '1234567890abcdef1234567890abcdef', name: 'Phone', grace_remaining_seconds: 0 }
test('retired scan settings cannot enable paired sensing or destroy saved display settings', () => {
  const config = defaultConfig()
  const legacy = { ...config, display: { ...config.display, presence_enabled: true,
    presence_address: '1/AA:BB:CC:DD:EE:FF', presence_name: 'legacy', presence_aliases: ['old'] } }
  assert.deepEqual(validateConfig(legacy), config)
  assert.deepEqual(makeSnapshot([], validateConfig(legacy)).display, config.display)
  assert.deepEqual(validateConfig({ ...legacy, display: { ...legacy.display,
    presence_enabled: 'bad', presence_aliases: null, presence_name: '\ud800' } }), config)
})
test('paired status validates capability, opaque identity, state consistency and UTF-8', () => {
  for (const name of ['선우의 Z Fold7', '山田の携帯電話', '张伟的手机', 'Jörgs Gerät', '<img onerror=bad>'])
    assert.equal(parsePairedPresence({ ...report, name })?.name, name)
  for (const bad of [{ schema: 1 }, { capability: 'paired-trial' }, { peer_id: '1/AA:BB:CC:DD:EE:FF' },
    { phone_bonded: false }, { enabled: 'yes' }, { presence_state: 'absent' },
    { name: '\ud800' }, { name: 'a\nb' }, { name: '한'.repeat(33) },
    { grace_remaining_seconds: -1 }, { grace_remaining_seconds: 61 },
    { grace_remaining_seconds: 3 }, { available: false }])
    assert.equal(parsePairedPresence({ ...report, ...bad }), undefined)
  assert.equal(parsePairedPresence({ ...report, phone_connected: false,
    presence_state: 'disconnected_grace', grace_remaining_seconds: 60 })?.state, 'disconnected_grace')
  assert.equal(parsePairedPresence({ type: 'presence_status', scanning: true, devices: [] }), undefined)
})
test('board status reaches PC without scan commands; malformed and old reports are ignored', { timeout: 6000 }, async () => {
  const host = new BleHost('fixture', () => spawn(process.execPath, [join(__dirname, 'ble-resource-fixture.js')], {
    windowsHide: true, stdio: 'pipe', env: { SystemRoot: process.env.SystemRoot }
  }))
  const controller = new QuotamonController(host, {
    getSnapshot: () => ({}), refreshIfStale: async () => {}, onChanged: () => () => {}
  }, () => {})
  const emit = (sequence: number, body: unknown) => {
    for (const frame of fragment(2, sequence, Buffer.from(JSON.stringify(body)), 244))
      host.emit('event', { type: 'notification', frame: Buffer.from(frame).toString('base64') })
  }
  try {
    assert.equal(controller.state().presence.supported, false)
    await controller.connect('12345')
    emit(40, report)
    assert.equal(controller.state().presence.supported, true)
    assert.equal(controller.state().presence.connected, true)
    emit(41, { ...report, peer_id: 'bad' })
    emit(42, { type: 'presence_status', scanning: true, available: true, present: true, devices: [] })
    assert.equal(controller.state().presence.peer_id, report.peer_id)
    assert.equal(controller.state().connected, true)
    await controller.disconnect()
    assert.equal(controller.state().presence.supported, false)
    assert.equal(controller.state().presence.connected, false)
  } finally { controller.dispose() }
})
