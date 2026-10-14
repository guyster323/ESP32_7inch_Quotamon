import { ipcRenderer } from 'electron'
import type { QuotamonApi } from '../main/quotamon/quotamon-api'
export const quotamonApi: QuotamonApi = {
  state: () => ipcRenderer.invoke('quotamon:state'),
  scan: () => ipcRenderer.invoke('quotamon:scan'),
  connect: (address) => ipcRenderer.invoke('quotamon:connect', address),
  confirm: (pin, accept) => ipcRenderer.invoke('quotamon:confirm', pin, accept),
  save: (config) => ipcRenderer.invoke('quotamon:save', config),
  disconnect: (forget) => ipcRenderer.invoke('quotamon:disconnect', forget),
  refresh: () => ipcRenderer.invoke('quotamon:refresh')
}
