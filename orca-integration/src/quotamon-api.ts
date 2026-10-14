import type { Config } from './quota-snapshot.js'
import type { ControllerState } from './quotamon-controller.js'
export interface QuotamonApi {
  state(): Promise<ControllerState>
  scan(): Promise<void>
  connect(address: string): Promise<void>
  confirm(pin: string, accept: boolean): Promise<void>
  save(config: Config): Promise<void>
  disconnect(forget: boolean): Promise<void>
  refresh(): Promise<void>
}
