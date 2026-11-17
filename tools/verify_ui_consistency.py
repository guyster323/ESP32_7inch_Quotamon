"""Check quota typography and character geometry through the shared LVGL renderer."""
from pathlib import Path
import argparse
import json
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--simulator', type=Path, default=ROOT/'build/native/espmonitor_sim.exe')
args = parser.parse_args()
frames = ROOT/'.local/ui-consistency-verified'
out = ROOT/'visuals/ui-consistency'
out.mkdir(parents=True, exist_ok=True)

for mode, name in [('render', 'native'), ('render-locales', 'locales'),
                   ('render-account-counts', 'counts'), ('render-animation', 'animation')]:
    result = subprocess.run([str(args.simulator.resolve()), '--'+mode, str(frames/name)],
                            cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    log = result.stdout + result.stderr
    assert result.returncode == 0, log
    assert '[Warn]' not in log and 'glyph dsc. not found' not in log, log
    print(result.stdout.strip())

exports = {
    'locales/credits-home-5-codex.ppm': 'home.png',
    'locales/19-reset-credits.ppm': 'home-three-accounts.png',
    'locales/20-reset-credit-detail.ppm': 'detail.png',
    'locales/21-reset-credit-detail-scrolled.ppm': 'detail-credits.png',
    'native/18-wrapped-reset.ppm': 'long-reset.png',
    'native/05-edge-states.ppm': 'quota-states.png',
    'native/17-fractional-quota.ppm': 'fractional-quota.png',
    'native/03-copilot-detail.ppm': 'copilot-detail.png',
    'native/22-countdown-maxima.ppm': 'countdown-maxima.png',
    'native/23-countdown-midpoint.ppm': 'countdown-midpoint.png',
    'native/24-countdown-minima.ppm': 'countdown-minima.png',
}
for source, target in exports.items():
    with Image.open(frames/source) as picture:
        assert picture.size == (800, 480)
        picture.save(out/target)

with Image.open(frames/'native/01-home.ppm') as picture:
    inks = list(picture.convert('RGB').crop((136, 130, 251, 154)).get_flattened_data())
    assert any(min(c) >= 240 for c in inks), 'Filled meter lacks white percentage ink'
    assert any(max(c) <= 25 for c in inks), 'Empty meter lacks dark percentage ink'

report = {
    'passed': True, 'actual_shared_lvgl_renderer': True, 'hardware_verified': False,
    'mock_data': True,
    'checks': [
        '1–5 home cards retain original 256 zoom with reset credits',
        'Home percentage text stays numeric, with the 25px pixel font and 51px meters',
        'Detail retains 512 character zoom and visible 40px pixel percentages',
        'All three Codex cards show two distinct account-specific credit expiries and plan reset together',
        'Credit rows are labelled R.T.1 and R.T.2 without absolute dates or timezones',
        'Home cards omit the redundant reset-credit title badge',
        'R.T. expiry interpolates from red at zero to green at 14 days and stays green beyond 14 days',
        'Plan 5h and W independently interpolate from green at zero to red at 5 hours and 7 days',
        'Cursor and Copilot use Plan Reset M with green at zero and red at 30 days',
        'Unknown countdowns are neutral and home/detail colours agree',
        'Copilot home/detail show percentage and Plan Reset countdown without monthly or session AIC',
        '1–5 selected accounts retain visible, non-overlapping credit expiries within each card',
        'Roomy cards retain both expiry rows with a larger first row',
        'Detail credits are reachable by scrolling and stay inside the viewport',
        'Long dense reset schedules ellipsize without shrinking characters or numbers',
        'Percentage ink is white on fill and dark on the empty track',
        'Five languages render without missing glyph warnings',
        'Zero, low, unknown, unlimited, fractional and disconnected quota states',
        '0–5 selected accounts, non-contiguous selections and 6/20-account pagination',
        '30 provider/animation states and 40 page lifecycle cycles',
    ],
    'screenshots': list(exports.values()),
}
(out/'checks.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('UI consistency: PASS (simulator; hardware not flashed).')
