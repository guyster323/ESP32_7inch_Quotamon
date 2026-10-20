"""Apply only the B-stage app after manual BOOT preparation; preserve PC/NVS.

Usage: .venv/Scripts/python.exe tools/flash_ble_paired_trial.py --port COM5
Does not reset the board, erase flash, or change the installed PC package.
Private flash backups and serial logs are written only below .local/.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import hashlib
import json
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True)
    parser.add_argument('--prepare-only',action='store_true',help='Back up the installed app/NVS while the trial builds')
    parser.add_argument('--resume-backup',type=Path,help='Reuse a verified private backup, rechecking current flash first')
    parser.add_argument('--baseline-build',type=Path,help='Verified build report for the currently installed app; defaults to the original presence build')
    parser.add_argument('--stage',choices=['B','C','D'],default='B',help='Select the separately verified B trial, C screen-control or D migration artifact')
    args=parser.parse_args()
    assert not(args.prepare_only and args.resume_backup)
    stage_c=args.stage in ('C','D')
    stage_d=args.stage=='D'
    current_path=ROOT/'docs'/('ble-paired-migration-build.json' if stage_d else 'ble-paired-presence-build.json' if stage_c else 'ble-paired-trial-build.json')
    current=None if args.prepare_only else json.loads(current_path.read_text(encoding='utf-8'))
    baseline=(args.baseline_build or ROOT/'docs/ble-presence-build.json').resolve()
    assert baseline.is_relative_to(ROOT) and baseline.is_file(),'Baseline report must be inside this workspace'
    previous=json.loads(baseline.read_text(encoding='utf-8'))
    image=ROOT/'dist'/('Quotamon-BLE-paired-migration-firmware' if stage_d else 'Quotamon-BLE-paired-presence-firmware' if stage_c else 'Quotamon-BLE-paired-trial-firmware')/'firmware.bin'
    sha=lambda b:hashlib.sha256(b).hexdigest()
    if current:
        assert sha(image.read_bytes())==current['sha256'],'Staged image hash mismatch'
        assert current['stage']==args.stage and current['presence_sleep_enabled']==stage_c
        if stage_c:assert current['stage_b_connection_gate_passed']
        assert len(image.read_bytes())<=0x7f0000
    tool=ROOT/'.tools/platformio/packages/tool-esptoolpy/esptool.py'
    assert tool.is_file()
    base=[sys.executable,str(tool),'--chip','esp32s3','--port',args.port,'--baud','460800',
          '--before','no_reset','--after','no_reset']
    if args.resume_backup:
        folder=args.resume_backup.resolve()
        assert folder.is_relative_to((ROOT/'.local').resolve()) and folder.is_dir(),'Backup must be inside this workspace .local'
    else:
        folder=ROOT/'.local'/('board-ble-paired-trial-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
        folder.mkdir(exist_ok=False)
    def run(name,*command):
        result=subprocess.run(base+list(command),cwd=ROOT,capture_output=True,text=True,
                              encoding='utf-8',errors='replace',timeout=300)
        (folder/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
        if result.returncode:raise RuntimeError(f'{name} failed; see private log {folder/name}.log')
        print(name+': PASS',flush=True)
        return result.stdout
    info=run('flash-id','flash_id')
    assert 'ESP32-S3' in info and 'Detected flash size: 16MB' in info,'Unexpected board/flash'
    partition_file=(ROOT/'dist/Quotamon-BLE-presence-firmware/partitions.bin') if args.prepare_only else image.parent/'partitions.bin'
    run('verify-partitions','verify_flash','0x8000',str(partition_file))
    before=folder/'boot-nvs-before.bin'
    backup=folder/'previous-app.bin'
    if args.resume_backup:
        assert before.stat().st_size==0x10000 and backup.stat().st_size==previous['binary_bytes']
        assert sha(backup.read_bytes())==previous['sha256'],'Previous backup hash mismatch'
        run('reverify-baseline-app','verify_flash','0x10000',str(backup))
        recheck=folder/'boot-nvs-resume.bin'
        run('reverify-boot-nvs','read_flash','0x0','0x10000',str(recheck))
        assert recheck.read_bytes()==before.read_bytes(),'Boot/NVS changed since backup; stop and inspect'
    else:
        run('backup-boot-nvs','read_flash','0x0','0x10000',str(before))
        run('backup-previous-app','read_flash','0x10000',str(previous['binary_bytes']),str(backup))
    assert sha(backup.read_bytes())==previous['sha256'],'Current app differs from expected baseline; no write performed'
    if args.prepare_only:
        (folder/'baseline-verification.json').write_text(json.dumps({'sha256':previous['sha256'],
            'bytes':previous['binary_bytes'],'verified_utc':datetime.now(timezone.utc).isoformat()},indent=2)+'\n',encoding='utf-8')
        print('Private backup verified. Keep download mode for the trial upload: '+str(folder.relative_to(ROOT)),flush=True)
        return
    run('write-app','write_flash','--flash_size','keep','--no-progress','0x10000',str(image))
    run('verify-new-app','verify_flash','0x10000',str(image))
    after=folder/'boot-nvs-after.bin'
    run('read-boot-nvs-after','read_flash','0x0','0x10000',str(after))
    assert before.read_bytes()==after.read_bytes(),'Boot/partition/NVS changed'
    current.update(status=f'stage-{args.stage.lower()}-flashed-readback-verified-boot-pending',hardware_uploaded=True,
        uploaded_utc=datetime.now(timezone.utc).isoformat(),application_readback_verified=True,
        bootloader_partitions_nvs_unchanged=True,previous_app_backup_verified=True,
        previous_app_sha256=previous['sha256'])
    for path in [current_path,image.parent/'build-verification.json',folder/'result.json']:
        path.write_text(json.dumps(current,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Application verified. PC package unchanged. Cold boot and Fold7 tests are still required.')
if __name__=='__main__':main()
