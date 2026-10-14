import { test } from 'node:test'
import assert from 'node:assert/strict'
import { RunningOrcaSource } from '../src/running-orca-source.js'
test('running Orca cache loss clears data, recovery emits change, refresh is coalesced', async () => {
  let online = true,
    queries = 0,
    refreshes = 0,
    changes = 0
  const snapshot = { codex: { systemDefault: { hasAuth: true } } }
  const source = new RunningOrcaSource(
    async () => {
      queries++
      if (!online) throw Error('private provider failure must not escape')
      return snapshot
    },
    async () => {
      refreshes++
      return snapshot
    },
    60000
  )
  source.onChanged(() => {
    changes++
  })
  try {
    await source.start()
    assert.equal(source.isAvailable(), true)
    assert.equal(changes, 1)
    await Promise.all([source.poll(), source.poll()])
    assert.equal(queries, 2)
    assert.equal(changes, 1)
    await source.refreshIfStale()
    await source.refreshIfStale()
    assert.equal(refreshes, 1)
    online = false
    await source.poll()
    assert.equal(source.isAvailable(), false)
    assert.throws(() => source.getSnapshot(), /Orca/)
    online = true
    await source.poll()
    assert.equal(source.isAvailable(), true)
  } finally {
    source.dispose()
  }
  assert.throws(() => source.getSnapshot())
})
test('a late account read cannot revive a disposed source', async () => {
  let finish: ((value: unknown) => void) | undefined
  const source = new RunningOrcaSource(
    () =>
      new Promise((resolve) => {
        finish = resolve
      })
  )
  const pending = source.start()
  source.dispose()
  finish?.({ codex: { systemDefault: { hasAuth: true } } })
  await pending
  assert.equal(source.isAvailable(), false)
})
