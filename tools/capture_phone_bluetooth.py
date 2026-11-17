"""Read Bluetooth-process logs over an already authorized Android USB connection.

No settings changes, app installs, pairing deletion, or log-buffer clearing.
Raw Bluetooth logs may contain device addresses; keep them under private .local.
"""
import argparse
from datetime import datetime
from pathlib import Path
import json
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', type=Path, default=ROOT/'.tools/android/platform-tools/adb.exe')
    parser.add_argument('--seconds', type=int, default=300)
    args = parser.parse_args()
    if not args.adb.is_file() or not 30 <= args.seconds <= 7200:
        parser.error('Expected an existing adb executable and duration30..7200 seconds')

    def adb(*command):
        return subprocess.run([str(args.adb), *command], capture_output=True, check=True,
                              encoding='utf-8', errors='replace', timeout=20).stdout.strip()

    devices = [line.split() for line in adb('devices').splitlines()[1:] if line.strip()]
    authorized = [row[0] for row in devices if len(row) >= 2 and row[1] == 'device']
    if len(authorized) != 1 or len(devices) != 1:
        raise SystemExit('Connect exactly one phone and accept its USB debugging prompt; no phone data read.')
    serial = authorized[0]
    selected = ['-s', serial]
    pids = adb(*selected, 'shell', 'pidof', 'com.android.bluetooth').split()
    if len(pids) != 1 or not pids[0].isdigit():
        raise SystemExit('Bluetooth process unavailable; no broad phone log capture performed.')
    folder = ROOT/'.local'/('phone-bluetooth-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir()
    info = {name: adb(*selected, 'shell', 'getprop', prop) for name, prop in [
        ('model', 'ro.product.model'), ('android_release', 'ro.build.version.release'),
        ('build_incremental', 'ro.build.version.incremental')]}
    info.update(duration_seconds=args.seconds, bluetooth_pid=int(pids[0]),
                settings_changed=False, raw_logs_private=True)
    print('Capturing Bluetooth-process logs only; no settings changes.', flush=True)
    with (folder/'bluetooth.log').open('wb') as output:
        proc = subprocess.Popen([str(args.adb), *selected, 'logcat', '--pid='+pids[0],
                                 '-T', '1', '-v', 'threadtime'], stdout=output, stderr=subprocess.PIPE)
        timer = threading.Timer(args.seconds, proc.terminate)
        timer.start()
        try:
            _, errors = proc.communicate()
        finally:
            timer.cancel()
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=10)
        (folder/'capture-errors.log').write_bytes(errors or b'')
        info.update(bytes=(folder/'bluetooth.log').stat().st_size,
                    exit_code=proc.returncode, phone_bluetooth_pid_changed=False)
    try:
        info['phone_bluetooth_pid_changed'] = adb(*selected, 'shell', 'pidof', 'com.android.bluetooth') != pids[0]
    except subprocess.SubprocessError:
        info['phone_connection_lost'] = True
    (folder/'capture.json').write_text(json.dumps(info, indent=2)+'\n', encoding='utf-8')
    print('Private capture saved: '+str(folder.relative_to(ROOT)), flush=True)


if __name__ == '__main__':
    main()
