import { record } from './quota-snapshot.js'

const counters = [
  'uptime_ms', 'window_ms', 'visible_characters', 'cpu_average_percent',
  'refresh_frames', 'lvgl_refresh_fps', 'render_mean_ms', 'render_max_ms',
  'pixels_per_refresh', 'psram_free_bytes', 'psram_minimum_free_bytes',
  'psram_largest_free_block_bytes', 'internal_free_bytes', 'internal_minimum_free_bytes'
] as const
export type RuntimeMetrics = Record<typeof counters[number], number> & {
  type: 'runtime_stats'; schema: 1; page: number;
  ble_connected: boolean; cpu_valid: boolean; cpu_percent: [number, number]
}
// Persist only numeric diagnostics. Never copy arbitrary peer/account fields.
export function parseRuntimeMetrics(input: unknown): RuntimeMetrics | undefined {
  if (!record(input) || input.type !== 'runtime_stats' || input.schema !== 1 ||
      typeof input.ble_connected !== 'boolean' || typeof input.cpu_valid !== 'boolean' ||
      typeof input.page !== 'number' || !Number.isInteger(input.page) || input.page < -9 || input.page > 19 ||
      !Array.isArray(input.cpu_percent) || input.cpu_percent.length !== 2 ||
      input.cpu_percent.some(v => typeof v !== 'number' || !Number.isFinite(v) || v < 0 || v > 100)) return
  const output = { type: 'runtime_stats', schema: 1, page: input.page,
    ble_connected: input.ble_connected, cpu_valid: input.cpu_valid,
    cpu_percent: [...input.cpu_percent] } as RuntimeMetrics
  for (const key of counters) {
    const value = input[key]
    if (typeof value !== 'number' || !Number.isFinite(value) || value < 0 || value > Number.MAX_SAFE_INTEGER) return
    output[key] = value
  }
  if (output.window_ms < 1000 || output.window_ms > 60000 || output.visible_characters > 5 ||
      output.cpu_average_percent > 100 || output.psram_free_bytes > 32 * 1024 * 1024 ||
      output.psram_minimum_free_bytes > output.psram_free_bytes ||
      output.psram_largest_free_block_bytes > output.psram_free_bytes) return
  return output
}
