"""Validate and stage the B-stage paired BLE firmware, preserving hardware evidence.

Run after the firmware/native builds, CTest and --render-paired-trial.
This does not upload, replace the PC package or certify Android reconnect behavior.
"""
from pathlib import Path
import argparse
from datetime import datetime, timezone
import hashlib
import json
import re
import shutil
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--stage',choices=['B','C','D'],default='B')
args=parser.parse_args()
stage_c=args.stage in ('C','D')
stage_d=args.stage=='D'
record_name='ble-paired-migration-build.json' if stage_d else 'ble-paired-presence-build.json' if stage_c else 'ble-paired-trial-build.json'
artifact_name='Quotamon-BLE-paired-migration-firmware' if stage_d else 'Quotamon-BLE-paired-presence-firmware' if stage_c else 'Quotamon-BLE-paired-trial-firmware'
evidence_prefix='stage-d' if stage_d else 'paired-presence' if stage_c else 'paired-trial'
build=ROOT/'firmware/.pio/build/waveshare-lcd7'
config=(build/'config/sdkconfig.h').read_text(encoding='utf-8')
def setting(name):
    match=re.search(r'^#define '+name+r'\s+(\S+)',config,re.M)
    return match[1] if match else None
assert setting('CONFIG_BT_NIMBLE_MAX_CONNECTIONS')=='2'
assert setting('CONFIG_BT_NIMBLE_MAX_BONDS')=='2'
assert setting('CONFIG_BT_NIMBLE_ROLE_OBSERVER') is None
assert setting('CONFIG_BT_NIMBLE_ROLE_CENTRAL') is None
assert setting('CONFIG_BT_NIMBLE_ROLE_PERIPHERAL')=='1'
assert int(setting('CONFIG_BT_CTRL_BLE_MAX_ACT'))>=3
assert setting('CONFIG_BT_CTRL_BLE_MAX_ACT_EFF')==setting('CONFIG_BT_CTRL_BLE_MAX_ACT')
for name in ['CONFIG_BT_NIMBLE_SM_SC','CONFIG_BT_NIMBLE_SM_SC_ONLY','CONFIG_BT_NIMBLE_SM_LVL','CONFIG_BT_NIMBLE_NVS_PERSIST']:
    assert setting(name)==('4' if name.endswith('LVL') else '1'),name
source=(ROOT/'firmware/main/ble_transport.cpp').read_text(encoding='utf-8')
assert 'ble_gap_disc(' not in source and 'ble_store_clear(' not in source
if stage_c:
    assert 'PairedPresenceTracker pairedPresence' in (ROOT/'firmware/main/main.cpp').read_text(encoding='utf-8')
    assert 'pairedState.shouldSleep()' in (ROOT/'firmware/main/main.cpp').read_text(encoding='utf-8')
    assert (ROOT/'native/paired_presence.h').is_file()
    prerequisite=json.loads((ROOT/'docs/ble-paired-trial-hardware.json').read_text(encoding='utf-8'))
    assert prerequisite['stage_b_connection_gate_passed'],'B connection gate must pass before C'
else:assert 'bool presenceShouldSleep() {return false;}' in source
assert source.count('ble_gatts_notify_custom(')==1
assert 'ble_gatts_notify_custom(s->key.handle,controlHandle,buffer)' in source
binary=(build/'firmware.bin').read_bytes()
assert (b'paired_presence_status' if stage_d else b'paired_trial_status') in binary
if stage_d:
    assert b'presence_scan' not in binary and b'presence_status' not in binary.replace(b'paired_presence_status',b'')
    assert 'presence_address' not in (ROOT/'native/snapshot_json.h').read_text(encoding='utf-8')
