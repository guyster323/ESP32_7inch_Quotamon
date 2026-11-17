import { test } from 'node:test'
import assert from 'node:assert/strict'
import { mkdir, mkdtemp, rm, writeFile } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { findCopilotInstall } from '../src/copilot-quota-source.js'

async function touch(path: string): Promise<void> {
  await mkdir(join(path, '..'), { recursive: true })
  await writeFile(path, '')
}

test('findCopilotInstall prefers the npm package and otherwise uses the Windows installer', async () => {
  const root = await mkdtemp(join(tmpdir(), 'copilot-install-'))
  try {
    const appData = join(root, 'roaming')
    const local = join(root, 'local')
    const npmRoot = join(
      appData,
      'npm/node_modules/@github/copilot/node_modules/@github/copilot-win32-x64'
    )
    const npm = {
      cli: join(npmRoot, 'copilot.exe'),
      sdk: join(npmRoot, 'copilot-sdk/index.js')
    }
    const oldSdk = join(local, 'copilot/pkg/win32-x64/1.0.9/copilot-sdk/index.js')
    const newSdk = join(local, 'copilot/pkg/win32-x64/1.0.112/copilot-sdk/index.js')
    const shim = join(local, 'GitHubCopilotCLI/copilot.exe')
    const pathCli = join(root, 'bin/copilot.exe')
    await touch(npm.cli)
    await touch(npm.sdk)
    await touch(oldSdk)
    await touch(newSdk)
    await touch(shim)
    await touch(pathCli)
    assert.deepEqual(findCopilotInstall(appData, { LOCALAPPDATA: local, PATH: join(root, 'bin') }), npm)

    await rm(npmRoot, { recursive: true, force: true })
    assert.deepEqual(findCopilotInstall(appData, { LOCALAPPDATA: local, PATH: join(root, 'bin') }), {
      cli: shim,
      sdk: newSdk
    })

    await rm(join(local, 'GitHubCopilotCLI'), { recursive: true, force: true })
    assert.deepEqual(findCopilotInstall(appData, { LOCALAPPDATA: local, PATH: join(root, 'bin') }), {
      cli: pathCli,
      sdk: newSdk
    })

    const sibling = join(local, 'copilot/pkg/win32-x64/9.0.0/copilot.exe')
    await touch(sibling)
    await touch(join(local, 'copilot/pkg/win32-x64/9.0.0/copilot-sdk/index.js'))
    assert.equal(
      findCopilotInstall(appData, { LOCALAPPDATA: local, PATH: join(root, 'bin') })?.cli,
      sibling
    )

    await rm(join(local, 'copilot/pkg'), { recursive: true, force: true })
    assert.equal(findCopilotInstall(appData, { LOCALAPPDATA: local, PATH: join(root, 'bin') }), undefined)
  } finally {
    await rm(root, { recursive: true, force: true })
  }
})
