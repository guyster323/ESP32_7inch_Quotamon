"""Check the target's flash and conservative PSRAM budget from the linked image.

This is a link-time estimate, not runtime heap/FPS measurement. Target assumptions:
8 MiB PSRAM, two 800x480 RGB565 framebuffers, 16 KiB external LVGL task stack.
"""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[1]
build = ROOT / 'firmware/.pio/build/waveshare-lcd7'
mapping = (build / 'quotamon_ble.map').read_text(encoding='utf-8')
config = (ROOT / 'firmware/sdkconfig.waveshare-lcd7').read_text(encoding='utf-8')
assert 'CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y' in config
assert 'CONFIG_SPIRAM_RODATA=y' in config
assert 'CONFIG_LV_IMG_CACHE_DEF_SIZE=0' in config
page = int(re.search(r'^CONFIG_MMU_PAGE_SIZE=(\S+)', config, re.M)[1], 0)


def address(symbol):
    return int(re.search(r'0x([0-9a-f]+)\s+' + symbol + r' =', mapping)[1], 16)


def reserved(segment):
    start, end = address(f'_{segment}_reserved_start'), address(f'_{segment}_reserved_end')
    return ((end + page - 1) // page - start // page) * page


instruction, rodata = reserved('instruction'), reserved('rodata')
psram, buffers, stack = 8 * 1024 * 1024, 2 * 800 * 480 * 2, 16 * 1024
remaining = psram - instruction - rodata - buffers - stack
binary = (build / 'firmware.bin').read_bytes()
partition = 0x7F0000
assert len(binary) <= partition, 'Firmware exceeds the factory app partition'
assert remaining >= 512 * 1024, f'Less than 512 KiB remains after XIP, LCD buffers and LVGL stack: {remaining}'
manifest = json.loads((ROOT / 'native/generated/asset-manifest.json').read_text(encoding='utf-8'))
report = {
    'status': 'built-and-native-verified-not-flashed',
    'built_utc': datetime.now(timezone.utc).isoformat(),
    'target': 'Waveshare ESP32-S3-Touch-LCD-7 Rev1.2',
    'binary_bytes': len(binary), 'sha256': hashlib.sha256(binary).hexdigest(),
    'flash_partition_bytes': partition, 'flash_partition_free_bytes': partition - len(binary),
    'compressed_sprite_bytes': manifest['animation']['raster_bytes'],
    'previous_static_sprite_bytes': 30 * 88 * 108 * 3,
    'decoder_scratch_bytes': 88 * 108 * 3,
    'psram_assumed_bytes': psram, 'mmu_page_bytes': page,
    'psram_instruction_reserved_bytes': instruction, 'psram_rodata_reserved_bytes': rodata,
    'rgb_framebuffers_bytes': buffers, 'lvgl_external_task_stack_bytes': stack,
    'estimated_psram_remaining_before_other_dynamic_allocations': remaining,
    'minimum_estimated_dynamic_reserve_bytes': 512 * 1024,
    'hardware_uploaded': False, 'hardware_animation_tested': False,
    'runtime_heap_fps_cpu_measured': False,
    'native_checks': 'visuals/native/animation-checks.json',
}
record_path = ROOT / 'docs/character-animation-build.json'
if record_path.exists():
    previous = json.loads(record_path.read_text(encoding='utf-8'))
    # Rechecking an identical image must retain its observed deployment state.
    if previous.get('sha256') == report['sha256']:
        for key, value in previous.items():
            if key not in report or key.startswith('hardware_') or key in ('status', 'runtime_heap_fps_cpu_measured'):
                report[key] = value
record_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print(f'Flash {len(binary):,}/{partition:,} bytes; estimated PSRAM reserve {remaining:,} bytes before other dynamic allocations.')