assert (b'Paired presence: 2 connections' if stage_c else b'Paired trial: 2 connections') in binary
assert b'HID consumer trial: no input transmission' in binary
inputs=list((ROOT/'firmware/main').glob('*.[ch]*'))+list((ROOT/'native').glob('*.h'))
inputs+=[ROOT/'native/ui.cpp',ROOT/'firmware/sdkconfig.defaults',ROOT/'firmware/sdkconfig.waveshare-lcd7']
inputs+=list((ROOT/'native/generated').glob('*.c'))
inputs+=list((ROOT/'native/generated').glob('*.h'))+list((ROOT/'protocol').glob('*.h'))
inputs+=list((ROOT/'firmware/vendor').glob('*.[ch]'))
inputs+=[ROOT/'native/sprite_decoder.cpp',ROOT/'firmware/lv_conf.h',ROOT/'firmware/main/CMakeLists.txt',
         ROOT/'firmware/CMakeLists.txt',ROOT/'firmware/platformio.ini',ROOT/'firmware/partitions.csv']
assert (build/'firmware.bin').stat().st_mtime>=max(p.stat().st_mtime for p in inputs),'Stale firmware'
for obj,src in [('main/ble_transport.cpp.o','firmware/main/ble_transport.cpp'),
                ('main/main.cpp.o','firmware/main/main.cpp'),('ui.cpp.o','native/ui.cpp')]:
    assert (build/obj).stat().st_mtime>=(ROOT/src).stat().st_mtime,f'Stale compiled object: {src}'
