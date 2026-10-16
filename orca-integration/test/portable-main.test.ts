import { test } from 'node:test'
import assert from 'node:assert/strict'
import { spawn } from 'node:child_process'
import { once } from 'node:events'
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { createInterface } from 'node:readline'
import { defaultConfig, record } from '../src/quota-snapshot.js'

test('portable startup backs up corrupt settings and serves default config and preferences',
  { timeout: 15000 }, async () => {
    const changed = { ...defaultConfig(), revision: 7 }
    for (const body of ['{broken', JSON.stringify({ config: null }),
      JSON.stringify({ config: changed, preferences: { copilot: 'invalid' } })]) {
      const directory = await mkdtemp(join(tmpdir(), 'quotamon-settings-'))
      const path = join(directory, 'settings.json')
      await writeFile(path, body)
      const child = spawn(process.execPath, [join(__dirname, '../src/portable-main.js'),
        '--root', directory, '--config', path, '--helper', process.execPath], {
        windowsHide: true, stdio: 'pipe', env: {
          ...process.env, LOCALAPPDATA: directory, APPDATA: directory,
          ORCA_CLI_COMMAND: join(directory, 'missing-orca.exe')
        }
      })
      const exited = once(child, 'exit')
      const lines = createInterface({ input: child.stdout })
      let timer: ReturnType<typeof setTimeout> | undefined
      try {
        const ready = new Promise<string>((resolve, reject) => {
          timer = setTimeout(() => reject(Error('Startup deadline')), 5000)
          child.once('exit', () => reject(Error('Startup exited')))
          lines.on('line', line => {
            const event: unknown = JSON.parse(line)
            if (record(event) && event.type === 'ready' && typeof event.url === 'string') resolve(event.url)
          })
        })
        const url = new URL(await ready)
        const response = await fetch(url.origin + '/api/state', {
          headers: { 'X-Quotamon-Key': url.hash.slice(1) }
        })
        assert.equal(response.status, 200)
        const state = await response.json()
        assert.deepEqual(state.config, defaultConfig())
        assert.deepEqual(state.preferences, { copilot: true, startup: false })
        assert.equal(await readFile(path + '.bad', 'utf8'), body)
      } finally {
        if (timer) clearTimeout(timer)
        lines.close()
        child.stdin.end()
        child.kill()
        await exited
        await rm(directory, { recursive: true, force: true })
      }
    }
  })
