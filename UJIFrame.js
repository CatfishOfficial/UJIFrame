/*!
 * UJIFrame — a portable, no-build-step in-browser admin terminal.
 *
 * Drop this one file into any page and call UJIFrame.create({...}) — it
 * injects its own DOM + CSS, gives you a command registry, a 5-tier
 * confirmation system, "|" command sequencing with sudoseq auto-confirm, a
 * localStorage-backed config system, and a set of built-in animations
 * (matrix rain, fire, fireworks) gated by the "glitter" config.
 *
 * Confirmation tiers (the exact phrases are part of the personality, not
 * just placeholders — keep them):
 *   1: "yes"
 *   2: "IKnowWhatImDoing!"
 *   3: "ISwearIKnowWhatImDoing!!"
 *   4: "ISwearIReallyKnowWhatImDoing!!!"
 *   5: tier 2, then tier 3 (two-step)
 *
 * Usage:
 *   const term = UJIFrame.create({ appName: 'MY APP CONSOLE', storageKey: 'myapp_cli_config' })
 *   term.registerCommand('list', { help: 'List things', run: async (args) => { term.println('...') } })
 *   const ok = await term.confirm(2, 'About to do a thing.')
 *   if (!ok) return
 */
(function (global) {
  'use strict'

  const TIER_PHRASES = {
    2: 'IKnowWhatImDoing!',
    3: 'ISwearIKnowWhatImDoing!!',
    4: 'ISwearIReallyKnowWhatImDoing!!!',
  }

  const THEME_PALETTE = {
    green:  { accent: '#4CAF50', lead: '#c8ffc8', trail: '#00bb00', glow: '#00ff41', bg: 'rgba(0,12,0,0.82)',  dim: 'rgba(0,255,65,0.25)' },
    amber:  { accent: '#FFB300', lead: '#ffe9b3', trail: '#cc8b00', glow: '#ffb300', bg: 'rgba(18,12,0,0.82)', dim: 'rgba(255,179,0,0.25)' },
    blue:   { accent: '#64B5F6', lead: '#cfe8ff', trail: '#2f7fd1', glow: '#64b5f6', bg: 'rgba(0,10,22,0.82)', dim: 'rgba(100,181,246,0.25)' },
    purple: { accent: '#BA68C8', lead: '#eccdf2', trail: '#8e3f9e', glow: '#ba68c8', bg: 'rgba(16,0,20,0.82)', dim: 'rgba(186,104,200,0.25)' },
  }

  const KONAMI = ['ArrowUp','ArrowUp','ArrowDown','ArrowDown','ArrowLeft','ArrowRight','ArrowLeft','ArrowRight','b','a']

  function buildCss(id) {
    return `
#${id}{display:none;position:fixed;inset:0;z-index:10000;background:rgba(0,0,0,0.82);backdrop-filter:blur(4px);align-items:center;justify-content:center;}
#${id}.open{display:flex;}
#${id}-box{--tf-accent:#4CAF50;--tf-glow:#00ff41;--tf-bg:rgba(0,12,0,0.82);--tf-dim:rgba(0,255,65,0.25);width:860px;max-width:96vw;height:580px;max-height:90vh;background:#0c0c0c;border:1px solid #2a2a2a;border-radius:7px;display:flex;flex-direction:column;font-family:'Courier New',Courier,monospace;box-shadow:0 24px 80px rgba(0,0,0,0.8);position:relative;}
#${id}-titlebar{display:flex;align-items:center;gap:6px;padding:9px 14px;border-bottom:1px solid #1a1a1a;flex-shrink:0;}
#${id}-titlebar .tf-dot{width:11px;height:11px;border-radius:50%;flex-shrink:0;}
#${id}-titlebar .tf-label{font-size:11px;color:#444;margin-left:auto;letter-spacing:0.07em;}
#${id}-output{flex:1;overflow-y:auto;padding:12px 18px;font-size:11.5px;line-height:1.75;color:#aaa;word-break:break-all;}
#${id}-output::-webkit-scrollbar{width:5px;}
#${id}-output::-webkit-scrollbar-track{background:#0c0c0c;}
#${id}-output::-webkit-scrollbar-thumb{background:#2a2a2a;border-radius:3px;}
#${id}-input-row{padding:8px 18px 14px;display:flex;align-items:center;gap:8px;border-top:1px solid #1a1a1a;flex-shrink:0;}
#${id}-prompt{color:var(--tf-accent);font-size:12px;flex-shrink:0;user-select:none;}
#${id}-input{flex:1;background:none;border:none;outline:none;color:#e8e8e8;font-family:inherit;font-size:12px;caret-color:var(--tf-accent);}
.tf-ok{color:#4CAF50;}.tf-err{color:#ef5350;}.tf-warn{color:#FFB300;}.tf-dim{color:#484848;}.tf-hl{color:#64B5F6;}.tf-gold{color:#C4A35A;}.tf-bold{font-weight:700;}
.tf-id{cursor:pointer;text-decoration:underline dotted rgba(100,181,246,0.45);}.tf-id:hover{text-decoration-color:rgba(100,181,246,0.9);}
.tf-simple-overlay{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;pointer-events:none;z-index:10;}
.tf-simple-overlay-inner{display:flex;align-items:center;gap:8px;background:var(--tf-bg);border:1px solid var(--tf-glow);border-radius:6px;padding:6px 14px;color:var(--tf-glow);font-family:'Courier New',monospace;font-size:12px;font-weight:700;}
.tf-spinner{width:13px;height:13px;border:2px solid var(--tf-dim);border-top-color:var(--tf-glow);border-radius:50%;animation:tf-spin 0.8s linear infinite;flex-shrink:0;}
@keyframes tf-spin{to{transform:rotate(360deg);}}
`
  }

  function buildHtml(id) {
    return `
<div id="${id}-box">
  <div id="${id}-titlebar">
    <div class="tf-dot" style="background:#ff5f56;"></div>
    <div class="tf-dot" style="background:#ffbd2e;"></div>
    <div class="tf-dot" style="background:#27c93f;"></div>
    <span class="tf-label" id="${id}-titlelabel"></span>
  </div>
  <div id="${id}-output"></div>
  <div id="${id}-input-row">
    <span id="${id}-prompt">&gt;</span>
    <input id="${id}-input" type="text" autocomplete="off" spellcheck="false">
  </div>
</div>`
  }

  function create(userOptions) {
    const opts = Object.assign({
      rootId: 'tf-console',
      appName: 'UJIFRAME CONSOLE',
      titleLabel: 'admin',
      storageKey: 'ujiframe_config',
      trigger: 'konami', // 'konami' | 'none'
      glitter: 'focus',
      color: 'green',
    }, userOptions || {})

    const id = opts.rootId

    if (!document.getElementById(id + '-style')) {
      const styleEl = document.createElement('style')
      styleEl.id = id + '-style'
      styleEl.textContent = buildCss(id)
      document.head.appendChild(styleEl)
    }

    let panel = document.getElementById(id)
    if (!panel) {
      panel = document.createElement('div')
      panel.id = id
      panel.innerHTML = buildHtml(id)
      document.body.appendChild(panel)
    }

    const box    = document.getElementById(id + '-box')
    const output = document.getElementById(id + '-output')
    const input  = document.getElementById(id + '-input')
    document.getElementById(id + '-titlelabel').textContent = opts.titleLabel

    // ── output helpers ──────────────────────────────────────────────────
    function escHtml(s) { return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;') }
    function print(html) {
      const d = document.createElement('div')
      d.innerHTML = html
      output.appendChild(d)
      output.scrollTop = output.scrollHeight
    }
    function println(text, cls) {
      print(cls ? `<span class="${cls}">${escHtml(text)}</span>` : escHtml(text))
    }
    function printCmdEcho(text, label) {
      const stamp = CONFIG.timestamps === 'on'
        ? `<span class="tf-dim">[${new Date().toLocaleTimeString('en-US', { hour12: false })}]</span> `
        : ''
      print(`${stamp}<span class="tf-dim">${label || '$'}</span> ${escHtml(text)}`)
    }
    function clearOutput() { output.innerHTML = '' }

    // ── config system ────────────────────────────────────────────────────
    const GLITTER_SPEED_MULT = { slow: 0.6, normal: 1, fast: 1.8 }
    const CONFIG_SCHEMA = {
      glitter: ['lively', 'focus', 'static'],
      color: [...Object.keys(THEME_PALETTE), 'lightmode'],
      timestamps: ['on', 'off'],
      confirmTimeout: ['30', '60', '120', 'never'],
      chainOverlay: ['on', 'off'],
      glitterSpeed: Object.keys(GLITTER_SPEED_MULT),
      glitterRainbow: ['on', 'off'],
      maxLoop: ['safe', 'freedom'],
    }
    const MAX_LOOP_VALUES = { safe: 100, freedom: Infinity }
    let CONFIG = {
      glitter: opts.glitter,
      color: opts.color,
      timestamps: 'off',
      confirmTimeout: 'never',
      chainOverlay: 'on',
      glitterSpeed: 'normal',
      glitterRainbow: 'on',
      maxLoop: 'safe',
    }
    try {
      Object.assign(CONFIG, JSON.parse(localStorage.getItem(opts.storageKey) || '{}'))
    } catch (e) {}
    let GLITTER = CONFIG_SCHEMA.glitter.includes(CONFIG.glitter) ? CONFIG.glitter : 'focus'

    function applyTheme(name) {
      const t = THEME_PALETTE[name] || THEME_PALETTE.green
      box.style.setProperty('--tf-accent', t.accent)
      box.style.setProperty('--tf-glow', t.glow)
      box.style.setProperty('--tf-bg', t.bg)
      box.style.setProperty('--tf-dim', t.dim)
    }
    applyTheme(CONFIG_SCHEMA.color.includes(CONFIG.color) ? CONFIG.color : 'green')

    function saveConfig() {
      try { localStorage.setItem(opts.storageKey, JSON.stringify(CONFIG)) } catch (e) {}
    }
    function registerConfig(key, allowed, defaultValue) {
      CONFIG_SCHEMA[key] = allowed
      if (!(key in CONFIG)) CONFIG[key] = defaultValue
    }

    function cmdConfig(args) {
      if (args.length === 0) {
        println('Current config:', 'tf-dim')
        Object.entries(CONFIG_SCHEMA).forEach(([key, allowed]) => {
          print(`  <span class="tf-hl">${key.padEnd(20)}</span>${escHtml(CONFIG[key])}  <span class="tf-dim">(${allowed.join(' | ')})</span>`)
        })
        return
      }
      const [key, value] = args
      if (!(key in CONFIG_SCHEMA)) {
        println(`"${key}" is not a config option. Type "config" to see available options.`, 'tf-err')
        return
      }
      if (!value) {
        println(`${key} = ${CONFIG[key]}`, 'tf-dim')
        return
      }
      const allowed = CONFIG_SCHEMA[key]
      if (!allowed.includes(value)) {
        println(`Invalid value. "${key}" must be one of: ${allowed.join(', ')}`, 'tf-err')
        return
      }
      if (key === 'color' && value === 'lightmode') {
        println('who uses light mode?', 'tf-dim')
        return
      }
      const prevValue = CONFIG[key]
      CONFIG[key] = value
      saveConfig()
      if (key === 'glitter') GLITTER = value
      if (key === 'color') applyTheme(value)
      println(`✓ ${key} set to "${value}".`, 'tf-ok')
      if (key === 'glitter' && value === 'lively' && prevValue !== 'lively') {
        playLivelyPreview()
      }
      if (key === 'glitter' && prevValue === 'lively' && value === 'focus') {
        println('boringgg~ :(', 'tf-dim')
      }
    }

    // ── animations ───────────────────────────────────────────────────────
    let _simpleOverlay = null
    function showSimpleOverlay(label, spin) {
      if (_simpleOverlay) return
      const el = document.createElement('div')
      el.className = 'tf-simple-overlay'
      el.innerHTML = `<div class="tf-simple-overlay-inner">${spin ? '<div class="tf-spinner"></div>' : ''}<span>${escHtml(label)}</span></div>`
      box.appendChild(el)
      _simpleOverlay = el
    }
    function hideSimpleOverlay() {
      if (_simpleOverlay) { _simpleOverlay.remove(); _simpleOverlay = null }
    }

    function createCanvas(zIndex, opacity) {
      const c = document.createElement('canvas')
      c.style.cssText = `position:absolute;inset:0;width:100%;height:100%;pointer-events:none;z-index:${zIndex};border-radius:7px;${opacity != null ? `opacity:${opacity};` : ''}`
      c.width = box.offsetWidth; c.height = box.offsetHeight
      box.appendChild(c)
      return c
    }

    // Shared rain renderer — used by the gated "loading" matrix and by the
    // always-on matrix easter egg, so the actual animation isn't duplicated.
    function runMatrixRain(c, label, runtimeOpts, setRaf) {
      const rOpts = runtimeOpts || {}
      const speed = rOpts.speed || 1
      const rainbow = !!rOpts.rainbow
      const ctx = c.getContext('2d')
      const W = c.width, H = c.height, fs = 13
      const cols = Math.floor(W / fs)
      const drops = Array.from({length: cols}, () => Math.random() * -(H / fs))
      const chars = 'アイウエオカキクケコサシスセソタチツテトナニヌネノ0123456789ABCDEF><|/'
      const theme = THEME_PALETTE[CONFIG.color] || THEME_PALETTE.green
      let hue = 0
      function frame() {
        ctx.fillStyle = 'rgba(0,0,0,0.055)'
        ctx.fillRect(0, 0, W, H)
        if (rainbow) hue = (hue + 3) % 360
        drops.forEach((y, i) => {
          const ch = chars[Math.floor(Math.random() * chars.length)]
          ctx.font = `${fs}px 'Courier New'`
          ctx.fillStyle = rainbow ? `hsl(${(hue + i * 14) % 360},100%,72%)` : theme.lead
          ctx.fillText(ch, i * fs, y * fs)
          ctx.fillStyle = rainbow ? `hsl(${(hue + i * 14 + 30) % 360},100%,45%)` : theme.trail
          if (y > 1) ctx.fillText(chars[Math.floor(Math.random() * chars.length)], i * fs, (y - 1) * fs)
          if (y * fs > H && Math.random() > 0.97) drops[i] = 0
          drops[i] += 0.55 * speed
        })
        const tw = ctx.measureText(label).width + 28
        const tx = (W - tw) / 2, ty = H / 2 - 13
        const labelColor = rainbow ? `hsl(${hue},100%,60%)` : theme.glow
        ctx.fillStyle = theme.bg
        ctx.fillRect(tx, ty, tw, 26)
        ctx.strokeStyle = labelColor
        ctx.lineWidth = 1
        ctx.strokeRect(tx, ty, tw, 26)
        ctx.fillStyle = labelColor
        ctx.font = 'bold 12px "Courier New"'
        ctx.textAlign = 'center'
        ctx.fillText(label, W / 2, ty + 17)
        ctx.textAlign = 'left'
        setRaf(requestAnimationFrame(frame))
      }
      frame()
    }

    let _mCanvas = null, _mRaf = null, _mDepth = 0
    function startLoading(label, loadOpts) {
      label = label || 'PROCESSING...'
      _mDepth++
      if (GLITTER === 'focus') { showSimpleOverlay(label, true); return }
      if (GLITTER === 'static') { showSimpleOverlay(label, false); return }
      if (_mCanvas) return
      _mCanvas = createCanvas(10, 0.78)
      runMatrixRain(_mCanvas, label, loadOpts, raf => { _mRaf = raf })
    }
    function stopLoading() {
      _mDepth = Math.max(0, _mDepth - 1)
      if (_mDepth > 0) return
      if (GLITTER !== 'lively') { hideSimpleOverlay(); return }
      if (_mRaf) { cancelAnimationFrame(_mRaf); _mRaf = null }
      if (_mCanvas) { _mCanvas.remove(); _mCanvas = null }
    }

    // The "matrix" easter egg is explicit and on-demand — always shows the
    // real rain regardless of glitter, fully independent of the loading state.
    let _mEggCanvas = null, _mEggRaf = null
    function startMatrixEgg(label) {
      if (_mEggCanvas) return
      _mEggCanvas = createCanvas(10, 0.78)
      runMatrixRain(_mEggCanvas, label, { speed: GLITTER_SPEED_MULT[CONFIG.glitterSpeed] || 1 }, raf => { _mEggRaf = raf })
    }
    function stopMatrixEgg() {
      if (_mEggRaf) { cancelAnimationFrame(_mEggRaf); _mEggRaf = null }
      if (_mEggCanvas) { _mEggCanvas.remove(); _mEggCanvas = null }
    }

    // Safety net: a sequence-driven rain should never outlive its sequence.
    let _mWatchdog = null
    function ensureSequenceWatchdog() {
      if (_mWatchdog) return
      _mWatchdog = setInterval(() => {
        if (sequenceActive) return
        if (_mCanvas) {
          _mDepth = 0
          if (_mRaf) { cancelAnimationFrame(_mRaf); _mRaf = null }
          _mCanvas.remove(); _mCanvas = null
        }
        if (_simpleOverlay) { _mDepth = 0; hideSimpleOverlay() }
        clearInterval(_mWatchdog)
        _mWatchdog = null
      }, 1500)
    }

    let _fCanvas = null, _fRaf = null, _fFalling = false, _fResolve = null
    function startFire() {
      if (GLITTER === 'focus') { showSimpleOverlay('PURGING...', true); return }
      if (GLITTER === 'static') { showSimpleOverlay('PURGING...', false); return }
      if (_fCanvas) return
      _fFalling = false
      const c = createCanvas(10)
      _fCanvas = c
      const ctx = c.getContext('2d')
      const W = c.width, H = c.height, fs = 13
      const CHARS  = ['█','█','▓','▓','▒','░','░']
      const COLORS = ['#ffdd00','#ffaa00','#ff8800','#ff5500','#ff2200','#bb0000','#660000']
      const MAX = 8
      const hCols = Math.floor(W / fs), vRows = Math.floor(H / fs)
      const mk = (n) => ({ h: Array.from({length:n}, () => 0), t: Array.from({length:n}, () => 0) })
      const bot = mk(hCols), top = mk(hCols), lft = mk(vRows), rgt = mk(vRows)

      const perim = []
      for (let i = 0; i < hCols; i++) perim.push({ lane: bot, idx: i, x: i*fs,     y: H - fs })
      for (let i = 0; i < vRows; i++) perim.push({ lane: rgt, idx: i, x: W - fs,   y: i*fs   })
      for (let i = hCols-1; i >= 0; i--) perim.push({ lane: top, idx: i, x: i*fs,  y: 0      })
      for (let i = vRows-1; i >= 0; i--) perim.push({ lane: lft, idx: i, x: 0,     y: i*fs   })

      let phase = 'lighting'
      let sparkPos = 0
      const sparkSpeed = perim.length / 40

      function drawChar(x, y, depth) {
        const ratio = depth / (MAX - 1)
        const jitter = Math.random() < 0.5 ? (Math.random() < 0.5 ? -1 : 1) : 0
        const idx = Math.min(COLORS.length-1, Math.max(0, Math.round(ratio*(COLORS.length-1))+jitter))
        ctx.fillStyle = COLORS[idx]
        ctx.globalAlpha = Math.max(0.1, 1 - ratio * 0.8)
        ctx.fillText(CHARS[Math.min(CHARS.length-1,idx)], x, y)
      }
      function tick(lane, maxD) {
        for (let i = 0; i < lane.h.length; i++) {
          if (_fFalling) lane.t[i] = 0
          else if (phase === 'rising' && Math.random() < 0.05) lane.t[i] = 2 + Math.random() * (maxD - 2)
          lane.h[i] += (lane.t[i] - lane.h[i]) * (_fFalling ? 0.07 : 0.025) + (_fFalling ? 0 : (Math.random()-0.48)*0.5)
          lane.h[i] = Math.max(0, Math.min(maxD, lane.h[i]))
        }
      }
      function frame() {
        ctx.clearRect(0, 0, W, H)
        ctx.font = `bold ${fs}px 'Courier New'`

        if (phase === 'lighting' && !_fFalling) {
          sparkPos += sparkSpeed
          const lit = Math.floor(sparkPos)
          for (let p = 0; p < Math.min(lit, perim.length); p++) {
            const e = perim[p]
            if (e.lane.t[e.idx] === 0) e.lane.t[e.idx] = 2 + Math.random() * (MAX - 2)
          }
          const TRAIL = ['#ffffff','#ffee44','#ffaa00','#ff4400']
          for (let t = 0; t < TRAIL.length; t++) {
            const pi = Math.max(0, Math.min(perim.length-1, lit - t))
            const e = perim[pi]
            ctx.fillStyle = TRAIL[t]
            ctx.globalAlpha = 1 - t * 0.22
            ctx.fillText(t === 0 ? '✦' : '·', e.x, e.y + fs)
          }
          ctx.globalAlpha = 1
          if (sparkPos >= perim.length) phase = 'rising'
        }

        tick(bot, MAX); tick(top, MAX); tick(lft, MAX); tick(rgt, MAX)
        for (let i = 0; i < hCols; i++) {
          for (let d = 0; d < Math.round(bot.h[i]); d++) { if (Math.random() > 0.18) drawChar(i*fs, H - d*fs,    d) }
          for (let d = 0; d < Math.round(top.h[i]); d++) { if (Math.random() > 0.18) drawChar(i*fs, (d+1)*fs,    d) }
        }
        for (let i = 0; i < vRows; i++) {
          for (let d = 0; d < Math.round(lft.h[i]); d++) { if (Math.random() > 0.18) drawChar(d*fs,       (i+1)*fs, d) }
          for (let d = 0; d < Math.round(rgt.h[i]); d++) { if (Math.random() > 0.18) drawChar(W-(d+1)*fs, (i+1)*fs, d) }
        }
        ctx.globalAlpha = 1
        const allDone = _fFalling && [bot,top,lft,rgt].every(l => l.h.every(v => v < 0.3))
        if (allDone) {
          cancelAnimationFrame(_fRaf); _fRaf = null
          c.remove(); _fCanvas = null; _fFalling = false
          if (_fResolve) { _fResolve(); _fResolve = null }
          return
        }
        _fRaf = requestAnimationFrame(frame)
      }
      frame()
    }
    function stopFire() {
      if (GLITTER !== 'lively') { hideSimpleOverlay(); return Promise.resolve() }
      if (!_fCanvas) return Promise.resolve()
      return new Promise(resolve => { _fFalling = true; _fResolve = resolve })
    }

    // One-shot preview fired when the user switches glitter to lively.
    let _gCanvas = null, _gRaf = null, _gRunning = false
    function startGlitterPreview() {
      if (_gCanvas) return
      _gRunning = true
      const c = createCanvas(11)
      _gCanvas = c
      const ctx = c.getContext('2d')
      const W = c.width, H = c.height
      let rockets = [], particles = [], frames = 0, nextLaunch = 0
      function frame() {
        ctx.clearRect(0, 0, W, H)
        ctx.font = 'bold 15px "Courier New"'
        ctx.textAlign = 'center'
        ctx.shadowBlur = 8
        if (_gRunning && frames >= nextLaunch) {
          rockets.push({
            x: W * (0.15 + Math.random() * 0.7),
            y: H,
            vy: -(9 + Math.random() * 3),
            targetY: H * (0.2 + Math.random() * 0.3),
            hue: Math.random() * 360,
          })
          nextLaunch = frames + 9 + Math.floor(Math.random() * 6)
        }
        rockets = rockets.filter(r => {
          r.y += r.vy
          ctx.shadowColor = `hsl(${r.hue},100%,85%)`
          ctx.fillStyle = `hsl(${r.hue},100%,85%)`
          ctx.fillText('*', r.x, r.y)
          if (r.y > r.targetY) return true
          const n = 12 + Math.floor(Math.random() * 8)
          for (let k = 0; k < n; k++) {
            const ang = (Math.PI * 2 * k) / n + Math.random() * 0.3
            const speed = 1.5 + Math.random() * 2.4
            particles.push({ x: r.x, y: r.y, vx: Math.cos(ang) * speed, vy: Math.sin(ang) * speed, hue: (r.hue + Math.random() * 40 - 20 + 360) % 360, life: 1 })
          }
          return false
        })
        particles = particles.filter(p => {
          p.x += p.vx; p.y += p.vy; p.vy += 0.08; p.life -= 0.035
          if (p.life <= 0) return false
          ctx.globalAlpha = Math.max(0, p.life)
          ctx.shadowColor = `hsl(${p.hue},100%,82%)`
          ctx.fillStyle = `hsl(${p.hue},100%,82%)`
          ctx.fillText('*', p.x, p.y)
          ctx.globalAlpha = 1
          return true
        })
        ctx.shadowBlur = 0
        frames++
        if (_gRunning || rockets.length || particles.length) {
          _gRaf = requestAnimationFrame(frame)
        } else {
          c.remove(); _gCanvas = null; _gRaf = null
        }
      }
      frame()
    }
    function stopGlitterPreview() { _gRunning = false }
    function playLivelyPreview() {
      startGlitterPreview()
      setTimeout(stopGlitterPreview, 1500)
    }

    // ── confirmation engine ──────────────────────────────────────────────
    // pendingConfirm: { kind: 'tier'|'yesno'|'konami', tier, step, resolve }
    let pendingConfirm = null
    let pendingKonamiOpen = null // resolver for an active confirmKonami() call

    function confirm(tier, promptHtml) {
      if (promptHtml) println(promptHtml, 'tf-warn')
      return new Promise(resolve => {
        if (sudoMode) { autoResolveTier(tier, resolve); return }
        if (tier === 1) {
          print('Type "yes" to confirm or anything else to cancel.')
        } else if (tier === 5) {
          print(`Type <span class="tf-hl">IKnowWhatImDoing!</span> to confirm (1/2).`)
        } else {
          print(`Type <span class="tf-hl">${TIER_PHRASES[tier]}</span> to confirm.`)
        }
        pendingConfirm = { kind: 'tier', tier, step: 1, resolve }
      })
    }
    function confirmYesNo(promptHtml) {
      if (promptHtml) print(promptHtml)
      return new Promise(resolve => {
        if (sudoMode) {
          printCmdEcho('y', '$ (sudoseq auto-confirm)')
          resolve(true)
          return
        }
        pendingConfirm = { kind: 'yesno', resolve }
      })
    }
    function confirmKonami(promptHtml) {
      if (promptHtml) print(promptHtml)
      input.blur()
      return new Promise(resolve => { pendingKonamiOpen = resolve })
    }

    function autoResolveTier(tier, resolve) {
      if (tier === 5) {
        printCmdEcho(TIER_PHRASES[2], '$ (sudoseq auto-confirm)')
        printCmdEcho(TIER_PHRASES[3], '$ (sudoseq auto-confirm)')
        resolve(true)
        return
      }
      const phrase = tier === 1 ? 'yes' : TIER_PHRASES[tier]
      printCmdEcho(phrase, '$ (sudoseq auto-confirm)')
      resolve(true)
    }

    function resolvePendingConfirm(answer) {
      const pc = pendingConfirm
      pendingConfirm = null
      if (pc.kind === 'yesno') {
        pc.resolve(answer.toLowerCase().startsWith('y'))
        return
      }
      if (pc.kind === 'tier') {
        if (pc.tier === 1) {
          const ok = answer.toLowerCase() === 'yes'
          if (!ok) println('Cancelled.', 'tf-dim')
          pc.resolve(ok)
          return
        }
        if (pc.tier === 5 && pc.step === 1) {
          if (answer !== TIER_PHRASES[2]) { println('Cancelled.', 'tf-dim'); pc.resolve(false); return }
          print(`Confirmed once. Type <span class="tf-hl">${TIER_PHRASES[3]}</span> to confirm (2/2).`)
          pendingConfirm = { kind: 'tier', tier: 5, step: 2, resolve: pc.resolve }
          return
        }
        const expected = pc.tier === 5 ? TIER_PHRASES[3] : TIER_PHRASES[pc.tier]
        const ok = answer === expected
        if (!ok) println('Cancelled.', 'tf-dim')
        pc.resolve(ok)
      }
    }

    // Confirmation timeout watchdog — always on, tracks pendingConfirm by
    // identity so any confirm() call is covered automatically.
    let _lastSeenConfirm = null, _confirmSetAt = 0
    setInterval(() => {
      if (pendingConfirm !== _lastSeenConfirm) {
        _lastSeenConfirm = pendingConfirm
        _confirmSetAt = Date.now()
      }
      if (!pendingConfirm || CONFIG.confirmTimeout === 'never') return
      const ms = parseInt(CONFIG.confirmTimeout, 10) * 1000
      if (Date.now() - _confirmSetAt >= ms) {
        println('Confirmation timed out — cancelled.', 'tf-warn')
        const pc = pendingConfirm
        pendingConfirm = null
        _lastSeenConfirm = null
        if (sequenceActive) { sequenceAbort = true }
        pc.resolve(false)
      }
    }, 1000)

    // ── command registry + help ─────────────────────────────────────────
    const commands = new Map()
    const hiddenCommands = new Map()

    // Aliases share the exact same def object under extra map keys. def.name
    // stays pinned to the canonical (first-registered) name, so help loops
    // can skip alias keys with `name !== def.name` and avoid duplicate rows.
    function registerCommand(name, def) {
      const canonical = name.toLowerCase()
      def.name = canonical
      const target = def.hidden ? hiddenCommands : commands
      target.set(canonical, def)
      if (Array.isArray(def.aliases)) {
        for (const alias of def.aliases) target.set(alias.toLowerCase(), def)
      }
    }

    // A command's `help` can be: a single string (one row, labeled with its
    // registered name); a flat [usageLabel, description] pair (one row, with
    // an explicit label — e.g. an alias like "clear / cls"); or an array of
    // such pairs, for commands with multiple usage patterns worth documenting
    // separately (e.g. delete / delete test / delete * sharing one command).
    function printHelpRow(name, help) {
      if (Array.isArray(help)) {
        const rows = Array.isArray(help[0]) ? help : [help]
        rows.forEach(([usage, desc]) => {
          print(`  <span class="tf-hl">${usage.padEnd(34)}</span><span class="tf-dim">${desc}</span>`)
        })
      } else {
        print(`  <span class="tf-hl">${name.padEnd(34)}</span><span class="tf-dim">${help}</span>`)
      }
    }

    // Reduces multi-row help to just its first row, for the abbreviated welcome screen.
    function firstHelpRow(help) {
      if (!Array.isArray(help)) return help
      return Array.isArray(help[0]) ? [help[0]] : help
    }

    function printWelcome() {
      print(`<span class="tf-gold tf-bold">${escHtml(opts.appName)}</span>  <span class="tf-dim">Esc or click outside to close</span>`)
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
      println('Type a command and press Enter. Chain commands with | (e.g. cmd1 | cmd2). Type "help" for the full command list.', 'tf-dim')
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
      let shown = 0
      for (const [name, def] of commands) {
        if (shown >= 5) break
        if (!def.help || def.builtin || name !== def.name) continue
        printHelpRow(name, firstHelpRow(def.help))
        shown++
      }
      print(`  <span class="tf-hl">${'help'.padEnd(34)}</span><span class="tf-dim">Show every available command</span>`)
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
    }

    function printHelp() {
      print(`<span class="tf-gold tf-bold">${escHtml(opts.appName)}</span>  <span class="tf-dim">Esc or click outside to close</span>`)
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
      print(`  <span class="tf-hl">${'cmd1 | cmd2 | ...'.padEnd(34)}</span><span class="tf-dim">Run commands in sequence — any confirmations still prompt, sequence resumes after</span>`)
      print(`  <span class="tf-hl">${'sudoseq | cmd1 | cmd2 | ...'.padEnd(34)}</span><span class="tf-dim">Sequence + auto-answer every confirmation</span>`)
      print(`  <span class="tf-hl">${'loop <n> cmd1 | cmd2 | ...'.padEnd(34)}</span><span class="tf-dim">Repeat a command (or chain) n times</span>`)
      print(`  <span class="tf-hl">${'wait <ms|Ns>'.padEnd(34)}</span><span class="tf-dim">Pause a chain, e.g. wait 2000 or wait 2s</span>`)
      for (const [name, def] of commands) {
        if (!def.help || name !== def.name) continue
        printHelpRow(name, def.help)
      }
      print(`  <span class="tf-hl">${'config [key] [value]'.padEnd(34)}</span><span class="tf-dim">View or change CLI settings, e.g. config glitter focus</span>`)
      print(`  <span class="tf-hl">${'matrix'.padEnd(34)}</span><span class="tf-dim">You know what this does</span>`)
      print(`  <span class="tf-hl">${'cowsay [message]'.padEnd(34)}</span><span class="tf-dim">The important one</span>`)
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
    }

    function printSudoHelp() {
      print(`<span class="tf-gold tf-bold">HIDDEN COMMANDS</span>  <span class="tf-dim">(not listed in help)</span>`)
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
      for (const [name, def] of hiddenCommands) {
        if (name !== def.name) continue
        printHelpRow(name, def.help || '')
      }
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
    }

    function cmdCowsay(args) {
      const msg = args.join(' ') || 'moo'
      const maxW = 38
      const words = msg.split(' ')
      const lines = []
      let cur = ''
      for (const w of words) {
        if (cur && cur.length + 1 + w.length > maxW) { lines.push(cur); cur = w }
        else { cur = cur ? cur + ' ' + w : w }
      }
      if (cur) lines.push(cur)
      const width = Math.max(...lines.map(l => l.length))
      const bar = '_'.repeat(width + 2)
      let bubble = ' ' + bar + '\n'
      lines.forEach((line, i) => {
        const padded = line.padEnd(width)
        if (lines.length === 1) bubble += `< ${padded} >\n`
        else if (i === 0)              bubble += `/ ${padded} \\\n`
        else if (i === lines.length-1) bubble += `\\ ${padded} /\n`
        else                           bubble += `| ${padded} |\n`
      })
      bubble += ' ' + '-'.repeat(width + 2)
      const cow = [
        '        \\   ^__^',
        '         \\  (oo)\\_______',
        '            (__)\\       )\\/\\',
        '                ||----w |',
        '                ||     ||',
      ].join('\n')
      print(`<pre style="font-family:'Courier New',monospace;font-size:12px;color:#7CFC00;line-height:1.45;margin:0;">${escHtml(bubble + '\n' + cow)}</pre>`)
    }

    const UJI_LOGO = [
      '     ___            ___               ',
      '    /\\__\\          /\\  \\        ___   ',
      '   /:/  /          \\:\\  \\      /\\  \\  ',
      '  /:/  /       ___ /::\\__\\     \\:\\  \\ ',
      ' /:/  /  ___  /\\  /:/\\/__/     /::\\__\\',
      '/:/__/  /\\__\\ \\:\\/:/  /     __/:/\\/__/',
      '\\:\\  \\ /:/  /  \\::/  /     /\\/:/  /   ',
      ' \\:\\  /:/  /    \\/__/      \\::/__/    ',
      '  \\:\\/:/  /                 \\:\\__\\    ',
      '   \\::/  /                   \\/__/    ',
      '    \\/__/                             ',
    ].join('\n')
    function cmdUji() {
      print(`<pre style="font-family:'Courier New',monospace;font-size:12px;color:#4CAF50;line-height:1.2;margin:0;">${escHtml(UJI_LOGO)}</pre>`)
    }

    // Accepts a plain number (ms) or a number suffixed with "s" (seconds).
    function parseDuration(str) {
      const m = String(str || '').trim().match(/^(\d+(?:\.\d+)?)(ms|s)?$/i)
      if (!m) return NaN
      const n = parseFloat(m[1])
      return m[2] && m[2].toLowerCase() === 's' ? n * 1000 : n
    }

    // ── built-in commands ───────────────────────────────────────────────
    // Routed through the same registry host apps use via registerCommand.
    // config/cowsay/matrix are left without a `.help` entry because printHelp
    // already documents them via dedicated static lines.
    registerCommand('help', { builtin: true, aliases: ['?'], run: () => printHelp() })
    registerCommand('sudohelp', { builtin: true, run: () => printSudoHelp() })
    registerCommand('clear', { builtin: true, aliases: ['cls'], help: ['clear / cls', 'Clear console output'], run: () => clearOutput() })
    registerCommand('exit', { builtin: true, aliases: ['q'], help: ['exit / q', 'Close admin console'], run: () => closePanel() })
    registerCommand('config', { builtin: true, run: (args) => cmdConfig(args) })
    registerCommand('cowsay', { builtin: true, run: (args) => cmdCowsay(args) })
    registerCommand('uji', { hidden: true, help: 'Print the UJIFrame logo', run: () => cmdUji() })
    registerCommand('sudo', { hidden: true, help: 'do superuser', run: () => println('superuser did') })
    registerCommand('wait', {
      builtin: true,
      run: async (args) => {
        const ms = parseDuration(args[0])
        if (!Number.isFinite(ms) || ms < 0) { println('Usage: wait <ms> or wait <N>s', 'tf-err'); return }
        println(`Waiting ${ms}ms...`, 'tf-dim')
        await new Promise(resolve => setTimeout(resolve, ms))
      },
    })
    registerCommand('matrix', {
      builtin: true,
      run: () => {
        startMatrixEgg('press any key')
        input.blur()
        setTimeout(() => {
          const stop = (e) => { e.preventDefault(); stopMatrixEgg(); document.removeEventListener('keydown', stop); input.focus() }
          document.addEventListener('keydown', stop)
        }, 50)
      },
    })
    registerCommand('startprocess', { hidden: true, help: 'Start the matrix loading animation without running anything', run: () => startLoading() })
    registerCommand('endprocess', { hidden: true, help: 'Stop the matrix loading animation started by startprocess', run: () => { stopLoading(); input.focus() } })
    registerCommand('startpurge', { hidden: true, help: 'Start the fire animation without running anything', run: () => startFire() })
    registerCommand('endpurge', { hidden: true, help: 'Stop the fire animation started by startpurge', run: async () => { await stopFire(); input.focus() } })
    registerCommand('startglitter', { hidden: true, help: 'Start the lively-mode firework preview, runs until stopped', run: () => startGlitterPreview() })
    registerCommand('endglitter', { hidden: true, help: 'Stop the fireworks started by startglitter', run: () => stopGlitterPreview() })

    // ── sequencing engine ────────────────────────────────────────────────
    const SUDOSEQ_PHRASE = TIER_PHRASES[4]
    let sudoMode = false
    let commandQueue = []
    let sequenceActive = false
    let sequenceAbort = false
    let currentStepIndex = -1
    let _queueListEl = null
    let seqMatrixOpts = null
    let chainOverlayActive = false

    function startQueueChecklist(cmds) {
      const div = document.createElement('div')
      div.style.margin = '6px 0'
      div.innerHTML = cmds.map((c, i) =>
        `<div data-qstep="${i}"><span class="tf-dim" data-qicon>○</span> <span class="tf-dim">${escHtml(c)}</span></div>`
      ).join('')
      output.appendChild(div)
      output.scrollTop = output.scrollHeight
      _queueListEl = div
    }
    function markQueueStep(i, ok) {
      if (!_queueListEl) return
      const icon = _queueListEl.querySelector(`[data-qstep="${i}"] [data-qicon]`)
      if (!icon) return
      icon.textContent = ok ? '✓' : '✗'
      icon.className = ok ? 'tf-ok' : 'tf-err'
      output.scrollTop = output.scrollHeight
    }
    function endQueueChecklist() { _queueListEl = null }

    async function runSingle(rawCmd) {
      const parts = rawCmd.trim().split(/\s+/)
      const cmd = parts[0].toLowerCase()
      const args = parts.slice(1)

      const def = commands.get(cmd) || hiddenCommands.get(cmd)
      if (def) { await def.run(args); return }
      println(`Unknown command: "${cmd}". Type "help" for a list.`, 'tf-err')
    }

    function finishQueue() {
      if (chainOverlayActive) stopLoading()
      chainOverlayActive = false
      sequenceActive = false
      sequenceAbort = false
      endQueueChecklist()
      currentStepIndex = -1
      seqMatrixOpts = null
      sudoMode = false
    }

    async function processQueue(queue) {
      commandQueue = queue
      currentStepIndex = -1
      sequenceActive = true
      startQueueChecklist(queue)
      if (CONFIG.chainOverlay !== 'off') {
        chainOverlayActive = true
        seqMatrixOpts = {
          speed: GLITTER_SPEED_MULT[CONFIG.glitterSpeed] || 1,
          rainbow: CONFIG.glitterRainbow === 'on' && (queue.length > 3 || sudoMode),
        }
        startLoading('RUNNING SEQUENCE...', seqMatrixOpts)
      }
      ensureSequenceWatchdog()
      while (commandQueue.length > 0 && !sequenceAbort) {
        const next = commandQueue.shift()
        currentStepIndex++
        printCmdEcho(next)
        try {
          await runSingle(next)
          markQueueStep(currentStepIndex, !sequenceAbort)
        } catch (e) {
          markQueueStep(currentStepIndex, false)
          println('Error: ' + (e && e.message ? e.message : e), 'tf-err')
        }
      }
      finishQueue()
    }

    async function dispatch(raw) {
      const trimmed = raw.trim()
      if (!trimmed) return

      history.unshift(trimmed)
      histIdx = -1

      if (pendingConfirm) {
        printCmdEcho(trimmed)
        resolvePendingConfirm(trimmed)
        return
      }

      if (/^sudoseq(\s|\||$)/i.test(trimmed)) {
        printCmdEcho(trimmed)
        const rest = trimmed.slice('sudoseq'.length).trim()
        const segments = rest.split('|').map(s => s.trim()).filter(Boolean)
        if (segments.length === 0) { println('Usage: sudoseq | cmd1 | cmd2 | ...', 'tf-warn'); return }
        println('⚠ This will run the following commands back-to-back and AUTOMATICALLY answer every "are you sure" / typed confirmation along the way — any destructive or irreversible commands in the chain will execute immediately with no pause to double check:', 'tf-warn')
        segments.forEach((s, i) => print(`  <span class="tf-dim">${i + 1}.</span> ${escHtml(s)}`))
        print(`Type <span class="tf-hl">${SUDOSEQ_PHRASE}</span> to confirm and run all ${segments.length} command${segments.length !== 1 ? 's' : ''}.`)
        pendingConfirm = {
          kind: 'tier',
          tier: 4,
          step: 1,
          resolve: (ok) => {
            if (!ok) { println('Cancelled.', 'tf-dim'); return }
            sudoMode = true
            processQueue(segments)
          },
        }
        return
      }

      const loopMatch = trimmed.match(/^loop\s+(\d+)\s+(.+)$/i)
      if (loopMatch) {
        printCmdEcho(trimmed)
        const count = parseInt(loopMatch[1], 10)
        const segments = loopMatch[2].split('|').map(s => s.trim()).filter(Boolean)
        if (count <= 0) { println('loop count must be a positive integer.', 'tf-err'); return }
        if (segments.length === 0) { println('Usage: loop <count> <command> [| command2 | ...]', 'tf-warn'); return }
        const maxLoop = MAX_LOOP_VALUES[CONFIG.maxLoop]
        if (count > maxLoop) {
          println(`loop count ${count} exceeds the "${CONFIG.maxLoop}" max (${maxLoop}). Switch with "config maxLoop freedom" for no cap.`, 'tf-err')
          return
        }
        const queue = []
        for (let i = 0; i < count; i++) queue.push(...segments)
        await processQueue(queue)
        return
      }

      if (trimmed.includes('|')) {
        printCmdEcho(trimmed)
        await processQueue(trimmed.split('|').map(s => s.trim()).filter(Boolean))
        return
      }

      printCmdEcho(trimmed)
      await runSingle(trimmed)
    }

    // ── panel lifecycle + input wiring ──────────────────────────────────
    let history = [], histIdx = -1

    function openPanel() {
      panel.classList.add('open')
      setTimeout(() => input.focus(), 50)
      if (output.children.length === 0) printWelcome()
    }
    function closePanel() {
      panel.classList.remove('open')
      // Deliberately leave pendingConfirm/sequence state alone — closing the
      // panel shouldn't abort an in-progress sequence.
    }

    input.addEventListener('keydown', e => {
      if (e.key === 'Enter') {
        const val = input.value
        input.value = ''
        dispatch(val)
      } else if (e.key === 'ArrowUp') {
        e.preventDefault()
        if (histIdx < history.length - 1) { histIdx++; input.value = history[histIdx] }
      } else if (e.key === 'ArrowDown') {
        e.preventDefault()
        if (histIdx > 0) { histIdx--; input.value = history[histIdx] }
        else { histIdx = -1; input.value = '' }
      } else if (e.key === 'Escape') {
        closePanel()
      }
    })

    panel.addEventListener('click', e => { if (e.target === panel) closePanel() })

    output.addEventListener('click', e => {
      const span = e.target.closest('.tf-id')
      if (!span) return
      const val = span.textContent.trim()
      const cur = input.value
      input.value = cur + (cur.length > 0 && !cur.endsWith(' ') ? ' ' : '') + val
      input.focus()
    })

    if (opts.trigger === 'konami') {
      let konamiIdx = 0
      document.addEventListener('keydown', e => {
        const isOpen = panel.classList.contains('open')
        if (isOpen && !pendingKonamiOpen) return
        if (pendingKonamiOpen) e.preventDefault()
        if (e.key === KONAMI[konamiIdx]) {
          konamiIdx++
          if (konamiIdx === KONAMI.length) {
            konamiIdx = 0
            if (pendingKonamiOpen) {
              const resolve = pendingKonamiOpen
              pendingKonamiOpen = null
              resolve(true)
              input.focus()
            } else {
              openPanel()
            }
          }
        } else {
          konamiIdx = e.key === KONAMI[0] ? 1 : 0
          if (pendingKonamiOpen && e.key === 'Escape') {
            const resolve = pendingKonamiOpen
            pendingKonamiOpen = null
            println('Cancelled.', 'tf-dim')
            resolve(false)
            input.focus()
          }
        }
      })
    }

    return {
      registerCommand,
      registerConfig,
      confirm,
      confirmYesNo,
      confirmKonami,
      print,
      println,
      escHtml,
      startLoading,
      stopLoading,
      startFire,
      stopFire,
      open: openPanel,
      close: closePanel,
      run: dispatch,
    }
  }

  global.UJIFrame = { create }
})(window)
