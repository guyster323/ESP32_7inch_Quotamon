"""Fetch pinned public build dependencies, retaining each upstream license."""
from pathlib import Path
import hashlib
import subprocess
import urllib.request
ROOT=Path(__file__).resolve().parents[1]
LVGL='4495f428630cc1741bd8bfd977f080e8460e8e8d'
dest=ROOT/'third_party/lvgl'
if not dest.exists():
    subprocess.run(['git','clone','--depth','1','--branch','v8.4.0','https://github.com/lvgl/lvgl.git',str(dest)],check=True)
head=subprocess.check_output(['git','-C',str(dest),'rev-parse','HEAD'],text=True).strip()
if head!=LVGL:raise SystemExit('LVGL checkout differs from the reviewed version; preserving existing files.')
json_dir=ROOT/'third_party/nlohmann';json_dir.mkdir(parents=True,exist_ok=True)
expected='aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63'
for name,remote in [('json.hpp','single_include/nlohmann/json.hpp'),('LICENSE.MIT','LICENSE.MIT')]:
    path=json_dir/name
    if not path.exists():
        with urllib.request.urlopen('https://raw.githubusercontent.com/nlohmann/json/v3.12.0/'+remote,timeout=60) as response:path.write_bytes(response.read())
    if name=='json.hpp' and hashlib.sha256(path.read_bytes()).hexdigest()!=expected:raise SystemExit('JSON header checksum mismatch')
print('LVGL 8.4.0 and nlohmann/json 3.12.0 ready; license texts retained.')
