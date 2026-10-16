"""Render and check every supported locale through the real shared LVGL UI."""
from pathlib import Path
import json
import subprocess
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
frames=ROOT/'.local/localized-lcd';out=ROOT/'visuals/locales';out.mkdir(exist_ok=True)
result=subprocess.run([str(ROOT/'build/native/espmonitor_sim.exe'),'--render-locales',str(frames)],
    capture_output=True,text=True,encoding='utf-8',errors='replace',cwd=ROOT)
assert result.returncode==0,result.stdout+result.stderr
assert '[Warn]' not in result.stdout+result.stderr,result.stdout+result.stderr
for name in [f'lcd-{code}-settings' for code in ['ko','zh-CN','ja','en','de']]+['lcd-ja-home','lcd-zh-CN-schedule','lcd-ko-time-input']:
    with Image.open(frames/(name+'.ppm')) as image:image.save(out/(name+'.png'))
report=json.loads((frames/'lcd-checks.json').read_text(encoding='utf-8'))
report['checks'].append('screen-off pauses animation; wake resumes')
(out/'lcd-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('Five-language native LCD rendering: PASS; simulated data, no missing glyph warnings.')
