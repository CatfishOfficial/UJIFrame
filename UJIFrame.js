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
#${id}-box{--tf-accent:#4CAF50;--tf-glow:#00ff41;--tf-bg:rgba(0,12,0,0.82);--tf-dim:rgba(0,255,65,0.25);width:860px;max-width:96vw;height:580px;max-height:90vh;background:#0c0c0c;border:1px solid #2a2a2a;border-radius:7px;display:flex;flex-direction:column;font-family:'Courier New',Courier,monospace;box-shadow:0 24px 80px rgba(0,0,0,0.8);position:relative;transform-origin:center;}
#${id}.open #${id}-box.tf-boot-anim{animation:tf-boot 0.5s ease-out;}
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
@keyframes tf-boot{
  0%{opacity:0;transform:scale(0.9);filter:brightness(2.4) saturate(1.8) hue-rotate(0deg);}
  8%{opacity:1;filter:brightness(2.2) saturate(2) hue-rotate(60deg);}
  16%{opacity:0.15;filter:brightness(1.8) saturate(2) hue-rotate(140deg);}
  24%{opacity:1;transform:scale(1.015);filter:brightness(2) saturate(2.2) hue-rotate(220deg);}
  32%{opacity:0.4;filter:brightness(1.6) saturate(2) hue-rotate(290deg);}
  40%{opacity:1;transform:scale(1);filter:brightness(1.5) saturate(1.6) hue-rotate(360deg);}
  70%{filter:brightness(1.1) saturate(1.1) hue-rotate(0deg);}
  100%{opacity:1;transform:scale(1);filter:brightness(1) saturate(1) hue-rotate(0deg);}
}
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
    // CONFIG.maxLoop is normally 'safe'/'freedom', but sudoconfig can set it
    // to an exact integer string instead — resolve either form to a number.
    function resolveMaxLoop() {
      if (CONFIG.maxLoop in MAX_LOOP_VALUES) return MAX_LOOP_VALUES[CONFIG.maxLoop]
      const n = parseInt(CONFIG.maxLoop, 10)
      return Number.isFinite(n) && n >= 0 ? n : MAX_LOOP_VALUES.safe
    }
    // UI strings, localizable via sudoconfig only — `lang` is deliberately not
    // in CONFIG_SCHEMA, so `config` never lists or accepts it. Config values
    // themselves (on/off, lively/focus/static, etc.) stay in English always —
    // they're command syntax, not prose, so localizing them would break typing.
    const STRINGS = {
      en: {
        escToClose: 'Esc or click outside to close',
        welcomeHint: 'Type a command and press Enter. Chain commands with | (e.g. cmd1 | cmd2). Type "help" for the full command list.',
        helpShowAll: 'Show every available command',
        helpChain: 'Run commands in sequence — any confirmations still prompt, sequence resumes after',
        helpSudoseq: 'Sequence + auto-answer every confirmation',
        helpLoop: 'Repeat a command (or chain) n times',
        helpWait: 'Pause a chain, e.g. wait 2000 or wait 2s',
        helpConfig: 'View or change CLI settings, e.g. config glitter focus',
        helpMatrix: 'You know what this does',
        helpCowsay: 'The important one',
        hiddenCommandsTitle: 'HIDDEN COMMANDS',
        notListedInHelp: '(not listed in help)',
        currentConfig: 'Current config:',
        notAConfigOption: (key) => `"${key}" is not a config option. Type "config" to see available options.`,
        invalidValueEnum: (key, allowed) => `Invalid value. "${key}" must be one of: ${allowed}`,
        whoUsesLightMode: 'who uses light mode?',
        configSet: (key, value) => `✓ ${key} set to "${value}".`,
        boringgg: 'boringgg~ :(',
        sudoconfigUsage: 'Usage: sudoconfig <key> <exact value> — bypasses the preset list for keys that support it.',
        sudoconfigUnsupported: (key, list) => `"${key}" doesn't support exact values via sudoconfig. Supported: ${list}.`,
        sudoconfigMaxLoopError: 'maxLoop must be a positive integer (e.g. sudoconfig maxLoop 420).',
        sudoconfigMaxLoopNegative: 'maxLoop must be positive. For negative loops, please install UJI Chronomancy.',
        sudoconfigMaxLoopZero: 'but why?',
        sudoconfigColorError: 'color must be a 6-digit hex value (e.g. sudoconfig color ff8800).',
        sudoconfigLangError: 'lang must be "en" or "ja" (e.g. sudoconfig lang ja).',
        sudoconfigSet: (key, value) => `✓ ${key} set to "${value}" (exact value via sudoconfig).`,
        sudoconfigValueList: (key, list) => `Supported values for "${key}": ${list}`,
        unknownCommand: (cmd) => `Unknown command: "${cmd}". Type "help" for a list.`,
        cancelled: 'Cancelled.',
        confirmTimedOut: 'Confirmation timed out — cancelled.',
        typeYesToConfirm: 'Type "yes" to confirm or anything else to cancel.',
        typeToConfirmStep: (phrase) => `Type <span class="tf-hl">${phrase}</span> to confirm (1/2).`,
        typeToConfirm: (phrase) => `Type <span class="tf-hl">${phrase}</span> to confirm.`,
        confirmedOnceStep2: (phrase) => `Confirmed once. Type <span class="tf-hl">${phrase}</span> to confirm (2/2).`,
        sudoseqUsage: 'Usage: sudoseq | cmd1 | cmd2 | ...',
        sudoseqWarning: '⚠ This will run the following commands back-to-back and AUTOMATICALLY answer every "are you sure" / typed confirmation along the way — any destructive or irreversible commands in the chain will execute immediately with no pause to double check:',
        sudoseqConfirm: (phrase, count) => `Type <span class="tf-hl">${phrase}</span> to confirm and run all ${count} command${count !== 1 ? 's' : ''}.`,
        loopCountInvalid: 'loop count must be a positive integer.',
        loopUsage: 'Usage: loop <count> <command> [| command2 | ...]',
        loopExceeds: (count, maxLoop, raw) => `loop count ${count} exceeds the current max (${maxLoop}, maxLoop="${raw}"). Switch with "config maxLoop freedom" or "sudoconfig maxLoop <n>".`,
        waitUsage: 'Usage: wait <ms> or wait <N>s',
        waiting: (ms) => `Waiting ${ms}ms...`,
        errorPrefix: 'Error: ',
        processingLabel: 'PROCESSING...',
        runningSequenceLabel: 'RUNNING SEQUENCE...',
        purgingLabel: 'PURGING...',
        pressAnyKey: 'press any key',
        sudoseqAutoConfirmLabel: '$ (sudoseq auto-confirm)',
        helpClearDesc: 'Clear console output',
        helpExitDesc: 'Close admin console',
        itsExit: 'its exit',
        seriouslyItsExit: 'seriously, its exit',
        fineIllDoItForYou: 'fine, ill do it for you',
        helpUji: 'Print the UJIFrame logo',
        helpSudo: 'do superuser',
        sudoOutput: 'superuser did',
        helpSudoconfig: 'sudoconfig <key> <exact value> — bypass the preset list (maxLoop, color, lang)',
        helpStartprocess: 'Start the matrix loading animation without running anything',
        helpEndprocess: 'Stop the matrix loading animation started by startprocess',
        helpStartpurge: 'Start the fire animation without running anything',
        helpEndpurge: 'Stop the fire animation started by startpurge',
        helpStartglitter: 'Start the lively-mode firework preview, runs until stopped',
        helpEndglitter: 'Stop the fireworks started by startglitter',
        helpCalc: 'calc <expr> evaluates math (+ - * / % ^ ! sqrt sin cos tan log ln abs pow, pi/e/ans). calc solve <eqn> solves linear/quadratic equations; calc solve <eqn1>, <eqn2> solves a 2-variable linear system.',
        calcUsage: 'Usage: calc <expression> or calc solve <equation>[, <equation2>], e.g. calc sqrt(16), calc solve x^2-5x+6=0, calc solve x+y=10, x-y=2',
        calcDivByZero: 'Division by zero',
        calcUnknownFn: (name) => `Unknown function "${name}"`,
        calcUnknownIdent: (name) => `Unknown identifier "${name}"`,
        calcNoAns: 'No previous result — "ans" is not set yet',
        calcFactorialBad: 'Factorial requires a non-negative integer',
        calcExpected: (tok) => `Expected "${tok}"`,
        calcUnexpectedEnd: 'Unexpected end of expression',
        calcUnexpectedChar: (c) => `Unexpected character "${c}"`,
        calcInvalidNumber: (s) => `Invalid number "${s}"`,
        calcUnexpectedToken: (v) => `Unexpected token "${v}"`,
        calcOr: 'or',
        calcAlgebraParseError: (s) => `Couldn't parse "${s}" as a term — solve expects plain polynomial terms like 3x^2, -5x, 7`,
        calcAlgebraNoEquals: 'Equation must contain "="',
        calcAlgebraNoVariable: 'No variable found to solve for',
        calcAlgebraTooManyVars: 'solve only supports one variable per equation (use a comma-separated pair for a 2-variable system)',
        calcAlgebraDegreeUnsupported: 'Only linear and quadratic (degree ≤ 2) terms are supported',
        calcAlgebraSystemVars: 'A 2-equation system must use exactly two variables total',
        calcAlgebraNoUniqueSolution: 'This system has no unique solution (equations are parallel or identical)',
        calcAlgebraTooManyEquations: 'solve supports at most two comma-separated equations',
        calcAlgebraIdentity: 'True for all values (identity)',
        calcAlgebraNoSolution: 'No solution',
        calcUnitMismatch: (from, to) => `Can't convert "${from}" to "${to}" — unknown or incompatible units`,
        helpHistory: 'List past commands (use !n to re-run #n)',
        helpBang: 'Re-run command #n from "history"',
        historyEmpty: 'No history yet.',
        historyNoSuchEntry: (n) => `No history entry #${n}.`,
        helpAlias: 'alias to view/list, alias name=cmd to define',
        helpUnalias: 'Remove a user-defined alias',
        aliasNone: 'No aliases defined.',
        aliasUsage: 'Usage: alias name=command (e.g. alias gco=cowsay)',
        aliasNotFound: (name) => `No alias named "${name}".`,
        aliasShadowsCommand: (name) => `"${name}" is already a real command — can't alias over it.`,
        aliasSet: (name, value) => `✓ alias ${name}="${value}"`,
        unaliasUsage: 'Usage: unalias <name>',
        unaliasDone: (name) => `✓ Removed alias "${name}".`,
        helpFind: 'Filter the console output, e.g. find error — find with no term clears it',
        findCleared: 'Filter cleared — showing everything.',
        findMatches: (n) => `${n} matching line${n !== 1 ? 's' : ''} shown, rest hidden. "find" with no term to clear.`,
        helpExport: 'Download the current console output as a .txt file',
        exportDone: 'Log downloaded.',
        settingsTease: "awwww that's cute",
        tabTypeSomethingFirst: 'try typing something first',
        helpPurge: 'Clear saved data — purge config, purge history, purge aliases, or purge all',
        purgeUsage: (targets) => `Usage: purge <target>, where target is one of: ${targets}`,
        purgeUnknownTarget: (target, targets) => `"${target}" isn't a purge target. Try one of: ${targets}`,
        purgeConfirmOne: (target) => `This will permanently delete your saved ${target} and reset it to defaults.`,
        purgeConfirmAll: 'This will permanently delete ALL saved data for this console — config, history, and aliases — and reset everything to defaults.',
        purgeDone: (target) => `✓ ${target} purged.`,
        purgeAllDone: '✓ Everything purged. Fresh start.',
        calcHelpTitle: 'CALC — quick reference',
        calcHelpArithmetic: 'Arithmetic:  +  -  *  /  %  ^ (power)  ! (factorial)',
        calcHelpArithmeticEx: 'e.g. calc 2^10 → 1024     calc 5! → 120     calc (2+3)*4 → 20',
        calcHelpFunctions: 'Functions:  sqrt sin cos tan asin acos atan log(base10) ln(natural) abs floor ceil round exp max min pow',
        calcHelpFunctionsEx: 'e.g. calc sqrt(16) → 4     calc max(3,9,5) → 9     calc pow(2,10) → 1024',
        calcHelpConstants: 'Constants:  pi   e   ans (remembers your last result)',
        calcHelpConstantsEx: 'e.g. calc pi * 2     calc ans + 1',
        calcHelpAlgebra: 'Algebra:  calc solve <equation> — solves a linear or quadratic equation in one variable',
        calcHelpAlgebraEx: 'e.g. calc solve 2x+5=3x-1 → x = 6     calc solve x^2-5x+6=0 → x = 3 or 2',
        calcHelpSystem: 'Systems:  calc solve <eqn1>, <eqn2> — solves a 2-variable linear system',
        calcHelpSystemEx: 'e.g. calc solve x+y=10, x-y=2 → x = 6, y = 4',
        calcHelpUnits: 'Unit conversion:  calc <n> <unit> to <unit> — length (m km cm mm mi yd ft in), mass (kg g mg lb oz), time (s min h day), temperature (c f k)',
        calcHelpUnitsEx: 'e.g. calc 5 km to mi     calc 100 c to f',
        calcHelpFooter: 'Type calc <expression> to evaluate it now, or calc solve ... to do algebra.',
        cowsayHints: [
          'have you tried the konami code?',
          'sudo makes things... interesting.',
          'not everything is in the help menu.',
          'try pressing tab sometime.',
          'this terminal remembers more than you would think.',
          'you can teach it new tricks. look into alias.',
          'math class flashbacks? try solving something.',
          'exclamation marks are more useful than you would think.',
          'some commands only answer to sudo.',
          'try asking for the settings. see what happens.',
        ],
      },
      ja: {
        escToClose: 'Escまたは外側をクリックして閉じる',
        welcomeHint: 'コマンドを入力してEnterを押してください。| でコマンドを連結できます(例: cmd1 | cmd2)。全コマンド一覧は "help" と入力。',
        helpShowAll: '利用可能な全コマンドを表示',
        helpChain: 'コマンドを順番に実行 — 確認が必要な場合は都度表示され、その後続行',
        helpSudoseq: '連続実行 + すべての確認に自動で答える',
        helpLoop: 'コマンド(またはチェーン)をn回繰り返す',
        helpWait: 'チェーンを一時停止。例: wait 2000 または wait 2s',
        helpConfig: 'CLI設定の表示・変更。例: config glitter focus',
        helpMatrix: 'もうお分かりですね',
        helpCowsay: 'これが一番重要',
        hiddenCommandsTitle: '隠しコマンド',
        notListedInHelp: '(helpには表示されません)',
        currentConfig: '現在の設定:',
        notAConfigOption: (key) => `"${key}" は設定項目ではありません。利用可能な項目は "config" と入力して確認してください。`,
        invalidValueEnum: (key, allowed) => `無効な値です。"${key}" は次のいずれかを指定してください: ${allowed}`,
        whoUsesLightMode: 'ライトモードなんて誰が使うの?',
        configSet: (key, value) => `✓ ${key} を "${value}" に設定しました。`,
        boringgg: 'つまらない~ :(',
        sudoconfigUsage: '使い方: sudoconfig <キー> <正確な値> — 対応するキーのプリセット一覧をバイパスします。',
        sudoconfigUnsupported: (key, list) => `"${key}" はsudoconfigでの正確な値の指定に対応していません。対応キー: ${list}。`,
        sudoconfigMaxLoopError: 'maxLoopは正の整数を指定してください(例: sudoconfig maxLoop 420)。',
        sudoconfigMaxLoopNegative: 'maxLoopは正の値にしてください。負のループをご希望の場合は「UJI Chronomancy」をインストールしてください。',
        sudoconfigMaxLoopZero: 'でも、なんで?',
        sudoconfigColorError: 'colorは6桁の16進数値を指定してください(例: sudoconfig color ff8800)。',
        sudoconfigLangError: 'langは "en" または "ja" を指定してください(例: sudoconfig lang ja)。',
        sudoconfigSet: (key, value) => `✓ ${key} を "${value}" に設定しました(sudoconfigによる正確な値)。`,
        sudoconfigValueList: (key, list) => `"${key}" で対応している値: ${list}`,
        unknownCommand: (cmd) => `不明なコマンドです: "${cmd}"。一覧は "help" と入力してください。`,
        cancelled: 'キャンセルしました。',
        confirmTimedOut: '確認がタイムアウトしました — キャンセルしました。',
        typeYesToConfirm: '"yes" と入力して確認、それ以外でキャンセルします。',
        typeToConfirmStep: (phrase) => `<span class="tf-hl">${phrase}</span> と入力して確認してください (1/2)。`,
        typeToConfirm: (phrase) => `<span class="tf-hl">${phrase}</span> と入力して確認してください。`,
        confirmedOnceStep2: (phrase) => `一度確認しました。<span class="tf-hl">${phrase}</span> と入力してください (2/2)。`,
        sudoseqUsage: '使い方: sudoseq | cmd1 | cmd2 | ...',
        sudoseqWarning: '⚠ 以下のコマンドを連続して実行し、すべての確認を自動的に承認します — 破壊的または取り消せない操作も一切確認なしで即実行されます:',
        sudoseqConfirm: (phrase, count) => `<span class="tf-hl">${phrase}</span> と入力すると、${count}件のコマンドをすべて実行します。`,
        loopCountInvalid: 'loopの回数は正の整数で指定してください。',
        loopUsage: '使い方: loop <回数> <コマンド> [| コマンド2 | ...]',
        loopExceeds: (count, maxLoop, raw) => `指定回数 ${count} は現在の上限(${maxLoop}, maxLoop="${raw}")を超えています。"config maxLoop freedom" または "sudoconfig maxLoop <n>" で変更してください。`,
        waitUsage: '使い方: wait <ミリ秒> または wait <N>s',
        waiting: (ms) => `${ms}ms 待機中...`,
        errorPrefix: 'エラー: ',
        processingLabel: '処理中...',
        runningSequenceLabel: '実行中...',
        purgingLabel: 'パージ中...',
        pressAnyKey: '何かキーを押してください',
        sudoseqAutoConfirmLabel: '$ (sudoseq自動確認)',
        helpClearDesc: 'コンソールの表示をクリア',
        helpExitDesc: '管理コンソールを閉じる',
        itsExit: 'それは exit だよ',
        seriouslyItsExit: 'マジで、exit だってば',
        fineIllDoItForYou: 'はいはい、代わりにやってあげるよ',
        helpUji: 'UJIFrameのロゴを表示',
        helpSudo: 'スーパーユーザーを実行',
        sudoOutput: 'スーパーユーザーが実行しました',
        helpSudoconfig: 'sudoconfig <キー> <正確な値> — プリセット一覧をバイパス(maxLoop, color, lang)',
        helpStartprocess: '何も実行せずにマトリックスのローディングアニメーションを開始',
        helpEndprocess: 'startprocessで開始したマトリックスアニメーションを停止',
        helpStartpurge: '何も実行せずに炎のアニメーションを開始',
        helpEndpurge: 'startpurgeで開始した炎のアニメーションを停止',
        helpStartglitter: 'livelyモードの花火プレビューを開始(停止するまで継続)',
        helpEndglitter: 'startglitterで開始した花火を停止',
        helpCalc: 'calc <式> で計算(+ - * / % ^ ! sqrt sin cos tan log ln abs pow、pi/e/ans)。calc solve <方程式> で一次・二次方程式を解く。calc solve <方程式1>, <方程式2> で2変数連立一次方程式を解く。',
        calcUsage: '使い方: calc <式> または calc solve <方程式>[, <方程式2>]、例: calc sqrt(16)、calc solve x^2-5x+6=0、calc solve x+y=10, x-y=2',
        calcDivByZero: '0で除算しています',
        calcUnknownFn: (name) => `不明な関数です: "${name}"`,
        calcUnknownIdent: (name) => `不明な識別子です: "${name}"`,
        calcNoAns: '直前の結果がありません — "ans" はまだ設定されていません',
        calcFactorialBad: '階乗には0以上の整数が必要です',
        calcExpected: (tok) => `"${tok}" が必要です`,
        calcUnexpectedEnd: '式が予期せず終了しました',
        calcUnexpectedChar: (c) => `予期しない文字です: "${c}"`,
        calcInvalidNumber: (s) => `不正な数値です: "${s}"`,
        calcUnexpectedToken: (v) => `予期しないトークンです: "${v}"`,
        calcOr: 'または',
        calcAlgebraParseError: (s) => `"${s}" を項として解析できませんでした — solveは 3x^2, -5x, 7 のような単純な多項式の項のみ対応しています`,
        calcAlgebraNoEquals: '方程式には "=" が必要です',
        calcAlgebraNoVariable: '解くべき変数が見つかりません',
        calcAlgebraTooManyVars: 'solveは1つの方程式につき変数1つのみ対応しています(2変数の場合はカンマで区切った連立方程式を使ってください)',
        calcAlgebraDegreeUnsupported: '一次・二次(次数2以下)の項のみ対応しています',
        calcAlgebraSystemVars: '2元連立方程式には合計でちょうど2つの変数が必要です',
        calcAlgebraNoUniqueSolution: 'この連立方程式には一意の解がありません(平行または同一の方程式です)',
        calcAlgebraTooManyEquations: 'solveはカンマ区切りで最大2つの方程式までです',
        calcAlgebraIdentity: 'すべての値で成り立ちます(恒等式)',
        calcAlgebraNoSolution: '解はありません',
        calcUnitMismatch: (from, to) => `"${from}" を "${to}" に変換できません — 不明または互換性のない単位です`,
        helpHistory: '過去のコマンドを一覧表示(!n で#nを再実行)',
        helpBang: '"history" の#nのコマンドを再実行',
        historyEmpty: '履歴はまだありません。',
        historyNoSuchEntry: (n) => `履歴 #${n} は存在しません。`,
        helpAlias: 'alias で一覧表示、alias name=cmd で定義',
        helpUnalias: 'ユーザー定義のaliasを削除',
        aliasNone: 'aliasは定義されていません。',
        aliasUsage: '使い方: alias name=command(例: alias gco=cowsay)',
        aliasNotFound: (name) => `"${name}" というaliasはありません。`,
        aliasShadowsCommand: (name) => `"${name}" は既存の実コマンドです — aliasで上書きできません。`,
        aliasSet: (name, value) => `✓ alias ${name}="${value}"`,
        unaliasUsage: '使い方: unalias <name>',
        unaliasDone: (name) => `✓ alias "${name}" を削除しました。`,
        helpFind: 'コンソール出力を絞り込み。例: find error — 引数なしで解除',
        findCleared: '絞り込みを解除しました — すべて表示中。',
        findMatches: (n) => `${n}件の一致行を表示、残りは非表示。引数なしの "find" で解除。`,
        helpExport: '現在のコンソール出力を.txtファイルとしてダウンロード',
        exportDone: 'ログをダウンロードしました。',
        settingsTease: 'あらあら、可愛いね〜',
        tabTypeSomethingFirst: 'まず何か入力してみて',
        helpPurge: '保存データを削除 — purge config、purge history、purge aliases、または purge all',
        purgeUsage: (targets) => `使い方: purge <対象>。対象は次のいずれか: ${targets}`,
        purgeUnknownTarget: (target, targets) => `"${target}" はpurgeの対象ではありません。次のいずれかを指定してください: ${targets}`,
        purgeConfirmOne: (target) => `保存されている ${target} を完全に削除し、初期設定に戻します。`,
        purgeConfirmAll: 'このコンソールの保存データ(config・history・alias)をすべて完全に削除し、初期状態に戻します。',
        purgeDone: (target) => `✓ ${target} を削除しました。`,
        purgeAllDone: '✓ すべて削除しました。まっさらな状態です。',
        calcHelpTitle: 'CALC — クイックリファレンス',
        calcHelpArithmetic: '四則演算:  +  -  *  /  %  ^ (累乗)  ! (階乗)',
        calcHelpArithmeticEx: '例: calc 2^10 → 1024     calc 5! → 120     calc (2+3)*4 → 20',
        calcHelpFunctions: '関数:  sqrt sin cos tan asin acos atan log(常用対数) ln(自然対数) abs floor ceil round exp max min pow',
        calcHelpFunctionsEx: '例: calc sqrt(16) → 4     calc max(3,9,5) → 9     calc pow(2,10) → 1024',
        calcHelpConstants: '定数:  pi   e   ans (直前の結果を記憶)',
        calcHelpConstantsEx: '例: calc pi * 2     calc ans + 1',
        calcHelpAlgebra: '代数:  calc solve <方程式> — 一次・二次方程式(変数1つ)を解く',
        calcHelpAlgebraEx: '例: calc solve 2x+5=3x-1 → x = 6     calc solve x^2-5x+6=0 → x = 3 または 2',
        calcHelpSystem: '連立方程式:  calc solve <方程式1>, <方程式2> — 2変数連立一次方程式を解く',
        calcHelpSystemEx: '例: calc solve x+y=10, x-y=2 → x = 6, y = 4',
        calcHelpUnits: '単位変換:  calc <数値> <単位> to <単位> — 長さ(m km cm mm mi yd ft in)、質量(kg g mg lb oz)、時間(s min h day)、温度(c f k)',
        calcHelpUnitsEx: '例: calc 5 km to mi     calc 100 c to f',
        calcHelpFooter: 'calc <式> で今すぐ計算、または calc solve ... で代数を解けます。',
        cowsayHints: [
          'コナミコマンドって試した?',
          'sudoをつけると…面白いことが起きるかも。',
          'helpに全部書いてあるわけじゃないよ。',
          'Tabキー、押してみた?',
          'このターミナル、思ったより色々覚えてるよ。',
          '新しい技を教えられるよ。aliasを調べてみて。',
          '数学の授業を思い出す?何か解いてみたら?',
          '!マークって意外と便利だよ。',
          'sudoにしか答えないコマンドもあるんだ。',
          '「設定」って聞いてみたら?何が起きるかな。',
        ],
      },
    }
    function t(key, ...args) {
      const dict = STRINGS[CONFIG.lang] || STRINGS.en
      const entry = key in dict ? dict[key] : STRINGS.en[key]
      return typeof entry === 'function' ? entry(...args) : entry
    }

    // Tracked separately from CONFIG_SCHEMA so `purge config` can restore
    // exactly these values — including any a host added via registerConfig.
    const CONFIG_DEFAULTS = {
      glitter: opts.glitter,
      color: opts.color,
      timestamps: 'off',
      confirmTimeout: 'never',
      chainOverlay: 'on',
      glitterSpeed: 'normal',
      glitterRainbow: 'on',
      maxLoop: 'safe',
      lang: 'en',
    }
    let CONFIG = { ...CONFIG_DEFAULTS }
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
    function hexToRgb(hex) {
      const m = String(hex || '').replace(/^#/, '').match(/^([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i)
      if (!m) return null
      return { r: parseInt(m[1], 16), g: parseInt(m[2], 16), b: parseInt(m[3], 16) }
    }
    // sudoconfig's exact-value escape hatch for `color` — derives a full
    // theme (bg/dim) from a single hex instead of picking a named palette.
    function applyCustomTheme(hex) {
      const rgb = hexToRgb(hex)
      if (!rgb) return false
      const { r, g, b } = rgb
      const normalized = `#${hex.replace(/^#/, '').toLowerCase()}`
      box.style.setProperty('--tf-accent', normalized)
      box.style.setProperty('--tf-glow', normalized)
      box.style.setProperty('--tf-bg', `rgba(${Math.round(r * 0.08)},${Math.round(g * 0.08)},${Math.round(b * 0.08)},0.82)`)
      box.style.setProperty('--tf-dim', `rgba(${r},${g},${b},0.25)`)
      return true
    }
    if (CONFIG_SCHEMA.color.includes(CONFIG.color)) {
      applyTheme(CONFIG.color)
    } else if (!applyCustomTheme(CONFIG.color)) {
      applyTheme('green')
    }

    function saveConfig() {
      try { localStorage.setItem(opts.storageKey, JSON.stringify(CONFIG)) } catch (e) {}
    }
    function registerConfig(key, allowed, defaultValue) {
      CONFIG_SCHEMA[key] = allowed
      CONFIG_DEFAULTS[key] = defaultValue
      if (!(key in CONFIG)) CONFIG[key] = defaultValue
    }

    function cmdConfig(args) {
      if (args.length === 0) {
        println(t('currentConfig'), 'tf-dim')
        Object.entries(CONFIG_SCHEMA).forEach(([key, allowed]) => {
          print(`  <span class="tf-hl">${key.padEnd(20)}</span>${escHtml(CONFIG[key])}  <span class="tf-dim">(${allowed.join(' | ')})</span>`)
        })
        return
      }
      const [key, value] = args
      if (!(key in CONFIG_SCHEMA)) {
        println(t('notAConfigOption', key), 'tf-err')
        return
      }
      if (!value) {
        println(`${key} = ${CONFIG[key]}`, 'tf-dim')
        return
      }
      const allowed = CONFIG_SCHEMA[key]
      if (!allowed.includes(value)) {
        println(t('invalidValueEnum', key, allowed.join(', ')), 'tf-err')
        return
      }
      if (key === 'color' && value === 'lightmode') {
        println(t('whoUsesLightMode'), 'tf-dim')
        return
      }
      const prevValue = CONFIG[key]
      CONFIG[key] = value
      saveConfig()
      if (key === 'glitter') GLITTER = value
      if (key === 'color') applyTheme(value)
      println(t('configSet', key, value), 'tf-ok')
      if (key === 'glitter' && value === 'lively' && prevValue !== 'lively') {
        playLivelyPreview()
      }
      if (key === 'glitter' && prevValue === 'lively' && value === 'focus') {
        println(t('boringgg'), 'tf-dim')
      }
    }

    // Exact-value escape hatch for config keys that `config` only exposes as
    // a fixed preset list (e.g. maxLoop's safe/freedom, color's named palette).
    const SUDOCONFIG_HANDLERS = {
      maxLoop: {
        validate: (v) => /^(0|[1-9]\d*)$/.test(v),
        apply: (v) => { CONFIG.maxLoop = v },
        // Negative input gets its own joke instead of the generic error.
        error: (v) => /^-\d+$/.test(v) ? t('sudoconfigMaxLoopNegative') : t('sudoconfigMaxLoopError'),
      },
      color: {
        validate: (v) => !!hexToRgb(v),
        apply: (v) => { CONFIG.color = `#${v.replace(/^#/, '').toLowerCase()}`; applyCustomTheme(v) },
        error: () => t('sudoconfigColorError'),
      },
      // The one bit of UI localization in UJIFrame — deliberately not exposed
      // via `config` (CONFIG_SCHEMA has no `lang` key), only sudoconfig.
      // `values` is a known/finite list, so a missing value can list it
      // (unlike maxLoop/color, which take arbitrary numbers/hex and just
      // fall back to the generic usage message instead).
      lang: {
        values: ['en', 'ja'],
        validate: (v) => SUDOCONFIG_HANDLERS.lang.values.includes(v),
        apply: (v) => { CONFIG.lang = v },
        error: () => t('sudoconfigLangError'),
      },
    }
    function cmdSudoConfig(args) {
      const [key, value] = args
      if (!key) {
        println(t('sudoconfigUsage'), 'tf-warn')
        return
      }
      const handler = SUDOCONFIG_HANDLERS[key]
      if (!handler) {
        println(t('sudoconfigUnsupported', key, Object.keys(SUDOCONFIG_HANDLERS).join(', ')), 'tf-err')
        return
      }
      if (!value) {
        if (handler.values) println(t('sudoconfigValueList', key, handler.values.join(', ')), 'tf-ok')
        else println(t('sudoconfigUsage'), 'tf-warn')
        return
      }
      if (!handler.validate(value)) {
        println(handler.error(value), 'tf-err')
        return
      }
      handler.apply(value)
      saveConfig()
      println(t('sudoconfigSet', key, value), 'tf-ok')
      if (key === 'maxLoop' && value === '0') println(t('sudoconfigMaxLoopZero'), 'tf-dim')
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
      label = label || t('processingLabel')
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
      if (GLITTER === 'focus') { showSimpleOverlay(t('purgingLabel'), true); return }
      if (GLITTER === 'static') { showSimpleOverlay(t('purgingLabel'), false); return }
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
          print(t('typeYesToConfirm'))
        } else if (tier === 5) {
          print(t('typeToConfirmStep', TIER_PHRASES[2]))
        } else {
          print(t('typeToConfirm', TIER_PHRASES[tier]))
        }
        pendingConfirm = { kind: 'tier', tier, step: 1, resolve }
      })
    }
    function confirmYesNo(promptHtml) {
      if (promptHtml) print(promptHtml)
      return new Promise(resolve => {
        if (sudoMode) {
          printCmdEcho('y', t('sudoseqAutoConfirmLabel'))
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
        printCmdEcho(TIER_PHRASES[2], t('sudoseqAutoConfirmLabel'))
        printCmdEcho(TIER_PHRASES[3], t('sudoseqAutoConfirmLabel'))
        resolve(true)
        return
      }
      const phrase = tier === 1 ? 'yes' : TIER_PHRASES[tier]
      printCmdEcho(phrase, t('sudoseqAutoConfirmLabel'))
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
          if (!ok) println(t('cancelled'), 'tf-dim')
          pc.resolve(ok)
          return
        }
        if (pc.tier === 5 && pc.step === 1) {
          if (answer !== TIER_PHRASES[2]) { println(t('cancelled'), 'tf-dim'); pc.resolve(false); return }
          print(t('confirmedOnceStep2', TIER_PHRASES[3]))
          pendingConfirm = { kind: 'tier', tier: 5, step: 2, resolve: pc.resolve }
          return
        }
        const expected = pc.tier === 5 ? TIER_PHRASES[3] : TIER_PHRASES[pc.tier]
        const ok = answer === expected
        if (!ok) println(t('cancelled'), 'tf-dim')
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
        println(t('confirmTimedOut'), 'tf-warn')
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

    // help may be a function (our own built-ins use this so their text can
    // react to a sudoconfig lang switch) instead of a static string/array.
    function resolveHelp(def) {
      return typeof def.help === 'function' ? def.help() : def.help
    }

    function printWelcome() {
      print(`<span class="tf-gold tf-bold">${escHtml(opts.appName)}</span>  <span class="tf-dim">${t('escToClose')}</span>`)
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
      println(t('welcomeHint'), 'tf-dim')
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
      let shown = 0
      for (const [name, def] of commands) {
        if (shown >= 5) break
        if (!def.help || def.builtin || name !== def.name) continue
        printHelpRow(name, firstHelpRow(resolveHelp(def)))
        shown++
      }
      print(`  <span class="tf-hl">${'help'.padEnd(34)}</span><span class="tf-dim">${t('helpShowAll')}</span>`)
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
    }

    function printHelp() {
      print(`<span class="tf-gold tf-bold">${escHtml(opts.appName)}</span>  <span class="tf-dim">${t('escToClose')}</span>`)
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
      print(`  <span class="tf-hl">${'cmd1 | cmd2 | ...'.padEnd(34)}</span><span class="tf-dim">${t('helpChain')}</span>`)
      print(`  <span class="tf-hl">${'sudoseq | cmd1 | cmd2 | ...'.padEnd(34)}</span><span class="tf-dim">${t('helpSudoseq')}</span>`)
      print(`  <span class="tf-hl">${'loop <n> cmd1 | cmd2 | ...'.padEnd(34)}</span><span class="tf-dim">${t('helpLoop')}</span>`)
      print(`  <span class="tf-hl">${'wait <ms|Ns>'.padEnd(34)}</span><span class="tf-dim">${t('helpWait')}</span>`)
      print(`  <span class="tf-hl">${'!n'.padEnd(34)}</span><span class="tf-dim">${t('helpBang')}</span>`)
      for (const [name, def] of commands) {
        if (!def.help || name !== def.name) continue
        printHelpRow(name, resolveHelp(def))
      }
      print(`  <span class="tf-hl">${'config [key] [value]'.padEnd(34)}</span><span class="tf-dim">${t('helpConfig')}</span>`)
      print(`  <span class="tf-hl">${'matrix'.padEnd(34)}</span><span class="tf-dim">${t('helpMatrix')}</span>`)
      print(`  <span class="tf-hl">${'cowsay [message]'.padEnd(34)}</span><span class="tf-dim">${t('helpCowsay')}</span>`)
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
    }

    function printSudoHelp() {
      print(`<span class="tf-gold tf-bold">${t('hiddenCommandsTitle')}</span>  <span class="tf-dim">${t('notListedInHelp')}</span>`)
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
      for (const [name, def] of hiddenCommands) {
        if (name !== def.name) continue
        printHelpRow(name, resolveHelp(def) || '')
      }
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
    }

    // cowsay with no message drops a vague hint instead of "moo" — kept
    // deliberately non-specific, just enough of a nudge to go exploring.
    function pickCowsayHint() {
      const hints = t('cowsayHints')
      return hints[Math.floor(Math.random() * hints.length)]
    }

    function cmdCowsay(args) {
      const msg = args.join(' ') || pickCowsayHint()
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

    // ── calculator ───────────────────────────────────────────────────────
    // A hand-rolled tokenizer + recursive-descent parser/evaluator — no
    // eval()/Function() involved, so there's no expression-injection risk.
    class CalcError extends Error {
      constructor(key, ...args) { super(key); this.key = key; this.args = args }
    }
    const CALC_FUNCS = {
      sqrt: Math.sqrt, sin: Math.sin, cos: Math.cos, tan: Math.tan,
      asin: Math.asin, acos: Math.acos, atan: Math.atan,
      abs: Math.abs, floor: Math.floor, ceil: Math.ceil, round: Math.round,
      exp: Math.exp, ln: Math.log, log: Math.log10,
      max: Math.max, min: Math.min, pow: Math.pow,
    }
    const CALC_CONSTS = { pi: Math.PI, e: Math.E }
    let lastCalcResult = null

    function tokenizeCalc(str) {
      const tokens = []
      let i = 0
      while (i < str.length) {
        const c = str[i]
        if (/\s/.test(c)) { i++; continue }
        if (/[0-9.]/.test(c)) {
          let j = i
          while (j < str.length && /[0-9.]/.test(str[j])) j++
          if (str[j] === 'e' || str[j] === 'E') {
            j++
            if (str[j] === '+' || str[j] === '-') j++
            while (j < str.length && /[0-9]/.test(str[j])) j++
          }
          const numStr = str.slice(i, j)
          const num = parseFloat(numStr)
          if (!Number.isFinite(num)) throw new CalcError('calcInvalidNumber', numStr)
          tokens.push({ type: 'num', value: num })
          i = j
          continue
        }
        if (/[A-Za-z_]/.test(c)) {
          let j = i
          while (j < str.length && /[A-Za-z0-9_]/.test(str[j])) j++
          tokens.push({ type: 'ident', value: str.slice(i, j).toLowerCase() })
          i = j
          continue
        }
        if ('()+-*/%^!,'.includes(c)) {
          tokens.push({ type: 'op', value: c })
          i++
          continue
        }
        throw new CalcError('calcUnexpectedChar', c)
      }
      return tokens
    }

    function calcFactorial(n) {
      if (n < 0 || Math.floor(n) !== n) throw new CalcError('calcFactorialBad')
      let result = 1
      for (let i = 2; i <= n; i++) result *= i
      return result
    }

    // Precedence (loosest to tightest): + - | * / % | unary +/- | ^ (right-assoc) | postfix ! | primary
    function evalMathExpr(str) {
      const tokens = tokenizeCalc(str)
      let pos = 0
      const peek = () => tokens[pos]
      const next = () => tokens[pos++]
      const isOp = (t, v) => !!t && t.type === 'op' && t.value === v
      function expectOp(op) {
        const t = next()
        if (!isOp(t, op)) throw new CalcError('calcExpected', op)
      }

      function parseExpression() { return parseAddSub() }

      function parseAddSub() {
        let left = parseMulDiv()
        while (isOp(peek(), '+') || isOp(peek(), '-')) {
          const op = next().value
          const right = parseMulDiv()
          left = op === '+' ? left + right : left - right
        }
        return left
      }

      function parseMulDiv() {
        let left = parseUnary()
        while (isOp(peek(), '*') || isOp(peek(), '/') || isOp(peek(), '%')) {
          const op = next().value
          const right = parseUnary()
          if (op === '*') left *= right
          else if (op === '/') { if (right === 0) throw new CalcError('calcDivByZero'); left /= right }
          else left %= right
        }
        return left
      }

      function parseUnary() {
        if (isOp(peek(), '-') || isOp(peek(), '+')) {
          const op = next().value
          const val = parseUnary()
          return op === '-' ? -val : val
        }
        return parsePow()
      }

      function parsePow() {
        const base = parsePostfix()
        if (isOp(peek(), '^')) {
          next()
          const exp = parseUnary() // right-assoc + lets "2^-2" / "2^3^2" work as expected
          return Math.pow(base, exp)
        }
        return base
      }

      function parsePostfix() {
        let val = parsePrimary()
        while (isOp(peek(), '!')) { next(); val = calcFactorial(val) }
        return val
      }

      function parsePrimary() {
        const t = peek()
        if (!t) throw new CalcError('calcUnexpectedEnd')
        if (t.type === 'num') { next(); return t.value }
        if (isOp(t, '(')) {
          next()
          const val = parseExpression()
          expectOp(')')
          return val
        }
        if (t.type === 'ident') {
          next()
          const name = t.value
          if (isOp(peek(), '(')) {
            next()
            const args = []
            if (!isOp(peek(), ')')) {
              args.push(parseExpression())
              while (isOp(peek(), ',')) { next(); args.push(parseExpression()) }
            }
            expectOp(')')
            const fn = CALC_FUNCS[name]
            if (!fn) throw new CalcError('calcUnknownFn', name)
            return fn(...args)
          }
          if (name === 'ans') {
            if (lastCalcResult === null) throw new CalcError('calcNoAns')
            return lastCalcResult
          }
          if (name in CALC_CONSTS) return CALC_CONSTS[name]
          throw new CalcError('calcUnknownIdent', name)
        }
        throw new CalcError('calcUnexpectedToken', t.value)
      }

      const result = parseExpression()
      if (pos < tokens.length) throw new CalcError('calcUnexpectedToken', tokens[pos].value)
      return result
    }

    function formatCalcResult(n) {
      if (!Number.isFinite(n)) return String(n)
      return String(Number(n.toPrecision(12)))
    }
    function formatComplex({ re, im }) {
      return `${formatCalcResult(re)} ${im < 0 ? '-' : '+'} ${formatCalcResult(Math.abs(im))}i`
    }

    // ── unit conversion (calc <n> <unit> to <unit>) ─────────────────────
    const UNIT_GROUPS = {
      length: { m: 1, km: 1000, cm: 0.01, mm: 0.001, mi: 1609.344, yd: 0.9144, ft: 0.3048, in: 0.0254 },
      mass: { kg: 1, g: 0.001, mg: 0.000001, lb: 0.45359237, oz: 0.028349523125 },
      time: { s: 1, sec: 1, min: 60, h: 3600, hr: 3600, day: 86400 },
    }
    function convertTemperature(value, from, to) {
      const celsius = from === 'c' ? value : from === 'f' ? (value - 32) * 5 / 9 : value - 273.15
      if (to === 'c') return celsius
      if (to === 'f') return celsius * 9 / 5 + 32
      return celsius + 273.15
    }
    function convertUnits(value, fromUnit, toUnit) {
      const from = fromUnit.toLowerCase(), to = toUnit.toLowerCase()
      const isTemp = (u) => u === 'c' || u === 'f' || u === 'k'
      if (isTemp(from) && isTemp(to)) return convertTemperature(value, from, to)
      for (const group of Object.values(UNIT_GROUPS)) {
        if (from in group && to in group) return (value * group[from]) / group[to]
      }
      throw new CalcError('calcUnitMismatch', fromUnit, toUnit)
    }

    // ── algebra solver (calc solve) ─────────────────────────────────────
    // A separate, deliberately restricted grammar for plain polynomial
    // terms (e.g. "3x^2", "-5x", "7") — not the general expression
    // evaluator, since solving needs symbolic coefficients, not a number.
    function splitOnce(str, sep) {
      const idx = str.indexOf(sep)
      return idx === -1 ? [str, undefined] : [str.slice(0, idx), str.slice(idx + sep.length)]
    }

    function parsePolyTerms(side) {
      const terms = []
      let s = side.trim()
      while (s.length > 0) {
        let sign = 1
        const signMatch = s.match(/^([+-])\s*/)
        if (signMatch) {
          sign = signMatch[1] === '-' ? -1 : 1
          s = s.slice(signMatch[0].length)
        }
        const termMatch = s.match(/^(\d+(?:\.\d+)?)?\s*\*?\s*([A-Za-z])?\s*(?:\^\s*(\d+))?\s*/)
        if (!termMatch || (termMatch[1] === undefined && termMatch[2] === undefined) || termMatch[0].length === 0) {
          throw new CalcError('calcAlgebraParseError', s)
        }
        const varName = termMatch[2] ? termMatch[2].toLowerCase() : null
        const exp = termMatch[3] !== undefined ? parseInt(termMatch[3], 10) : (varName ? 1 : 0)
        const coeff = termMatch[1] !== undefined ? parseFloat(termMatch[1]) : 1
        terms.push({ sign, coeff, varName, exp })
        s = s.slice(termMatch[0].length)
        if (s.length > 0 && !/^[+-]/.test(s)) throw new CalcError('calcAlgebraParseError', s)
      }
      return terms
    }

    // Combines both sides (RHS negated) into a*v^2 + b*v + c = 0 form.
    function equationToPoly(eqStr) {
      const [lhsStr, rhsStr] = splitOnce(eqStr, '=')
      if (rhsStr === undefined) throw new CalcError('calcAlgebraNoEquals')
      const lhsTerms = parsePolyTerms(lhsStr)
      const rhsTerms = parsePolyTerms(rhsStr).map((term) => ({ ...term, sign: term.sign * -1 }))
      return lhsTerms.concat(rhsTerms)
    }

    function solveSingleEquation(eqStr) {
      const allTerms = equationToPoly(eqStr)
      const varNames = [...new Set(allTerms.filter((term) => term.varName).map((term) => term.varName))]
      if (varNames.length === 0) throw new CalcError('calcAlgebraNoVariable')
      if (varNames.length > 1) throw new CalcError('calcAlgebraTooManyVars')
      const v = varNames[0]
      let a = 0, b = 0, c = 0
      for (const term of allTerms) {
        const val = term.sign * term.coeff
        if (!term.varName) c += val
        else if (term.exp === 1) b += val
        else if (term.exp === 2) a += val
        else throw new CalcError('calcAlgebraDegreeUnsupported')
      }
      if (a === 0) {
        if (b === 0) return { type: c === 0 ? 'identity' : 'none' }
        return { type: 'linear', varName: v, value: -c / b }
      }
      const disc = b * b - 4 * a * c
      if (disc > 0) {
        const sq = Math.sqrt(disc)
        return { type: 'real', varName: v, roots: [(-b + sq) / (2 * a), (-b - sq) / (2 * a)] }
      }
      if (disc === 0) return { type: 'real', varName: v, roots: [-b / (2 * a)] }
      const sq = Math.sqrt(-disc)
      const re = -b / (2 * a), im = sq / (2 * a)
      return { type: 'complex', varName: v, roots: [{ re, im }, { re, im: -im }] }
    }

    function equationToLinearCoeffs(eqStr) {
      const allTerms = equationToPoly(eqStr)
      const coeffs = {}
      let constant = 0
      for (const term of allTerms) {
        if (term.exp > 1) throw new CalcError('calcAlgebraDegreeUnsupported')
        const val = term.sign * term.coeff
        if (!term.varName) constant += val
        else coeffs[term.varName] = (coeffs[term.varName] || 0) + val
      }
      return { coeffs, constant }
    }

    function solveSystem(eq1Str, eq2Str) {
      const e1 = equationToLinearCoeffs(eq1Str)
      const e2 = equationToLinearCoeffs(eq2Str)
      const varNames = [...new Set([...Object.keys(e1.coeffs), ...Object.keys(e2.coeffs)])]
      if (varNames.length !== 2) throw new CalcError('calcAlgebraSystemVars')
      const [v1, v2] = varNames
      const a1 = e1.coeffs[v1] || 0, b1 = e1.coeffs[v2] || 0, k1 = -e1.constant
      const a2 = e2.coeffs[v1] || 0, b2 = e2.coeffs[v2] || 0, k2 = -e2.constant
      const det = a1 * b2 - a2 * b1
      if (det === 0) throw new CalcError('calcAlgebraNoUniqueSolution')
      const val1 = (k1 * b2 - k2 * b1) / det
      const val2 = (a1 * k2 - a2 * k1) / det
      return { type: 'system', vars: [[v1, val1], [v2, val2]] }
    }

    function printSolveResult(result) {
      if (result.type === 'identity') { println(t('calcAlgebraIdentity'), 'tf-ok'); return }
      if (result.type === 'none') { println(t('calcAlgebraNoSolution'), 'tf-ok'); return }
      if (result.type === 'linear') { println(`${result.varName} = ${formatCalcResult(result.value)}`, 'tf-ok'); return }
      if (result.type === 'real') {
        const text = result.roots.length === 1
          ? formatCalcResult(result.roots[0])
          : `${formatCalcResult(result.roots[0])} ${t('calcOr')} ${formatCalcResult(result.roots[1])}`
        println(`${result.varName} = ${text}`, 'tf-ok')
        return
      }
      if (result.type === 'complex') {
        const [r1, r2] = result.roots
        println(`${result.varName} = ${formatComplex(r1)} ${t('calcOr')} ${formatComplex(r2)}`, 'tf-ok')
        return
      }
      if (result.type === 'system') {
        println(result.vars.map(([v, val]) => `${v} = ${formatCalcResult(val)}`).join(', '), 'tf-ok')
      }
    }

    function cmdCalcSolve(rest) {
      const equations = rest.split(',').map((s) => s.trim()).filter(Boolean)
      if (equations.length === 1) {
        printSolveResult(solveSingleEquation(equations[0]))
      } else if (equations.length === 2) {
        printSolveResult(solveSystem(equations[0], equations[1]))
      } else {
        throw new CalcError('calcAlgebraTooManyEquations')
      }
    }

    function printCalcHelp() {
      println(t('calcHelpTitle'), 'tf-gold')
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
      println(t('calcHelpArithmetic'), 'tf-hl')
      println(t('calcHelpArithmeticEx'), 'tf-dim')
      println(t('calcHelpFunctions'), 'tf-hl')
      println(t('calcHelpFunctionsEx'), 'tf-dim')
      println(t('calcHelpConstants'), 'tf-hl')
      println(t('calcHelpConstantsEx'), 'tf-dim')
      println(t('calcHelpAlgebra'), 'tf-hl')
      println(t('calcHelpAlgebraEx'), 'tf-dim')
      println(t('calcHelpSystem'), 'tf-hl')
      println(t('calcHelpSystemEx'), 'tf-dim')
      println(t('calcHelpUnits'), 'tf-hl')
      println(t('calcHelpUnitsEx'), 'tf-dim')
      print(`<span class="tf-dim">──────────────────────────────────────────────────────</span>`)
      println(t('calcHelpFooter'), 'tf-dim')
    }

    function cmdCalc(args) {
      const expr = args.join(' ')
      if (!expr) { printCalcHelp(); return }
      try {
        if (/^solve\s+/i.test(expr)) {
          cmdCalcSolve(expr.replace(/^solve\s+/i, ''))
          return
        }
        const convertMatch = expr.match(/^(-?\d+(?:\.\d+)?)\s*([A-Za-z]+)\s+to\s+([A-Za-z]+)$/i)
        if (convertMatch) {
          const [, numStr, fromUnit, toUnit] = convertMatch
          const result = convertUnits(parseFloat(numStr), fromUnit, toUnit)
          lastCalcResult = result
          println(`= ${formatCalcResult(result)} ${toUnit}`, 'tf-ok')
          return
        }
        const result = evalMathExpr(expr)
        lastCalcResult = result
        println(`= ${formatCalcResult(result)}`, 'tf-ok')
      } catch (e) {
        const msg = e instanceof CalcError ? t(e.key, ...e.args) : e.message
        println(t('errorPrefix') + msg, 'tf-err')
      }
    }

    // ── history, aliases, find, export ──────────────────────────────────
    function cmdHistory() {
      if (history.length === 0) { println(t('historyEmpty'), 'tf-dim'); return }
      ;[...history].reverse().forEach((cmd, i) => {
        print(`  <span class="tf-dim">${String(i + 1).padStart(4)}</span>  ${escHtml(cmd)}`)
      })
    }

    // User-defined aliases — separate from registerCommand's registry since
    // these are end-user shortcuts (persisted per browser), not host-app
    // commands. `dispatch()` expands them before real-command lookup.
    function loadAliases() {
      try {
        const raw = JSON.parse(localStorage.getItem(opts.storageKey + '_aliases') || '{}')
        return raw && typeof raw === 'object' ? raw : {}
      } catch (e) { return {} }
    }
    function saveAliases() {
      try { localStorage.setItem(opts.storageKey + '_aliases', JSON.stringify(USER_ALIASES)) } catch (e) {}
    }
    const USER_ALIASES = loadAliases()

    function cmdAlias(args) {
      const joined = args.join(' ')
      if (!joined) {
        const names = Object.keys(USER_ALIASES)
        if (names.length === 0) { println(t('aliasNone'), 'tf-dim'); return }
        names.forEach((name) => print(`  <span class="tf-hl">${name.padEnd(20)}</span>${escHtml(USER_ALIASES[name])}`))
        return
      }
      const eqIdx = joined.indexOf('=')
      if (eqIdx === -1) {
        const name = joined.trim().toLowerCase()
        if (USER_ALIASES[name] === undefined) { println(t('aliasNotFound', name), 'tf-err'); return }
        println(`${name}=${USER_ALIASES[name]}`, 'tf-dim')
        return
      }
      const name = joined.slice(0, eqIdx).trim().toLowerCase()
      const value = joined.slice(eqIdx + 1).trim()
      if (!name || !value) { println(t('aliasUsage'), 'tf-warn'); return }
      if (commands.has(name) || hiddenCommands.has(name)) { println(t('aliasShadowsCommand', name), 'tf-err'); return }
      USER_ALIASES[name] = value
      saveAliases()
      println(t('aliasSet', name, value), 'tf-ok')
    }
    function cmdUnalias(args) {
      const name = (args[0] || '').toLowerCase()
      if (!name) { println(t('unaliasUsage'), 'tf-warn'); return }
      if (USER_ALIASES[name] === undefined) { println(t('aliasNotFound', name), 'tf-err'); return }
      delete USER_ALIASES[name]
      saveAliases()
      println(t('unaliasDone', name), 'tf-ok')
    }

    function cmdFind(args) {
      const term = args.join(' ').toLowerCase()
      const rows = Array.from(output.children)
      if (!term) {
        rows.forEach((row) => { row.style.display = '' })
        println(t('findCleared'), 'tf-dim')
        return
      }
      let matches = 0
      rows.forEach((row) => {
        const hit = row.textContent.toLowerCase().includes(term)
        row.style.display = hit ? '' : 'none'
        if (hit) matches++
      })
      println(t('findMatches', matches), 'tf-dim')
    }

    function cmdExport() {
      const text = Array.from(output.children).map((el) => el.textContent).join('\n')
      const blob = new Blob([text], { type: 'text/plain' })
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `${opts.rootId}-log.txt`
      document.body.appendChild(a)
      a.click()
      a.remove()
      URL.revokeObjectURL(url)
      println(t('exportDone'), 'tf-ok')
    }

    // ── purge ────────────────────────────────────────────────────────────
    function purgeConfig() {
      try { localStorage.removeItem(opts.storageKey) } catch (e) {}
      CONFIG = { ...CONFIG_DEFAULTS }
      GLITTER = CONFIG_SCHEMA.glitter.includes(CONFIG.glitter) ? CONFIG.glitter : 'focus'
      if (CONFIG_SCHEMA.color.includes(CONFIG.color)) applyTheme(CONFIG.color)
      else if (!applyCustomTheme(CONFIG.color)) applyTheme('green')
    }
    function purgeHistory() {
      history.length = 0
      histIdx = -1
      try { localStorage.removeItem(opts.storageKey + '_history') } catch (e) {}
    }
    function purgeAliases() {
      Object.keys(USER_ALIASES).forEach((k) => delete USER_ALIASES[k])
      try { localStorage.removeItem(opts.storageKey + '_aliases') } catch (e) {}
    }
    const PURGE_TARGETS = { config: purgeConfig, history: purgeHistory, aliases: purgeAliases }
    const PURGE_TARGET_LIST = Object.keys(PURGE_TARGETS).concat('all').join(', ')

    async function cmdPurge(args) {
      const target = (args[0] || '').toLowerCase()
      if (!target) { println(t('purgeUsage', PURGE_TARGET_LIST), 'tf-warn'); return }
      if (target === 'all') {
        const ok = await confirm(3, t('purgeConfirmAll'))
        if (!ok) return // confirm() already prints "Cancelled." on rejection
        startFire()
        Object.values(PURGE_TARGETS).forEach((fn) => fn())
        await stopFire()
        println(t('purgeAllDone'), 'tf-ok')
        return
      }
      const fn = PURGE_TARGETS[target]
      if (!fn) { println(t('purgeUnknownTarget', target, PURGE_TARGET_LIST), 'tf-err'); return }
      const ok = await confirm(2, t('purgeConfirmOne', target))
      if (!ok) return
      startFire()
      fn()
      await stopFire()
      println(t('purgeDone', target), 'tf-ok')
    }

    // ── built-in commands ───────────────────────────────────────────────
    // Routed through the same registry host apps use via registerCommand.
    // config/cowsay/matrix are left without a `.help` entry because printHelp
    // already documents them via dedicated static lines.
    registerCommand('help', { builtin: true, aliases: ['?'], run: () => printHelp() })
    registerCommand('sudohelp', { builtin: true, run: () => printSudoHelp() })
    registerCommand('clear', { builtin: true, aliases: ['cls'], help: () => ['clear / cls', t('helpClearDesc')], run: () => clearOutput() })
    registerCommand('exit', { builtin: true, aliases: ['q'], help: () => ['exit / q', t('helpExitDesc')], run: () => closePanel() })
    // Quiet typo-catchers for people who forget the command is "exit" — not
    // real aliases (they don't close the panel), just a nudge in the right
    // direction. No `.help`, so they stay out of both help and sudohelp.
    // Escalates the more times it happens; closePanel() resets the counter.
    let notExitCount = 0
    ;['close', 'quit', 'leave', 'bye', 'logout', 'shutdown', 'end'].forEach((name) => {
      registerCommand(name, {
        builtin: true,
        run: async () => {
          notExitCount++
          if (notExitCount === 3) {
            println(t('seriouslyItsExit'))
          } else if (notExitCount >= 4) {
            println(t('fineIllDoItForYou'))
            await new Promise(resolve => setTimeout(resolve, 2000))
            printCmdEcho('exit')
            await runSingle('exit')
          } else {
            println(t('itsExit'))
          }
        },
      })
    })
    registerCommand('config', { builtin: true, run: (args) => cmdConfig(args) })
    // The real command is "config" — "settings" just gets teased, not run.
    registerCommand('settings', { builtin: true, run: () => println(t('settingsTease')) })
    registerCommand('sudoconfig', { hidden: true, help: () => t('helpSudoconfig'), run: (args) => cmdSudoConfig(args) })
    registerCommand('cowsay', { builtin: true, run: (args) => cmdCowsay(args) })
    registerCommand('calc', { help: () => ['calc <expr|solve ...>', t('helpCalc')], run: (args) => cmdCalc(args) })
    registerCommand('history', { help: () => ['history', t('helpHistory')], run: () => cmdHistory() })
    registerCommand('alias', { help: () => ['alias [name[=cmd]]', t('helpAlias')], run: (args) => cmdAlias(args) })
    registerCommand('unalias', { help: () => ['unalias <name>', t('helpUnalias')], run: (args) => cmdUnalias(args) })
    registerCommand('find', { help: () => ['find [term]', t('helpFind')], run: (args) => cmdFind(args) })
    registerCommand('export', { help: () => ['export', t('helpExport')], run: () => cmdExport() })
    registerCommand('purge', { help: () => ['purge <config|history|aliases|all>', t('helpPurge')], run: (args) => cmdPurge(args) })
    registerCommand('uji', { hidden: true, help: () => t('helpUji'), run: () => cmdUji() })
    registerCommand('sudo', { hidden: true, help: () => t('helpSudo'), run: () => println(t('sudoOutput')) })
    registerCommand('wait', {
      builtin: true,
      run: async (args) => {
        const ms = parseDuration(args[0])
        if (!Number.isFinite(ms) || ms < 0) { println(t('waitUsage'), 'tf-err'); return }
        println(t('waiting', ms), 'tf-dim')
        await new Promise(resolve => setTimeout(resolve, ms))
      },
    })
    registerCommand('matrix', {
      builtin: true,
      run: () => {
        startMatrixEgg(t('pressAnyKey'))
        input.blur()
        setTimeout(() => {
          const stop = (e) => { e.preventDefault(); stopMatrixEgg(); document.removeEventListener('keydown', stop); input.focus() }
          document.addEventListener('keydown', stop)
        }, 50)
      },
    })
    registerCommand('startprocess', { hidden: true, help: () => t('helpStartprocess'), run: () => startLoading() })
    registerCommand('endprocess', { hidden: true, help: () => t('helpEndprocess'), run: () => { stopLoading(); input.focus() } })
    registerCommand('startpurge', { hidden: true, help: () => t('helpStartpurge'), run: () => startFire() })
    registerCommand('endpurge', { hidden: true, help: () => t('helpEndpurge'), run: async () => { await stopFire(); input.focus() } })
    registerCommand('startglitter', { hidden: true, help: () => t('helpStartglitter'), run: () => startGlitterPreview() })
    registerCommand('endglitter', { hidden: true, help: () => t('helpEndglitter'), run: () => stopGlitterPreview() })

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
      println(t('unknownCommand', cmd), 'tf-err')
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
        startLoading(t('runningSequenceLabel'), seqMatrixOpts)
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
          println(t('errorPrefix') + (e && e.message ? e.message : e), 'tf-err')
        }
      }
      finishQueue()
    }

    async function dispatch(raw) {
      let trimmed = raw.trim()
      if (!trimmed) return

      if (pendingConfirm) {
        recordHistory(trimmed)
        histIdx = -1
        printCmdEcho(trimmed)
        resolvePendingConfirm(trimmed)
        return
      }

      // !n re-runs the nth command shown by `history` (most recent = highest
      // number) — expand it to the real command text before anything else
      // sees it, so it can itself be a chain/loop/alias and gets recorded
      // as the resolved text, not the literal "!n" (matches shell behavior).
      const bangMatch = trimmed.match(/^!(\d+)$/)
      if (bangMatch) {
        const displayList = [...history].reverse()
        const resolved = displayList[parseInt(bangMatch[1], 10) - 1]
        if (resolved === undefined) {
          recordHistory(trimmed)
          histIdx = -1
          printCmdEcho(trimmed)
          println(t('historyNoSuchEntry', bangMatch[1]), 'tf-err')
          return
        }
        trimmed = resolved
      }

      // User-defined aliases (see `alias`) expand the leading word only,
      // bash-style — real registered commands always take precedence so an
      // alias can never shadow a built-in or host command.
      const firstSpace = trimmed.indexOf(' ')
      const firstWord = (firstSpace === -1 ? trimmed : trimmed.slice(0, firstSpace)).toLowerCase()
      if (USER_ALIASES[firstWord] !== undefined && !commands.has(firstWord) && !hiddenCommands.has(firstWord)) {
        const restArgs = firstSpace === -1 ? '' : trimmed.slice(firstSpace)
        trimmed = USER_ALIASES[firstWord] + restArgs
      }

      recordHistory(trimmed)
      histIdx = -1

      if (/^sudoseq(\s|\||$)/i.test(trimmed)) {
        printCmdEcho(trimmed)
        const rest = trimmed.slice('sudoseq'.length).trim()
        const segments = rest.split('|').map(s => s.trim()).filter(Boolean)
        if (segments.length === 0) { println(t('sudoseqUsage'), 'tf-warn'); return }
        println(t('sudoseqWarning'), 'tf-warn')
        segments.forEach((s, i) => print(`  <span class="tf-dim">${i + 1}.</span> ${escHtml(s)}`))
        print(t('sudoseqConfirm', SUDOSEQ_PHRASE, segments.length))
        pendingConfirm = {
          kind: 'tier',
          tier: 4,
          step: 1,
          resolve: (ok) => {
            if (!ok) { println(t('cancelled'), 'tf-dim'); return }
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
        if (count <= 0) { println(t('loopCountInvalid'), 'tf-err'); return }
        if (segments.length === 0) { println(t('loopUsage'), 'tf-warn'); return }
        const maxLoop = resolveMaxLoop()
        if (count > maxLoop) {
          println(t('loopExceeds', count, maxLoop, CONFIG.maxLoop), 'tf-err')
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
    const HISTORY_LIMIT = 200
    function loadHistory() {
      try {
        const raw = JSON.parse(localStorage.getItem(opts.storageKey + '_history') || '[]')
        return Array.isArray(raw) ? raw.slice(0, HISTORY_LIMIT) : []
      } catch (e) { return [] }
    }
    function saveHistory() {
      try { localStorage.setItem(opts.storageKey + '_history', JSON.stringify(history)) } catch (e) {}
    }
    // history is stored most-recent-first; recordHistory keeps it capped + persisted.
    function recordHistory(cmd) {
      history.unshift(cmd)
      if (history.length > HISTORY_LIMIT) history.length = HISTORY_LIMIT
      saveHistory()
    }
    let history = loadHistory(), histIdx = -1

    function openPanel(triggerOpts) {
      box.classList.remove('tf-boot-anim')
      void box.offsetWidth // reflow so the boot animation can replay on the next Konami open
      panel.classList.add('open')
      if (triggerOpts && triggerOpts.boot) box.classList.add('tf-boot-anim')
      setTimeout(() => input.focus(), 50)
      if (output.children.length === 0) printWelcome()
    }
    function closePanel() {
      panel.classList.remove('open')
      notExitCount = 0
      // Deliberately leave pendingConfirm/sequence state alone — closing the
      // panel shouldn't abort an in-progress sequence.
    }

    input.addEventListener('keydown', e => {
      if (e.key === 'Enter') {
        const val = input.value
        input.value = ''
        dispatch(val)
      } else if (e.key === 'Tab') {
        e.preventDefault()
        const val = input.value
        if (val.includes(' ')) return // only completes the leading command name, not arguments
        const prefix = val.toLowerCase()
        if (!prefix) { println(t('tabTypeSomethingFirst'), 'tf-dim'); return }
        // Hidden commands complete too — you still have to know/guess the
        // name to get a match, so this is a discovery path, not a giveaway.
        const allNames = new Set([...commands.keys(), ...hiddenCommands.keys()])
        const matches = [...allNames].filter(name => name.startsWith(prefix)).sort()
        if (matches.length === 1) {
          input.value = matches[0] + ' '
        } else if (matches.length > 1) {
          printCmdEcho(val, '$ (tab)')
          print(matches.map(m => `<span class="tf-hl">${escHtml(m)}</span>`).join('  '))
        }
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
              openPanel({ boot: true })
            }
          }
        } else {
          konamiIdx = e.key === KONAMI[0] ? 1 : 0
          if (pendingKonamiOpen && e.key === 'Escape') {
            const resolve = pendingKonamiOpen
            pendingKonamiOpen = null
            println(t('cancelled'), 'tf-dim')
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
