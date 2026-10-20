// Observe only the installed board's advertising; never scan for phone presence.
// A paired-device cache entry is counted separately from actual radio packets.
import { spawn } from 'node:child_process'
import { readFile, mkdir, writeFile } from 'node:fs/promises'
import { dirname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { BleHost } from '../orca-integration/dist/src/ble-host.js'
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..')
const seconds=Number(process.argv[2]??22)
if(!Number.isFinite(seconds)||seconds<5||seconds>25)throw Error('Expected 5..25 seconds')
const settings=JSON.parse(await readFile(join(process.env.APPDATA,'Quotamon/settings.json'),'utf8'))
const expected=settings.config.address
if(typeof expected!=='string'||!/^\d{1,20}$/.test(expected))throw Error('No saved board')
const host=new BleHost(join(root,'dist/Quotamon-Portable/Quotamon.exe'),p=>spawn(p,['--ble-resource'],{windowsHide:true,stdio:'pipe'}))
const report={started_utc:new Date().toISOString(),duration_seconds:seconds,
  live_advertisement_count:0,cached_device_count:0,name_observed:false,
  rssi_min:null,rssi_max:null,error:null}
host.on('event',event=>{
  if(event.type!=='device'||event.address!==expected)return
  if(event.paired===true){report.cached_device_count++;return}
  if(typeof event.rssi!=='number')return
  report.live_advertisement_count++
  report.name_observed ||= event.name==='Quotamon'
  report.rssi_min=report.rssi_min===null?event.rssi:Math.min(report.rssi_min,event.rssi)
  report.rssi_max=report.rssi_max===null?event.rssi:Math.max(report.rssi_max,event.rssi)
})
try {
  await host.request('scan')
  await new Promise(resolve=>setTimeout(resolve,seconds*1000))
} catch(error){report.error=error.message;process.exitCode=1}
finally {
  host.dispose();report.finished_utc=new Date().toISOString()
  const folder=join(root,'.local/paired-trial-observations')
  await mkdir(folder,{recursive:true})
  await writeFile(join(folder,Date.now()+'-advertising.json'),JSON.stringify(report,null,2)+'\n')
  console.log(JSON.stringify(report))
}
