import { useEffect, useState } from 'react'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Checkbox } from '@/components/ui/checkbox'
import type {
  Config,
  Selection
} from '../../../../main/quotamon/quota-snapshot'
import type { ControllerState } from '../../../../main/quotamon/quotamon-controller'
export function QuotamonSettingsPane(): React.JSX.Element {
  const [state, setState] = useState<ControllerState | null>(null),
    [draft, setDraft] = useState<Config | null>(null)
  const [error, setError] = useState(''),
    [loadError, setLoadError] = useState(''),
    [busy, setBusy] = useState(false),
    [dirty, setDirty] = useState(false)
  useEffect(() => {
    let alive = true
    const load = async (): Promise<void> => {
      try {
        const next = await window.api.quotamon.state()
        if (alive) {
          setState(next)
          setLoadError('')
          setDraft((d) => d ?? next.config)
        }
      } catch {
        if (alive)
          setLoadError(
            'Quotamon BLE 리소스가 포함된 Windows Orca 빌드가 필요합니다.'
          )
      }
    }
    void load()
    const timer = setInterval(() => void load(), 1000)
    return () => {
      alive = false
      clearInterval(timer)
    }
  }, [])
  useEffect(() => {
    if (state && !dirty) setDraft(state.config)
  }, [state, dirty])
  const run = async (action: () => Promise<void>): Promise<void> => {
    setBusy(true)
    setError('')
    try {
      await action()
    } catch {
      setError(
        '작업을 완료하지 못했습니다. 연결 상태와 설정 변경 여부를 확인하세요.'
      )
    } finally {
      setBusy(false)
    }
  }
  const selection = (id: string): Selection =>
    draft?.accounts.find((a) => a.id === id) ?? {
      id,
      alias: '',
      plan: '',
      enabled: false
    }
  const change = (id: string, update: Partial<Selection>): void => {
    if (!draft) return
    const row = { ...selection(id), ...update }
    setDraft({
      ...draft,
      accounts: draft.accounts.some((a) => a.id === id)
        ? draft.accounts.map((a) => (a.id === id ? row : a))
        : [...draft.accounts, row]
    })
    setDirty(true)
  }
  const order = (id: string, delta: number): void => {
    if (!draft || !state) return
    const accounts = ordered.map((a) => selection(a.id)),
      index = accounts.findIndex((a) => a.id === id),
      target = index + delta
    if (index < 0 || target < 0 || target >= accounts.length) return
    ;[accounts[index], accounts[target]] = [accounts[target], accounts[index]]
    setDraft({ ...draft, accounts })
    setDirty(true)
  }
  const display = (
    key: keyof Config['display'],
    value: number | boolean
  ): void => {
    if (draft) {
      setDraft({ ...draft, display: { ...draft.display, [key]: value } })
      setDirty(true)
    }
  }
  const ordered =
    state?.accounts.slice().sort((a, b) => {
      const ids = draft?.accounts.map((s) => s.id) ?? []
      const rank = (id: string): number =>
        ids.includes(id)
          ? ids.indexOf(id)
          : ids.length + (state?.accounts.findIndex((a) => a.id === id) ?? 0)
      return rank(a.id) - rank(b.id)
    }) ?? []
  return (
    <section
      className="space-y-4 rounded-lg border border-border bg-card p-4"
      aria-label="Quotamon"
    >
      <div className="space-y-1">
        <h3 className="text-sm font-medium">Quotamon · BLE 구독 모니터</h3>
        <p className="text-xs text-muted-foreground">
          디스플레이 연결 → 표시할 계정 선택 → 저장. 별도 PC 앱 없이 Orca가
          실행되는 동안 동기화합니다.
        </p>
      </div>
      {(error || loadError || state?.error) && (
        <p role="alert" className="text-xs text-destructive">
          {error || loadError || state?.error}
        </p>
      )}
      <div className="flex flex-wrap items-center gap-2">
        <span className="text-xs">
          {state?.connected ? '연결됨' : '연결 대기'}
        </span>
        <Button
          variant="outline"
          disabled={busy}
          onClick={() => void run(() => window.api.quotamon.scan())}
        >
          디스플레이 찾기
        </Button>
        <Button
          variant="outline"
          disabled={busy || !state?.config.address}
          onClick={() => void run(() => window.api.quotamon.disconnect(true))}
        >
          자동 연결 해제
        </Button>
      </div>
      {state?.devices.map((device) => (
        <div
          key={device.address}
          className="flex items-center justify-between gap-2"
        >
          <span className="text-xs">
            {device.name} · {device.rssi} dBm
          </span>
          <Button
            disabled={busy}
            onClick={() =>
              void run(() => window.api.quotamon.connect(device.address))
            }
          >
            연결
          </Button>
        </div>
      ))}
      {state?.pairingPin && (
        <div
          className="space-y-2 rounded-md border border-border p-3"
          role="dialog"
          aria-label="페어링 번호 확인"
        >
          <p className="text-xs">디스플레이에도 같은 번호가 보이나요?</p>
          <p className="font-mono text-2xl tracking-widest">
            {state.pairingPin}
          </p>
          <div className="flex gap-2">
            <Button
              onClick={() =>
                void run(() =>
                  window.api.quotamon.confirm(state.pairingPin ?? '', true)
                )
              }
            >
              일치 · 승인
            </Button>
            <Button
              variant="outline"
              onClick={() =>
                void run(() =>
                  window.api.quotamon.confirm(state.pairingPin ?? '', false)
                )
              }
            >
              다름 · 거절
            </Button>
          </div>
        </div>
      )}
      <div className="space-y-2">
        <h4 className="text-sm font-medium">표시할 계정</h4>
        <p className="text-xs text-muted-foreground">
          Codex · Cursor · Copilot · Claude · Grok. Orca에 로그인된 계정과
          제공되는 사용량만 표시합니다. 로그인/인증 변경은 Orca의 계정 설정에서
          진행하세요.
        </p>
        {ordered.length === 0 && (
          <p className="text-xs text-muted-foreground">
            Orca 계정 설정에서 먼저 로그인하세요.
          </p>
        )}
        {ordered.map((account, index) => (
          <div
            key={account.id}
            className="space-y-2 rounded-md border border-border p-3"
          >
            <div className="flex items-center gap-2">
              <Checkbox
                id={account.id}
                checked={selection(account.id).enabled}
                onCheckedChange={(v) =>
                  change(account.id, { enabled: v === true })
                }
              />
              <label htmlFor={account.id} className="text-xs font-medium">
                {account.title}
              </label>
              <span className="text-xs text-muted-foreground">
                {account.state === 'ready'
                  ? '수집됨'
                  : account.state === 'stale'
                    ? '오래된 정보'
                    : '확인 불가'}
              </span>
            </div>
            <div className="grid grid-cols-2 gap-2">
              <Input
                aria-label={`${account.title} 표시 이름`}
                placeholder={account.profile}
                value={selection(account.id).alias}
                onChange={(e) => change(account.id, { alias: e.target.value })}
              />
              <Input
                aria-label={`${account.title} 요금제`}
                placeholder={account.plan}
                value={selection(account.id).plan}
                onChange={(e) => change(account.id, { plan: e.target.value })}
              />
            </div>
            <div className="flex gap-2">
              <Button
                variant="outline"
                disabled={index === 0}
                onClick={() => order(account.id, -1)}
              >
                위로
              </Button>
              <Button
                variant="outline"
                disabled={index === ordered.length - 1}
                onClick={() => order(account.id, 1)}
              >
                아래로
              </Button>
            </div>
            {account.source_note && (
              <p className="text-xs text-muted-foreground">
                {account.source_note}
              </p>
            )}
          </div>
        ))}
      </div>
      {draft && (
        <div className="space-y-3">
          <div className="grid grid-cols-2 gap-3">
            <label className="space-y-1 text-xs">
              전송 주기 (15–3600초)
              <Input
                type="number"
                min={15}
                max={3600}
                value={draft.display.poll_seconds}
                onChange={(e) =>
                  display('poll_seconds', Number(e.target.value))
                }
              />
            </label>
            <label className="space-y-1 text-xs">
              화면 밝기 (10–100%)
              <Input
                type="number"
                min={10}
                max={100}
                value={draft.display.brightness_percent}
                onChange={(e) =>
                  display('brightness_percent', Number(e.target.value))
                }
              />
            </label>
          </div>
          <div className="flex flex-wrap gap-4">
            {(
              [
                'show_reset',
                'show_updated',
                'show_credits',
                'show_session'
              ] as const
            ).map((key, index) => (
              <label key={key} className="flex items-center gap-2 text-xs">
                <Checkbox
                  checked={draft.display[key]}
                  onCheckedChange={(v) => display(key, v === true)}
                />
                {
                  ['리셋 시각', '수집 시각', '잔여 금액/크레딧', '세션 사용량'][
                    index
                  ]
                }
              </label>
            ))}
          </div>
          <div className="flex gap-2">
            <Button
              disabled={busy || !dirty}
              onClick={() =>
                void run(async () => {
                  await window.api.quotamon.save(draft)
                  const next = await window.api.quotamon.state()
                  setDraft(next.config)
                  setState(next)
                  setDirty(false)
                })
              }
            >
              저장
            </Button>
            <Button
              variant="outline"
              disabled={busy}
              onClick={() => {
                setDraft(state?.config ?? null)
                setDirty(false)
              }}
            >
              취소 / 다시 읽기
            </Button>
            <Button
              variant="outline"
              disabled={busy}
              onClick={() => void run(() => window.api.quotamon.refresh())}
            >
              사용량 갱신
            </Button>
          </div>
          <p className="text-xs text-muted-foreground">
            BLE 전송 주기와 서비스 원천 조회 주기는 별개입니다. 원천 조회는
            Orca의 캐시와 제한 정책을 따릅니다.
          </p>
        </div>
      )}
    </section>
  )
}
