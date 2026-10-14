'use strict'
const key =
  location.hash.slice(1) || sessionStorage.getItem('quotamon-key') || ''
if (key) sessionStorage.setItem('quotamon-key', key)
history.replaceState(null, '', '/')
const $ = (id) => document.getElementById(id)
let state,
  draft,
  dirty = false,
  prefsDirty = false,
  busyPoll = false,
  scanUntil = 0,
  localNotice = ''
const pages = {
  overview: [
    '구독의 여유를 확인하세요.',
    'Orca에 연결된 계정을 책상 위 화면으로.'
  ],
  accounts: ['나의 계정, 나의 순서.', '표시할 구독과 계정을 선택하세요.'],
  device: [
    '책상 위 화면과 연결하세요.',
    '가까운 디스플레이를 Bluetooth로 연결합니다.'
  ],
  settings: ['내게 맞는 화면으로.', '갱신 주기와 표시 항목을 조정하세요.']
}
function page(name) {
  if (!pages[name]) return
  document.querySelectorAll('.page').forEach((el) => {
    el.hidden = el.id !== name
  })
  document
    .querySelectorAll('[data-page]')
    .forEach((el) => el.classList.toggle('active', el.dataset.page === name))
  $('page-title').textContent = pages[name][0]
  $('page-subtitle').textContent = pages[name][1]
}
document
  .querySelectorAll('[data-page],[data-go]')
  .forEach((el) =>
    el.addEventListener('click', () => page(el.dataset.page || el.dataset.go))
  )
