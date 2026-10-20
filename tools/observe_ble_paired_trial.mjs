// Observe the installed PC bridge's ACK and paired-trial telemetry without
// changing accounts/settings or storing snapshots, peer addresses or PINs.
import { spawn } from 'node:child_process'
import { readFile, mkdir, writeFile } from 'node:fs/promises'
import { fileURLToPath } from 'node:url'
import { dirname, join, resolve } from 'node:path'
import { BleHost } from '../orca-integration/dist/src/ble-host.js'
import { Reassembler } from '../orca-integration/dist/src/ble-protocol.js'
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..')
const seconds=Number(process.argv[2]??120)
if(!Number.isFinite(seconds)||seconds<5||seconds>3600)throw Error('Expected observation duration 5..3600 seconds')
const settings=JSON.parse(await readFile(join(process.env.APPDATA,'Quotamon/settings.json'),'utf8'))
const helper=join(root,'dist/Quotamon-Portable/Quotamon.exe')
const host=new BleHost(helper,path=>spawn(path,['--ble-resource'],{windowsHide:true,stdio:'pipe'}))
const receiver=new Reassembler()
const report={started_utc:new Date().toISOString(),duration_seconds:seconds,ack_count:0,runtime_count:0,statuses:[],runtime:[],error:null}
host.on('event',event=>{
  if(event.type!=='notification'||typeof event.frame!=='string')return
  try {
    const message=receiver.push(Buffer.from(event.frame,'base64'))
    if(!message||message.kind!==2)return
    const body=JSON.parse(Buffer.from(message.body).toString('utf8'))
    if(body.type==='ack'){report.ack_count++;console.log('Installed PC snapshot ACK observed')}
    if(body.type==='runtime_stats'){
      report.runtime_count++
      const allowed=['uptime_ms','ble_connected','cpu_valid','cpu_average_percent','lvgl_refresh_fps','psram_free_bytes','internal_free_bytes']
      report.runtime.push(Object.fromEntries(allowed.filter(k=>k in body).map(k=>[k,body[k]])))
    }
    if(body.type==='paired_trial_status'&&body.schema===1&&body.capability==='paired-trial') {
      const status={received_utc:new Date().toISOString(),available:body.available===true,
        phone_bonded:body.phone_bonded===true,phone_connected:body.phone_connected===true,
        phone_changed_ms:body.phone_changed_ms,presence_sleep_enabled:body.presence_sleep_enabled===true}
      report.statuses.push(status);console.log(JSON.stringify(status))
    }
  } catch { /* Incomplete/malformed observation is never success evidence. */ }
})
try {
  await host.request('connect',{address:settings.config.address},30000)
  console.log('Observing existing PC link; no settings changes or phone scanning.')
  await new Promise(resolve=>setTimeout(resolve,seconds*1000))
} catch(error) {report.error=error.message;process.exitCode=1}
finally {
  host.dispose();report.finished_utc=new Date().toISOString()
  const folder=join(root,'.local/paired-trial-observations');await mkdir(folder,{recursive:true})
  await writeFile(join(folder,Date.now()+'.json'),JSON.stringify(report,null,2)+'\n')
  console.log(JSON.stringify({ack_count:report.ack_count,runtime_count:report.runtime_count,paired_status_count:report.statuses.length,error:report.error}))
}
