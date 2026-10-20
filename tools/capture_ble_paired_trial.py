"""Capture BLE link timing privately without toggling DTR/RTS or resetting the board."""
from pathlib import Path
from datetime import datetime
import argparse
import re
import time
import serial

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True)
    parser.add_argument('--seconds',type=int,default=180)
    args=parser.parse_args()
    assert 5<=args.seconds<=7200
    root=Path(__file__).resolve().parents[1]
    folder=root/'.local/paired-trial-console';folder.mkdir(exist_ok=True)
    path=folder/(datetime.now().strftime('%Y%m%d-%H%M%S')+'.log')
    handle=None;pending=b'';deadline=time.monotonic()+args.seconds
    print('Passive BLE console capture started; logs remain private.',flush=True)
    with path.open('ab') as log:
        while time.monotonic()<deadline:
            try:
                if handle is None:
                    handle=serial.Serial();handle.port=args.port;handle.baudrate=115200;handle.timeout=0.25
                    handle.dtr=False;handle.rts=False;handle.open()
                data=handle.read(max(1,handle.in_waiting))
                if not data:continue
                log.write(data);log.flush();pending+=data
                while b'\n' in pending:
                    line,pending=pending.split(b'\n',1)
                    text=line.decode('utf-8',errors='replace').rstrip()
                    if any(tag in text for tag in ['quotamon-ble','quotamon-ui','Guru Meditation','abort()','Rebooting']):
                        text=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[peer]',text)
                        print(text,flush=True)
            except serial.SerialException:
                if handle:handle.close()
                handle=None;time.sleep(0.25)
        if handle:handle.close()
    print('Passive capture complete: '+str(path.relative_to(root)),flush=True)
if __name__=='__main__':main()
