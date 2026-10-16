import { test } from 'node:test'
import assert from 'node:assert/strict'
import { installedOrcaCli, readRunningOrca, RunningOrcaSource } from '../src/running-orca-source.js'

test('missing Orca install environment rejects without a relative executable', async () => {
  const previous = { local: process.env.LOCALAPPDATA, cli: process.env.ORCA_CLI_COMMAND }
  try {
    process.env.LOCALAPPDATA = ''
    process.env.ORCA_CLI_COMMAND = ''
    assert.equal(installedOrcaCli(), undefined)
    await assert.rejects(readRunningOrca(), /Orca/)
  } finally {
    if (previous.local === undefined) delete process.env.LOCALAPPDATA
    else process.env.LOCALAPPDATA = previous.local
    if (previous.cli === undefined) delete process.env.ORCA_CLI_COMMAND
    else process.env.ORCA_CLI_COMMAND = previous.cli
  }
})
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
  const started = gate<void>()
  const read = gate<unknown>()
  const source = new RunningOrcaSource(() => {
    started.resolve()
    return read.promise
  })
  const pending = source.start()
  await started.promise
  source.dispose()
  read.resolve({ codex: { systemDefault: { hasAuth: true } } })
  await pending
  assert.equal(source.isAvailable(), false)
  assert.throws(() => source.getSnapshot(), /Orca/)
})

function gate<T>() {
  let resolve: (value: T) => void = () => {}
  let reject: (error: Error) => void = () => {}
  const promise = new Promise<T>((res, rej) => {
    resolve = res
    reject = rej
  })
  return { promise, resolve, reject }
}

test('an older refresh cannot overwrite a newer poll or revive a disposed source', async () => {
  const refresh = gate<unknown>()
  const poll = gate<unknown>()
  let mode: 'boot' | 'race' = 'boot'
  const source = new RunningOrcaSource(
    () => (mode === 'boot' ? Promise.resolve({ owner: 'boot' }) : poll.promise),
    () => refresh.promise
  )
  try {
    await source.poll()
    mode = 'race'
    const pendingRefresh = source.refreshIfStale()
    const pendingPoll = source.poll()
    poll.resolve({ owner: 'poll' })
    await pendingPoll
    assert.deepEqual(source.getSnapshot(), { owner: 'poll' })
    refresh.resolve({ owner: 'refresh' })
    await pendingRefresh
    assert.deepEqual(source.getSnapshot(), { owner: 'poll' })
  } finally {
    source.dispose()
  }
})
test('a provider refresh that resolves after dispose cannot revive the source', async () => {
  const started = gate<void>()
  const refresh = gate<unknown>()
  const source = new RunningOrcaSource(async () => ({ owner: 'boot' }), () => {
    started.resolve()
    return refresh.promise
  })
  try {
    await source.poll()
    const pending = source.refreshIfStale()
    await started.promise
    source.dispose()
    refresh.resolve({ owner: 'late' })
    await pending
    assert.equal(source.isAvailable(), false)
    assert.throws(() => source.getSnapshot(), /Orca/)
  } finally {
    source.dispose()
  }
})

test('an older poll failure cannot clear a newer applied refresh', async () => {
  const refresh = gate<unknown>()
  const poll = gate<unknown>()
  let mode: 'boot' | 'race' | 'same' = 'boot'
  let changes = 0
  const source = new RunningOrcaSource(
    () =>
      mode === 'boot'
        ? Promise.resolve({ owner: 'boot' })
        : mode === 'race'
          ? poll.promise
          : Promise.resolve({ owner: 'refresh' }),
    () => refresh.promise
  )
  source.onChanged(() => {
    changes++
  })
  try {
    await source.poll()
    mode = 'race'
    const pendingPoll = source.poll()
    const pendingRefresh = source.refreshIfStale()
    refresh.resolve({ owner: 'refresh' })
    await pendingRefresh
    assert.equal(changes, 2)
    poll.reject(Error('stale read'))
    await pendingPoll
    assert.equal(source.isAvailable(), true)
    assert.deepEqual(source.getSnapshot(), { owner: 'refresh' })
    mode = 'same'
    await source.poll()
    assert.equal(changes, 2)
  } finally {
    source.dispose()
  }
})

test('an older refresh cannot revive the source after a newer poll loss', async () => {
  const refresh = gate<unknown>()
  const poll = gate<unknown>()
  let mode: 'boot' | 'race' = 'boot'
  const source = new RunningOrcaSource(
    () => (mode === 'boot' ? Promise.resolve({ owner: 'boot' }) : poll.promise),
    () => refresh.promise
  )
  try {
    await source.poll()
    mode = 'race'
    const pendingRefresh = source.refreshIfStale()
    const pendingPoll = source.poll()
    poll.reject(Error('lost'))
    await pendingPoll
    assert.equal(source.isAvailable(), false)
    refresh.resolve({ owner: 'stale-refresh' })
    await pendingRefresh
    assert.equal(source.isAvailable(), false)
    assert.throws(() => source.getSnapshot(), /Orca/)
  } finally {
    source.dispose()
  }
})

test('a synchronous query throw releases the poll and the next read recovers', async () => {
  let fail = true
  const source = new RunningOrcaSource((): Promise<unknown> => {
    if (fail) throw Error('sync')
    return Promise.resolve({ owner: 'recovered' })
  })
  try {
    await source.poll()
    assert.equal(source.isAvailable(), false)
    fail = false
    await source.poll()
    assert.deepEqual(source.getSnapshot(), { owner: 'recovered' })
  } finally {
    source.dispose()
  }
})

test('concurrent refresh calls share one provider refresh', async () => {
  const started = gate<void>()
  let refreshes = 0
  let release: (value: unknown) => void = () => {}
  const source = new RunningOrcaSource(async () => ({ owner: 'cache' }), () => {
    started.resolve()
    return new Promise((resolve) => {
      refreshes++
      release = resolve
    })
  })
  try {
    await source.poll()
    const first = source.refreshIfStale()
    const second = source.refreshIfStale()
    await started.promise
    assert.equal(refreshes, 1)
    release({ owner: 'fresh' })
    await Promise.all([first, second])
    assert.deepEqual(source.getSnapshot(), { owner: 'fresh' })
  } finally {
    source.dispose()
  }
})

test('a snapshot that cannot be fingerprinted clears the previous reading', async () => {
  let circular = false
  const source = new RunningOrcaSource((): Promise<unknown> => {
    if (!circular) return Promise.resolve({ owner: 'boot' })
    const value: Record<string, unknown> = { owner: 'bad' }
    value.self = value
    return Promise.resolve(value)
  })
  try {
    await source.poll()
    circular = true
    await source.poll()
    assert.equal(source.isAvailable(), false)
    assert.throws(() => source.getSnapshot(), /Orca/)
  } finally {
    source.dispose()
  }
})
