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
  pollAgain = false,
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
const localeData = globalThis.QuotamonI18n
let language = 'ko', currentPage = 'overview'
const staticNodes = []
const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT)
while (walker.nextNode()) {
  const node = walker.currentNode, source = node.textContent.trim().replace(/\s+/g, ' ')
  if (source && localeData.messages[source]) staticNodes.push([node, source])
}
const staticAttributes = [...document.querySelectorAll('[aria-label]')].map(el => [el, el.getAttribute('aria-label')])
function t(source) {
  if (typeof source !== 'string' || language === 'ko') return source
  const normalized = source.trim().replace(/\s+/g, ' ')
  const translated = localeData.messages[normalized]?.[language]
  if (translated) return source.replace(source.trim(), translated)
  // Preserve separators in short dynamic labels without translating user data.
  const prefix = /^(\s*[·|]?\s*)(.+)$/.exec(source)
  const value = prefix && localeData.messages[prefix[2]]?.[language]
  return value ? prefix[1] + value : source
}
function setLanguage(code) {
  language = localeData.languages.includes(code) ? code : 'ko'
  document.documentElement.lang = language
  document.title = t('Quotamon · 설정')
  for (const [node, source] of staticNodes) node.textContent = t(source)
  for (const [el, source] of staticAttributes) el.setAttribute('aria-label', t(source))
  page(currentPage)
  if (dirty) $('account-dirty').textContent = t('저장하지 않은 변경 사항이 있습니다.')
}
function minutes(value) {
  const match = /^(\d{2}):(\d{2})$/.exec(value)
  return match && Number(match[1]) < 24 && Number(match[2]) < 60 ? Number(match[1]) * 60 + Number(match[2]) : NaN
}
function timeValue(value) { return String(Math.floor(value / 60)).padStart(2, '0') + ':' + String(value % 60).padStart(2, '0') }

