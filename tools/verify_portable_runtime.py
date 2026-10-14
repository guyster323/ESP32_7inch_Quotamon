"""Read-only live checks plus a temporary local settings roundtrip. No pairing/flash."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile,time,urllib.request,winreg

root=Path(__file__).resolve().parents[1]
package=root/'dist'/'Quotamon-Portable'
def startup_value():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,r'Software\Microsoft\Windows\CurrentVersion\Run') as key:
            return winreg.QueryValueEx(key,'Quotamon')[0]
    except FileNotFoundError:return None
def request(base,key,path,value=None):
    req=urllib.request.Request(base+'/api/'+path,
        data=None if value is None else json.dumps(value).encode(),
        headers={'X-Quotamon-Key':key,'Origin':base,'Content-Type':'application/json'})
    with urllib.request.urlopen(req,timeout=10) as response:return json.load(response)
before=startup_value()
orca_bundle=Path(os.environ['LOCALAPPDATA'])/'Programs'/'orca'/'resources'/'app.asar'
binary_before=hashlib.sha256(orca_bundle.read_bytes()).hexdigest() if orca_bundle.exists() else None
with tempfile.TemporaryDirectory(prefix='quotamon-verify-') as temporary:
    config=Path(temporary)/'settings.json'
    child=subprocess.Popen([str(package/'node.exe'),str(package/'bridge.cjs'),
        '--root',str(package),'--helper',str(package/'Quotamon.exe'),'--config',str(config)],
        stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,
        text=True,encoding='utf-8',creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        ready=json.loads(child.stdout.readline())
        assert ready['type']=='ready'
        base,key=ready['url'].split('/#')
        state=None
        for _ in range(40):
            state=request(base,key,'state')
            if state['source_available'] and any(a['provider']=='copilot' for a in state['accounts']):break
            time.sleep(1)
        assert state and state['source_available'],'Running Orca is required for this live check'
        assert state['radio_available'] is True,'BLE adapter probe failed'
        configuration=state['config']
        configuration['accounts']=[dict(id=a['id'],alias='',plan='',enabled=True) for a in state['accounts']]
        request(base,key,'save',configuration)
        saved=json.loads(config.read_text(encoding='utf-8'))
        assert saved['config']['revision']==1
        assert all(a['enabled'] for a in saved['config']['accounts'])
        for sensitive in ['authToken','access_token','refresh_token','email','@']:
            assert sensitive not in config.read_text(encoding='utf-8')
        request(base,key,'scan',{})
        time.sleep(21)
        state=request(base,key,'state')
        summary={'portable_backend_live':True,'orca_available':True,'providers':{},
            'ble_adapter':state['radio_available'],'discovered_devices':len(state['devices']),
            'temporary_settings_roundtrip':True,'hardware_transfer_verified':False}
        for a in state['accounts']:
            counts=summary['providers'].setdefault(a['provider'],dict(accounts=0,ready=0,stale=0,unknown=0))
            counts['accounts']+=1;counts[a['state']]+=1
        request(base,key,'preferences',dict(copilot=False,startup=False))
        state=request(base,key,'state')
        assert all(a['provider']!='copilot' for a in state['accounts'])
        request(base,key,'preferences',dict(copilot=True,startup=False))
        state=request(base,key,'state')
        assert any(a['provider']=='copilot' for a in state['accounts'])
        summary['copilot_toggle_roundtrip']=True
        if binary_before is not None:
            assert hashlib.sha256(orca_bundle.read_bytes()).hexdigest()==binary_before
            summary['orca_application_unchanged']=True
        print(json.dumps(summary,ensure_ascii=False,indent=2))
    finally:
        child.stdin.close()
        try:child.wait(timeout=10)
        except subprocess.TimeoutExpired:child.kill();child.wait()
assert startup_value()==before,'The verification must not change startup registration'
print('Launcher startup registration untouched; bridge stopped.')