$('brand').addEventListener('click', (event) => {
  event.preventDefault()
  page('overview')
})
async function api(action, value) {
  const response = await fetch('/api/' + action, {
    method: value === undefined ? 'GET' : 'POST',
    headers: {
      'X-Quotamon-Key': key,
      ...(value === undefined ? {} : { 'Content-Type': 'application/json' })
    },
    body: value === undefined ? undefined : JSON.stringify(value)
  })
  const result = await response.json()
  if (!response.ok) throw Error(result.error || '연결을 확인해주세요.')
  return result
}
function notice(message) {
  localNotice = message
  $('notice').textContent = message
  $('notice').hidden = !message
}
function changed() {
  dirty = true
  $('account-dirty').textContent = '저장하지 않은 변경 사항이 있습니다.'
}
function image(provider) {
  const img = document.createElement('img')
  img.src = '/assets/' + provider + '.png'
  img.alt = provider
  return img
}
function text(tag, content, cls) {
  const el = document.createElement(tag)
  el.textContent = content
  if (cls) el.className = cls
  return el
}
function selection(a) {
  let row = draft.accounts.find((v) => v.id === a.id)
  if (!row) {
    row = { id: a.id, alias: '', plan: '', enabled: false }
    draft.accounts.push(row)
  }
  return row
}
function ordered() {
  const byId = new Map(state.accounts.map((a) => [a.id, a]))
  const rows = []
  for (const row of draft.accounts) {
    const a = byId.get(row.id)
    if (a) {
      rows.push(a)
      byId.delete(row.id)
    }
  }
  return rows.concat([...byId.values()])
}
function loadDraft() {
  draft = structuredClone(state.config)
  dirty = false
  $('poll').value = draft.display.poll_seconds
  $('brightness').value = draft.display.brightness_percent
  for (const name of ['reset', 'updated', 'credits', 'session'])
    $('show-' + name).checked = draft.display['show_' + name]
  if (!prefsDirty) {
    $('startup').checked = state.preferences.startup
    $('copilot').checked = state.preferences.copilot
  }
  renderAccounts()
  $('account-dirty').textContent = '최신 설정을 불러왔습니다.'
}
let accountIds = ''
function renderAccounts() {
  $('account-list').replaceChildren()
  accountIds = state.accounts.map((a) => a.id).join(',')
  if (!state.accounts.length) {
    $('account-list').append(
      text('p', 'Orca를 켜면 선택할 계정이 나타납니다.', 'hint')
    )
    return
  }
  const rows = ordered()
  rows.forEach((a, index) => {
    const selected = selection(a),
      row = document.createElement('div')
    row.className = 'account-row'
    const check = document.createElement('input')
    check.type = 'checkbox'
    check.checked = selected.enabled
    check.setAttribute('aria-label', a.title + ' 표시')
    check.addEventListener('change', () => {
      selected.enabled = check.checked
      changed()
    })
    const aliasLabel = text('label', a.provider.toUpperCase() + ' · 표시 이름'),
      alias = document.createElement('input')
    alias.type = 'text'
    alias.maxLength = 64
    alias.value = selected.alias
    alias.placeholder = a.profile
    alias.setAttribute('aria-label', a.provider + ' 표시 이름')
    alias.addEventListener('input', () => {
      selected.alias = alias.value
      changed()
    })
    aliasLabel.append(alias)
    const planLabel = text('label', '요금제'),
      plan = document.createElement('input')
    plan.type = 'text'
    plan.maxLength = 64
    plan.value = selected.plan
    plan.placeholder = a.plan
    plan.setAttribute('aria-label', a.provider + ' 요금제')
    plan.addEventListener('input', () => {
      selected.plan = plan.value
      changed()
    })
    planLabel.append(plan)
    const controls = document.createElement('div')
    controls.className = 'account-order'
    for (const offset of [-1, 1]) {
      const button = text('button', offset < 0 ? '↑' : '↓')
      button.setAttribute(
        'aria-label',
        a.title + (offset < 0 ? ' 위로' : ' 아래로')
      )
      button.disabled = index + offset < 0 || index + offset >= rows.length
      button.addEventListener('click', () => {
        const selections = rows.map(selection)
        ;[selections[index], selections[index + offset]] = [
          selections[index + offset],
          selections[index]
        ]
        draft.accounts = selections
        changed()
        renderAccounts()
      })
      controls.append(button)
    }
    row.append(check, image(a.provider), aliasLabel, planLabel, controls)
    $('account-list').append(row)
  })
}
function renderCards() {
  $('cards').replaceChildren()
  $('empty').hidden = state.accounts.length !== 0
  for (const a of state.accounts) {
    const card = document.createElement('article')
    card.className = 'quota-card'
    const top = document.createElement('div')
    top.className = 'card-top'
    const name = document.createElement('div')
    name.append(
      text('div', a.provider, 'provider'),
      text('div', a.profile, 'card-profile'),
      text('div', a.plan, 'card-plan')
    )
    top.append(name, image(a.provider))
    card.append(top)
    const pool = a.pools.find((p) => p.remaining >= 0) || a.pools[0]
    const value = pool.remaining,
      label = document.createElement('div')
    label.className = 'quota-label'
    label.append(
      text(
        'span',
        value === -2
          ? '무제한'
          : value < 0
            ? '?'
            : (value > 0 && value < 1 ? '<1' : Math.round(value)) + '%',
        'quota-number'
      ),
      text('span', pool.name, 'quota-pool')
    )
    const bar = document.createElement('div'),
      fill = document.createElement('div')
    bar.className = 'bar'
    fill.className =
      'fill' + (value <= 10 ? ' low' : value <= 20 ? ' warn' : '')
    fill.style.width = Math.max(0, Math.min(100, value)) + '%'
    bar.append(fill)
    const note =
      a.state === 'stale'
        ? '오래된 정보 · 다시 확인 필요'
        : a.state === 'unknown'
          ? '사용량 확인 불가'
          : pool.reset
    card.append(
      label,
      bar,
      text(
        'div',
        note + (a.enabled ? '' : ' · 표시 선택 전'),
        'card-note' + (a.state === 'stale' ? ' stale' : '')
      )
    )
    $('cards').append(card)
  }
}
function renderDevices() {
  $('devices').replaceChildren()
  for (const device of state.devices) {
    const row = document.createElement('div')
    row.className = 'device-row'
    const label = document.createElement('div')
    label.append(
      text('strong', device.name),
      text('small', '신호 ' + device.rssi + ' dBm')
    )
    const button = text('button', '연결', 'secondary')
    button.disabled = state.connecting || !state.source_available
    button.addEventListener('click', () =>
      action(
        'connect',
        { address: device.address },
        '연결 중입니다. 양쪽의 번호를 확인해주세요.'
      )
    )
    row.append(label, button)
    $('devices').append(row)
  }
}
function render() {
  $('source-label').textContent = state.source_available
    ? 'Orca 연결됨'
    : 'Orca 연결 대기'
  $('source-dot').classList.toggle('good', state.source_available)
  $('connection-dot').classList.toggle('good', state.connected)
  $('connection-label').textContent = state.connected
    ? '디스플레이와 동기화 중'
    : state.connecting
      ? '디스플레이 연결 중'
      : '디스플레이 연결 대기'
  $('connection-note').textContent = state.connected
    ? '선택한 계정만 기기에 전달합니다.'
    : '계정을 고르고 기기에서 BLE 연결 시작을 눌러주세요.'
  $('device-source').textContent = state.source_available
    ? '연결됨'
    : 'Orca를 켜주세요'
  $('device-radio').textContent =
    state.radio_available === null
      ? '확인 중'
      : state.radio_available
        ? '사용 가능'
        : '사용할 수 없음'
  $('device-link').textContent = state.connected
    ? '연결됨'
    : state.connecting
      ? '연결 중'
      : '연결 대기'
  $('scan').disabled = Date.now() < scanUntil
  $('scan-status').textContent =
    Date.now() < scanUntil
      ? '주변 기기를 20초 동안 찾고 있습니다.'
      : scanUntil && !state.devices.length
        ? '기기를 찾지 못했습니다. 펌웨어와 BLE 연결 시작을 확인하세요.'
        : ''
  $('disconnect').disabled = !state.config.address && !state.connected
  $('last-poll').textContent =
    'PC 확인 ' + new Date().toLocaleTimeString('ko-KR', { hour12: false })
  if (!draft || (!dirty && draft.revision !== state.config.revision))
    loadDraft()
  else if (accountIds !== state.accounts.map((a) => a.id).join(',') && !dirty)
    renderAccounts()
  if (dirty && draft.revision !== state.config.revision)
    notice(
      '다른 화면에서 설정이 변경되었습니다. 최신 설정 읽기로 다시 확인하세요.'
    )
  else if (state.ui_error) notice(state.ui_error)
  else if (state.error && state.error !== 'Orca 사용량 정보 대기 중')
    notice(state.error)
  else if (!localNotice) notice('')
  renderCards()
  renderDevices()
  const dialog = $('pair-dialog')
  if (state.pairingPin) {
    $('pin').textContent = state.pairingPin
    if (!dialog.open) dialog.showModal()
  } else if (dialog.open) dialog.close()
}
async function poll() {
  if (busyPoll) return
  busyPoll = true
  try {
    state = await api('state')
    render()
  } catch {
    notice(
      '중계 프로그램 연결이 끊겼습니다. 트레이에서 설정을 다시 열어주세요.'
    )
  } finally {
    busyPoll = false
  }
}
async function action(name, value = {}, success = '') {
  try {
    await api(name, value)
    notice(success)
    await poll()
  } catch (error) {
    notice(error.message)
  }
}
$('refresh').addEventListener('click', () =>
  action('refresh', {}, '사용량 갱신을 요청했습니다.')
)
$('scan').addEventListener('click', async () => {
  scanUntil = Date.now() + 20000
  await action('scan')
  render()
})
$('disconnect').addEventListener('click', () =>
  action('disconnect', { forget: true }, '자동 연결을 해제했습니다.')
)
$('reload-accounts').addEventListener('click', () => {
  loadDraft()
  notice('최신 설정을 불러왔습니다.')
})
async function saveDisplay() {
  if (!draft) return
  draft.display = {
    poll_seconds: Number($('poll').value),
    brightness_percent: Number($('brightness').value),
    show_reset: $('show-reset').checked,
    show_updated: $('show-updated').checked,
    show_credits: $('show-credits').checked,
    show_session: $('show-session').checked
  }
  try {
    await api('save', draft)
    dirty = false
    draft = undefined
    notice('설정을 저장했습니다.')
    await poll()
  } catch (error) {
    notice(error.message)
  }
}
$('save-accounts').addEventListener('click', saveDisplay)
$('save-display').addEventListener('click', saveDisplay)
$('cancel-display').addEventListener('click', () => {
  loadDraft()
  notice('최신 설정을 불러왔습니다.')
})
for (const id of [
  'poll',
  'brightness',
  'show-reset',
  'show-updated',
  'show-credits',
  'show-session'
])
  $(id).addEventListener('input', changed)
for (const id of ['startup', 'copilot'])
  $(id).addEventListener('change', () => {
    prefsDirty = true
  })
$('save-preferences').addEventListener('click', async () => {
  try {
    await api('preferences', {
      copilot: $('copilot').checked,
      startup: $('startup').checked
    })
    prefsDirty = false
    notice('PC 설정을 저장했습니다.')
    await poll()
  } catch (error) {
    notice(error.message)
  }
})
for (const [id, accept] of [
  ['accept-pin', true],
  ['reject-pin', false]
])
  $(id).addEventListener('click', () => {
    if (state.pairingPin) action('confirm', { pin: state.pairingPin, accept })
  })
$('pair-dialog').addEventListener('cancel', (event) => {
  event.preventDefault()
  if (state.pairingPin)
    action('confirm', { pin: state.pairingPin, accept: false })
})
void poll()
setInterval(poll, 2000)