function page(name) {
  if (!pages[name]) return
  currentPage = name
  document.querySelectorAll('.page').forEach((el) => {
    el.hidden = el.id !== name
  })
  document
    .querySelectorAll('[data-page]')
    .forEach((el) => el.classList.toggle('active', el.dataset.page === name))
  $('page-title').textContent = t(pages[name][0])
  $('page-subtitle').textContent = t(pages[name][1])
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
  if (!response.ok) throw Error(result.error || t('연결을 확인해주세요.'))
  return result
}
function notice(message) {
  localNotice = message
  $('notice').textContent = message
  $('notice').hidden = !message
}
function changed() {
  dirty = true
  $('account-dirty').textContent = t('저장하지 않은 변경 사항이 있습니다.')
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
  setLanguage(draft.display.language || 'ko')
  $('language').value = language
  $('sleep-enabled').checked = draft.display.sleep_enabled === true
  $('sleep-start').value = timeValue(draft.display.sleep_start_minute ?? 1380)
  $('sleep-end').value = timeValue(draft.display.sleep_end_minute ?? 420)
  $('poll').value = draft.display.poll_seconds
  $('brightness').value = draft.display.brightness_percent
  for (const name of ['reset', 'updated', 'credits', 'session'])
    $('show-' + name).checked = draft.display['show_' + name]
  if (!prefsDirty) {
    $('startup').checked = state.preferences.startup
    $('copilot').checked = state.preferences.copilot
  }
  renderAccounts()
  $('account-dirty').textContent = t('최신 설정을 불러왔습니다.')
}
let accountIds = ''
function renderAccounts() {
  $('account-list').replaceChildren()
  accountIds = state.accounts.map((a) => a.id).join(',')
  if (!state.accounts.length) {
    $('account-list').append(
      text('p', t('Orca를 켜면 선택할 계정이 나타납니다.'), 'hint')
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
    check.setAttribute('aria-label', a.title + t(' 표시'))
    check.addEventListener('change', () => {
      selected.enabled = check.checked
      changed()
    })
    const aliasLabel = text('label', a.provider.toUpperCase() + t(' · 표시 이름')),
      alias = document.createElement('input')
    alias.type = 'text'
    alias.maxLength = 64
    alias.value = selected.alias
    alias.placeholder = a.profile
    alias.setAttribute('aria-label', a.provider + t(' 표시 이름'))
    alias.addEventListener('input', () => {
      selected.alias = alias.value
      changed()
    })
    aliasLabel.append(alias)
    const planLabel = text('label', t('요금제')),
      plan = document.createElement('input')
    plan.type = 'text'
    plan.maxLength = 64
    plan.value = selected.plan
    plan.placeholder = a.plan
    plan.setAttribute('aria-label', a.provider + t(' 요금제'))
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
        a.title + (offset < 0 ? t(' 위로') : t(' 아래로'))
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
      text('div', t(a.plan), 'card-plan')
    )
    top.append(name, image(a.provider))
    card.append(top)
    const pools = a.provider === 'cursor'
      ? a.pools
      : [a.pools.find((p) => p.remaining >= 0) || a.pools[0]]
    for (const pool of pools) {
      const value = pool.remaining,
        label = document.createElement('div')
      label.className = 'quota-label'
      label.append(
        text(
          'span',
          value === -2
            ? t('무제한')
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
          ? t('오래된 정보 · 다시 확인 필요')
          : a.state === 'unknown'
            ? t('사용량 확인 불가')
            : t(pool.reset)
      card.append(
        label,
        bar,
        text(
          'div',
          note + (a.enabled ? '' : t(' · 표시 선택 전')),
          'card-note' + (a.state === 'stale' ? ' stale' : '')
        )
      )
    }
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
      text('small', device.paired ? t('Windows에 페어링된 디스플레이') : t('신호 ') + device.rssi + ' dBm')
    )
    const button = text('button', t('연결'), 'secondary')
    button.disabled = state.connecting || !state.source_available
    button.addEventListener('click', () =>
      action(
        'connect',
        { address: device.address },
        t('연결 중입니다. 양쪽의 번호를 확인해주세요.')
      )
    )
    row.append(label, button)
    $('devices').append(row)
  }
}
function render() {
  if (!draft || (!dirty && draft.revision !== state.config.revision)) loadDraft()
  $('source-label').textContent = state.source_available
    ? t('Orca 연결됨')
    : t('Orca 연결 대기')
  $('source-dot').classList.toggle('good', state.source_available)
  $('connection-dot').classList.toggle('good', state.connected)
  $('connection-label').textContent = state.connected
    ? t('디스플레이와 동기화 중')
    : state.connecting
      ? t('디스플레이 연결 중')
      : t('디스플레이 연결 대기')
  $('connection-note').textContent = state.connected
    ? t('선택한 계정만 기기에 전달합니다.')
    : t('계정을 고르고 기기에서 BLE 연결 시작을 눌러주세요.')
  $('device-source').textContent = state.source_available
    ? t('연결됨')
    : t('Orca를 켜주세요')
  $('device-radio').textContent =
    state.radio_available === null
      ? t('확인 중')
      : state.radio_available
        ? t('사용 가능')
        : t('사용할 수 없음')
  $('device-link').textContent = state.connected
    ? t('연결됨')
    : state.connecting
      ? t('연결 중')
      : t('연결 대기')
  renderScanStatus()
  $('disconnect').disabled = !state.config.address && !state.connected
  $('last-poll').textContent =
    t('PC 확인 ') + new Date().toLocaleTimeString(language, { hour12: false })
  if (accountIds !== state.accounts.map((a) => a.id).join(',') && !dirty)
    renderAccounts()
  if (dirty && draft.revision !== state.config.revision)
    notice(
      t('다른 화면에서 설정이 변경되었습니다. 최신 설정 읽기로 다시 확인하세요.')
    )
  else if (state.ui_error) notice(t(state.ui_error))
  else if (state.error && state.error !== t('Orca 사용량 정보 대기 중'))
    notice(t(state.error))
  else if (!localNotice) notice('')
  renderCards()
  renderDevices()
  const dialog = $('pair-dialog')
  if (state.pairingPin) {
    $('pin').textContent = state.pairingPin
    if (!dialog.open) dialog.showModal()
  } else if (dialog.open) dialog.close()
}
function renderScanStatus() {
  $('scan').disabled = Date.now() < scanUntil
  $('scan-status').textContent =
    Date.now() < scanUntil
      ? t('주변 기기를 20초 동안 찾고 있습니다.')
      : scanUntil && !state.devices.length
        ? t('기기를 찾지 못했습니다. 펌웨어와 BLE 연결 시작을 확인하세요.')
        : ''
}
async function poll() {
  if (busyPoll) {
    pollAgain = true
    return
  }
  busyPoll = true
  try {
    state = await api('state')
    render()
  } catch {
    notice(
      t('중계 프로그램 연결이 끊겼습니다. 트레이에서 설정을 다시 열어주세요.')
    )
  } finally {
    busyPoll = false
    if (pollAgain) {
      pollAgain = false
      void poll()
    }
  }
}
async function action(name, value = {}, success = '') {
  try {
    await api(name, value)
    notice(success)
    await poll()
  } catch (error) {
    notice(t(error.message))
  }
}
$('refresh').addEventListener('click', () =>
  action('refresh', {}, t('사용량 갱신을 요청했습니다.'))
)
$('scan').addEventListener('click', async () => {
  scanUntil = Date.now() + 20000
  setTimeout(renderScanStatus, 20000)
  await action('scan')
  render()
})
$('disconnect').addEventListener('click', () =>
  action('disconnect', { forget: true }, t('자동 연결을 해제했습니다.'))
)
$('reload-accounts').addEventListener('click', () => {
  loadDraft()
  notice(t('최신 설정을 불러왔습니다.'))
})
async function saveDisplay() {
  if (!draft) return
  if (!Number.isFinite(minutes($('sleep-start').value)) || !Number.isFinite(minutes($('sleep-end').value))) {
    notice(t('시간은 24시간제 HH:MM 형식으로 입력하세요.')); return
  }
  if ($('sleep-enabled').checked && ($('sleep-start').value === $('sleep-end').value)) {
    notice(t('시작과 종료 시각은 달라야 합니다.')); return
  }
  draft.display = {
    language: $('language').value,
    sleep_enabled: $('sleep-enabled').checked,
    sleep_start_minute: minutes($('sleep-start').value),
    sleep_end_minute: minutes($('sleep-end').value),
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
    notice(t('설정을 저장했습니다.'))
    await poll()
  } catch (error) {
    notice(t(error.message))
  }
}
$('save-accounts').addEventListener('click', saveDisplay)
$('save-display').addEventListener('click', saveDisplay)
$('cancel-display').addEventListener('click', () => {
  loadDraft()
  notice(t('최신 설정을 불러왔습니다.'))
})
for (const id of [
  'language', 'sleep-enabled', 'sleep-start', 'sleep-end',
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
    notice(t('PC 설정을 저장했습니다.'))
    await poll()
  } catch (error) {
    notice(t(error.message))
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
$('language').addEventListener('change', () => {
  setLanguage($('language').value)
  localNotice = ''
  if (state) { render(); renderAccounts() }
})
// One authenticated stream replaces periodic state requests. The bridge signals
// two seconds before the next snapshot, or immediately for connection/settings.
async function watch() {
  try {
    const response = await fetch('/api/events', {
      headers: { 'X-Quotamon-Key': key }
    })
    if (response.status === 403) {
      notice(t('트레이에서 설정 페이지를 다시 열어주세요.'))
      return
    }
    if (!response.ok || !response.body) throw Error('Stream unavailable')
    if (localNotice === t('중계 프로그램 연결을 확인하고 있습니다.')) notice('')
    const reader = response.body.getReader(), decoder = new TextDecoder()
    let pending = ''
    try {
      while (true) {
        const { value, done } = await reader.read()
        if (done) break
        pending += decoder.decode(value, { stream: true })
        let end
        while ((end = pending.indexOf('\n\n')) >= 0) {
          const event = pending.slice(0, end)
          pending = pending.slice(end + 2)
          if (event === 'data: refresh') void poll()
        }
      }
    } finally {
      reader.releaseLock()
    }
  } catch {
    /* The stream also ends normally when its controller is replaced. */
  }
  notice(t('중계 프로그램 연결을 확인하고 있습니다.'))
  setTimeout(watch, 3000)
}
void watch()
