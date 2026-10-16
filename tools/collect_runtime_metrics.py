"""Collect fresh ESP32 counters received by the installed portable bridge.

The bridge keeps only the latest numeric report beside settings.json. This tool
does not open BLE connections or read account settings/credentials.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import json
import os
import time

ROOT = Path(__file__).resolve().parents[1]
FIELDS = ('received_at', 'type', 'schema', 'uptime_ms', 'window_ms', 'page', 'visible_characters',
          'ble_connected', 'cpu_valid', 'cpu_percent', 'cpu_average_percent', 'refresh_frames',
          'lvgl_refresh_fps', 'render_mean_ms', 'render_max_ms', 'pixels_per_refresh',
          'psram_free_bytes', 'psram_minimum_free_bytes', 'psram_largest_free_block_bytes',
          'internal_free_bytes', 'internal_minimum_free_bytes')


def summarize(samples):
    windows = sum(s['window_ms'] for s in samples)
    frames = sum(s['refresh_frames'] for s in samples)
    weighted = lambda key: round(sum(s[key] * s['window_ms'] for s in samples) / windows, 3)
    return {
        'samples': len(samples), 'measured_seconds': windows / 1000,
        'cpu_core_mean_percent': [round(sum(s['cpu_percent'][c] * s['window_ms'] for s in samples) / windows, 3) for c in range(2)],
        'cpu_average_mean_percent': weighted('cpu_average_percent'),
        'cpu_average_range_percent': [min(s['cpu_average_percent'] for s in samples), max(s['cpu_average_percent'] for s in samples)],
        'lvgl_refresh_fps': round(frames * 1000 / windows, 3),
        'lvgl_refresh_fps_range': [min(s['lvgl_refresh_fps'] for s in samples), max(s['lvgl_refresh_fps'] for s in samples)],
        'render_mean_ms': round(sum(s['render_mean_ms'] * s['refresh_frames'] for s in samples) / frames, 3) if frames else 0,
        'render_max_ms': max(s['render_max_ms'] for s in samples),
        'psram_free_range_bytes': [min(s['psram_free_bytes'] for s in samples), max(s['psram_free_bytes'] for s in samples)],
        'psram_minimum_since_boot_bytes': min(s['psram_minimum_free_bytes'] for s in samples),
        'psram_smallest_largest_free_block_bytes': min(s['psram_largest_free_block_bytes'] for s in samples),
        'internal_free_range_bytes': [min(s['internal_free_bytes'] for s in samples), max(s['internal_free_bytes'] for s in samples)],
        'internal_minimum_since_boot_bytes': min(s['internal_minimum_free_bytes'] for s in samples),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=90, help='Wall-clock observation duration')
    parser.add_argument('--source', type=Path, default=Path(os.environ.get('APPDATA', '.')) / 'Quotamon/runtime-metrics.json')
    parser.add_argument('--out', type=Path, default=ROOT / 'docs/runtime-metrics.json')
    parser.add_argument('--minimum-samples', type=int, default=6)
    args = parser.parse_args()
    if not 30 <= args.seconds <= 3600 or args.minimum_samples < 1:
        parser.error('Use 30..3600 seconds and a positive sample count')
    started = datetime.now(timezone.utc)
    deadline = time.monotonic() + args.seconds
    samples, seen = [], set()
    previous_view, excluded_transitions = None, 0
    while time.monotonic() < deadline:
        try:
            if args.source.stat().st_size > 4096:
                raise ValueError('Unexpected metrics file size')
            value = json.loads(args.source.read_text(encoding='utf-8'))
            stamp = datetime.fromisoformat(value['received_at'].replace('Z', '+00:00'))
            identity = (value['received_at'], value['uptime_ms'])
            if (stamp >= started and identity not in seen and value['type'] == 'runtime_stats' and
                    value['schema'] == 1 and value['cpu_valid'] is True and value['ble_connected'] is True and
                    value['uptime_ms'] >= 15000):
                sample = {key: value[key] for key in FIELDS}
                seen.add(identity)
                view = (sample['page'], sample['visible_characters'])
                if previous_view is not None and view != previous_view:
                    previous_view = view; excluded_transitions += 1
                    continue  # This five-second window may contain the previous screen.
                previous_view = view
                samples.append(sample)
                print(f"Sample {len(samples)}: page {sample['page']}, {sample['visible_characters']} characters; "
                      f"CPU {sample['cpu_average_percent']:.2f}%, LVGL {sample['lvgl_refresh_fps']:.2f} fps, "
                      f"free PSRAM {sample['psram_free_bytes']:,} bytes", flush=True)
        except (OSError, ValueError, KeyError, TypeError):
            pass
        time.sleep(0.5)
    if len(samples) < args.minimum_samples:
        raise SystemExit(f'Only {len(samples)} fresh samples received; need {args.minimum_samples}. Check the updated bridge and firmware connection.')
    scenarios = []
    for page, visible in sorted({(s['page'], s['visible_characters']) for s in samples}):
        selected = [s for s in samples if (s['page'], s['visible_characters']) == (page, visible)]
        scenarios.append({'page': page, 'visible_characters': visible, **summarize(selected)})
    build = json.loads((ROOT / 'docs/character-animation-build.json').read_text(encoding='utf-8'))
    report = {
        'hardware_measured': True, 'firmware_sha256': build['sha256'],
        'started_utc': started.isoformat(), 'finished_utc': datetime.now(timezone.utc).isoformat(),
        'method': {
            'cpu': 'Per-core 100 * (1 - delta idle task runtime / delta ESP timer runtime); two-core mean, with FreeRTOS runtime accounting enabled',
            'fps': 'Completed LVGL refresh cycles / wall-clock window, not LCD scan rate or a maximum-FPS benchmark',
            'psram': 'heap_caps_get_info(MALLOC_CAP_SPIRAM), after XIP/framebuffers/UI/BLE allocations',
            'transport': 'Existing authenticated BLE notification path; no additional BLE client',
            'overhead': 'Five-second sampling plus task runtime accounting; measured firmware includes this instrumentation',
        },
        'excluded_view_transition_windows': excluded_transitions,
        'summary': summarize(samples), 'scenarios': scenarios, 'samples': samples,
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Saved {len(samples)} hardware samples to {args.out}', flush=True)


if __name__ == '__main__':
    main()
