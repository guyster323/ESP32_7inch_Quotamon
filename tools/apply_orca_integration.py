"""Apply the reviewed patch and integration modules to an Orca SOURCE checkout, never an installed app."""
from pathlib import Path
import argparse
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
PIN='3f81541188b435aabc822aaf36c6f23a70e1a783'
parser=argparse.ArgumentParser()
parser.add_argument('--orca-root',type=Path,required=True)
parser.add_argument('--allow-other-commit',action='store_true')
args=parser.parse_args()
target=args.orca_root.resolve()
head=subprocess.check_output(['git','-C',str(target),'rev-parse','HEAD'],text=True).strip()
if head!=PIN and not args.allow_other_commit:parser.error('Orca version differs from reviewed source; use --allow-other-commit only after review.')
patch=ROOT/'orca-integration/patches/orca-1.4.214.patch'
subprocess.run(['git','-C',str(target),'apply','--check',str(patch)],check=True)
files={p:target/'src/main/quotamon'/p.name for p in (ROOT/'orca-integration/src').glob('*.ts')}
files.update({ROOT/'orca-integration/orca/quotamon-entry.ts':target/'src/main/quotamon/quotamon-entry.ts',
              ROOT/'orca-integration/orca/quotamon-preload.ts':target/'src/preload/quotamon-preload.ts',
              ROOT/'orca-integration/orca/QuotamonSettingsPane.tsx':target/'src/renderer/src/components/settings/QuotamonSettingsPane.tsx'})
for source,dest in files.items():
    if dest.exists() and dest.read_bytes()!=source.read_bytes():parser.error(f'Refusing to overwrite changed source: {dest}')
subprocess.run(['git','-C',str(target),'apply',str(patch)],check=True)
for source,dest in files.items():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest)
resource=ROOT/'dist/orca-resource/Quotamon.Ble.exe'
if resource.exists():
    dest=target/'resources/quotamon';dest.mkdir(parents=True,exist_ok=True);shutil.copyfile(resource,dest/resource.name)
else:print('Build the Windows resource, then copy dist/orca-resource/Quotamon.Ble.exe to resources/quotamon/.')
print('Source integration applied. Review/build Orca before use; installed Orca was not changed.')
