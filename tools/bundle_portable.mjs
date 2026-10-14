import { createRequire } from 'node:module'
import { fileURLToPath } from 'node:url'
import { resolve } from 'node:path'
const require = createRequire(new URL('../orca-integration/package.json', import.meta.url))
const { build } = require('esbuild')
const root = fileURLToPath(new URL('../', import.meta.url))
await build({
  entryPoints: [resolve(root, 'orca-integration/src/portable-main.ts')],
  outfile: process.argv[2], bundle: true, platform: 'node', target: 'node22', format: 'cjs',
  logLevel: 'info'
})