partition=0x7f0000
assert len(binary)<=partition
mapping=(build/'quotamon_ble.map').read_text(encoding='utf-8')
page=int(setting('CONFIG_MMU_PAGE_SIZE'),0)
def reserved(segment):
    def symbol(name):return int(re.search(r'0x([0-9a-f]+)\s+'+name+r' =',mapping)[1],16)
    a,b=symbol(f'_{segment}_reserved_start'),symbol(f'_{segment}_reserved_end')
    return ((b+page-1)//page-a//page)*page
instruction,rodata=reserved('instruction'),reserved('rodata')
remaining=8*1024*1024-instruction-rodata-2*800*480*2-16*1024
assert remaining>=512*1024,'Insufficient conservative PSRAM reserve'
checks=json.loads((ROOT/f'.local/{evidence_prefix}-lcd/checks.json').read_text(encoding='utf-8'))
assert checks['passed'] and not checks['hardware_tested']
if stage_c:assert checks['paired_presence_controls'] and checks['presence_state_screens']
tests=ET.parse(ROOT/f'.local/{evidence_prefix}-ctest.xml').getroot()
suites=[tests] if tests.tag=='testsuite' else list(tests)
assert sum(int(t.get('tests','0')) for t in suites)>=(8 if stage_c else 7)
assert all(int(t.get('failures','0'))==0 and int(t.get('errors','0'))==0 for t in suites)
sha=lambda data:hashlib.sha256(data).hexdigest()
report={
    'status':f'stage-{args.stage.lower()}-built-and-simulated-verified-awaiting-hardware',
    'built_utc':datetime.now(timezone.utc).isoformat(),
    'stage':args.stage,'sha256':sha(binary),'binary_bytes':len(binary),
    'flash_partition_bytes':partition,'flash_partition_free_bytes':partition-len(binary),
    'host_connections':2,'host_bonds':2,'controller_activities':int(setting('CONFIG_BT_CTRL_BLE_MAX_ACT')),
    'observer_enabled':False,'presence_sleep_enabled':stage_c,
    'phone_profile':'hid-consumer-trial','automatic_input_enabled':False,
    'hid_service_uuid':'1812','hid_appearance':'03c0','hid_report_id':1,
    'hid_report_bytes':1,'keyboard_mouse_collections':False,
    'battery_read_behavior':'application-error-0x80-measurement-unavailable',
    'pnp_identity':'experimental-unassigned-ffff-0001',
    'bluetooth_profile_conformance_certified':False,
    'psram_assumed_bytes':8*1024*1024,'psram_instruction_reserved_bytes':instruction,
    'psram_rodata_reserved_bytes':rodata,'estimated_psram_remaining_before_other_dynamic_allocations':remaining,
    'native_tests':8 if stage_c else 7,'actual_transport_simulated':True,'lcd_languages':checks['languages'],
    'hardware_uploaded':False,'pc_package_replaced':False,'hardware_gate_passed':False,
    'hardware_checks':{name:'unverified' for name in ['pc_bond_preserved','pc_snapshot_ack',
        'native_phone_pairing','authenticated_phone_identity','locked_30_minutes','automatic_return_reconnect']},
    'next_step':'C hardware: 60s absence, automatic wake, touch/schedule and PC-independent operation' if stage_c else 'docs/BLE_PAIRED_PRESENCE_PLAN.md section B; validate Fold7 before stages C/D/E',
    'inputs':{str(p.relative_to(ROOT)).replace('\\','/'):sha(p.read_bytes()) for p in inputs},
}
if stage_c:
    report.update(grace_seconds=60,touch_wake_seconds=300,physical_return_required_cycles=1,
        stage_b_connection_gate_passed=True,settings_owner='board NVS paired_on; legacy PC scan settings cannot control it')
if stage_d:
    web_checks=json.loads((ROOT/'.local/stage-d-web/checks.json').read_text(encoding='utf-8'))
    assert web_checks['passed'] and not web_checks['hardware_tested']
    package=ROOT/'dist/Quotamon-Paired-Portable'
    package_manifest=json.loads((package/'build-manifest.json').read_text(encoding='utf-8-sig'))
    assert package_manifest['version']=='0.3.0'
    for row in package_manifest['files']:
        path=(package/row['path']).resolve()
        assert path.is_relative_to(package.resolve()) and path.is_file()
        content=path.read_bytes()
        assert len(content)==row['bytes'] and sha(content)==row['sha256'],row['path']
    for name in ['app.js','index.html','locales.js','style.css']:
        assert (package/'web'/name).read_bytes()==(ROOT/'portable/web'/name).read_bytes(),name
    bridge=(package/'bridge.cjs').read_bytes()
    assert b'paired_presence_status' in bridge and b'presence_scan' not in bridge
    archive=package.with_name(package.name+'-win-x64.zip')
    assert archive.is_file()
    report.update(status_schema=2,capability='paired-presence',legacy_scan_removed=True,
        peer_id='board-generated random 16-byte display identifier; never an authorization credential',
        portable_version=package_manifest['version'],portable_archive_sha256=sha(archive.read_bytes()),
        portable_executable_sha256=sha((package/'Quotamon.exe').read_bytes()),
        portable_bridge_sha256=sha(bridge),portable_manifest_verified=True,web_languages=web_checks['languages'],
        next_step='D/E hardware: preserved bonds, PC ACK and read-only status, local settings persistence, schedule and PC-independent operation')
record=ROOT/'docs'/record_name
if record.exists():
    previous=json.loads(record.read_text(encoding='utf-8'))
    if previous.get('sha256')==report['sha256']:
        for name,value in previous.items():
            if name not in report or name in ['status','built_utc','hardware_uploaded','hardware_checks','hardware_gate_passed','pc_package_replaced']:
                report[name]=value
out=ROOT/'dist'/artifact_name;out.mkdir(exist_ok=True)
for name in ['firmware.bin','bootloader.bin','partitions.bin']:shutil.copyfile(build/name,out/name)
shutil.copyfile(ROOT/'THIRD_PARTY_NOTICES.md',out/'THIRD_PARTY_NOTICES.md')
licenses=out/'licenses';licenses.mkdir(exist_ok=True)
for name in ['NotoSansKR-OFL.txt','NotoSansSC-OFL.txt','F108pro-LICENSE.txt']:shutil.copyfile(ROOT/'native/generated'/name,licenses/name)
for path in [record,out/'build-verification.json']:path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(f'Paired trial: {len(binary):,}/{partition:,} bytes; estimated PSRAM reserve {remaining:,} bytes; hardware gate pending.')
