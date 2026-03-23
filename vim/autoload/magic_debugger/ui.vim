" =============================================================================
" File: ui.vim
" Description: Magic Debugger UI Module - Window and buffer management
"
" This module provides UI functionality for Magic Debugger including:
"   - Debug windows layout (stack, variables, output, watch)
"   - Buffer creation and management
"   - Syntax highlighting for debug views
"   - Window navigation
"
" Phase 5: Frontend - Vim Plugin UI
" =============================================================================

" Prevent duplicate loading
if exists('g:loaded_magic_debugger_ui')
    finish
endif
let g:loaded_magic_debugger_ui = 1

" =============================================================================
" Internal State
" =============================================================================

" Window IDs
let s:stack_win_id = -1
let s:vars_win_id = -1
let s:output_win_id = -1
let s:watch_win_id = -1

" Buffer numbers
let s:stack_bufnr = -1
let s:vars_bufnr = -1
let s:output_bufnr = -1
let s:watch_bufnr = -1

" UI state
let s:ui_open = 0
let s:last_layout = ''

" =============================================================================
" Public Functions
" =============================================================================

" Open debug UI
function! magic_debugger#ui#Open()
    if s:ui_open
        return
    endif

    " Save current layout
    let s:last_layout = winlayout()

    " Create debug windows based on configured layout
    if g:magic_debugger_layout == 'vertical'
        call s:CreateVerticalLayout()
    else
        call s:CreateHorizontalLayout()
    endif

    let s:ui_open = 1
endfunction

" Close debug UI
function! magic_debugger#ui#Close()
    if !s:ui_open
        return
    endif

    " Close debug windows
    for win_id in [s:stack_win_id, s:vars_win_id, s:output_win_id, s:watch_win_id]
        if win_id >= 0 && win_id != win_getid()
            let winnr = win_id2win(win_id)
            if winnr > 0
                execute winnr . 'close'
            endif
        endif
    endfor

    let s:ui_open = 0
endfunction

" Toggle debug UI
function! magic_debugger#ui#Toggle()
    if s:ui_open
        call magic_debugger#ui#Close()
    else
        call magic_debugger#ui#Open()
    endif
endfunction

" Focus source window
function! magic_debugger#ui#FocusSource()
    " Find source window (first window that's not a debug window)
    for winnr in range(1, winnr('$'))
        let bufname = bufname(winbufnr(winnr))
        if bufname !~# 'MagicDebug'
            execute winnr . 'wincmd w'
            return
        endif
    endfor
endfunction

" Focus stack window
function! magic_debugger#ui#FocusStack()
    call s:FocusWindow(s:stack_win_id)
endfunction

" Focus variables window
function! magic_debugger#ui#FocusVars()
    call s:FocusWindow(s:vars_win_id)
endfunction

" Focus output window
function! magic_debugger#ui#FocusOutput()
    call s:FocusWindow(s:output_win_id)
endfunction

" Update stack window
function! magic_debugger#ui#UpdateStack(body)
    if s:stack_bufnr == -1
        return
    endif

    let lines = []

    if has_key(a:body, 'stackFrames')
        for frame in a:body.stackFrames
            let name = get(frame, 'name', '??')
            let source = get(frame, 'source', {})
            let path = get(source, 'path', '??')
            let line = get(frame, 'line', 0)

            " Truncate path for display
            let filename = fnamemodify(path, ':t')
            let display = printf("#%d %s at %s:%d", frame.id, name, filename, line)
            call add(lines, display)
        endfor
    endif

    call s:SetBufferContent(s:stack_bufnr, lines)
endfunction

" Update scopes/variables
function! magic_debugger#ui#UpdateScopes(body)
    " Scopes are containers for variables
    " Usually we just update variables directly
endfunction

" Update variables window
function! magic_debugger#ui#UpdateVariables(body)
    if s:vars_bufnr == -1
        return
    endif

    let lines = []

    if has_key(a:body, 'variables')
        for var in a:body.variables
            let name = get(var, 'name', '??')
            let value = get(var, 'value', '??')
            let type = get(var, 'type', '')

            if type != ''
                let display = printf("%s: %s = %s", name, type, value)
            else
                let display = printf("%s = %s", name, value)
            endif

            call add(lines, display)
        endfor
    endif

    call s:SetBufferContent(s:vars_bufnr, lines)
endfunction

" Update threads window
function! magic_debugger#ui#UpdateThreads(body)
    " Could update a threads buffer if we have one
endfunction

" Show evaluation result
function! magic_debugger#ui#ShowEvalResult(body)
    if has_key(a:body, 'result')
        echo "Result: " . a:body.result
        call magic_debugger#ui#AddOutput('result', a:body.result)
    endif
endfunction

" Add output line
function! magic_debugger#ui#AddOutput(type, text)
    if s:output_bufnr == -1
        return
    endif

    let prefix = ''
    if a:type == 'command'
        let prefix = '> '
    elseif a:type == 'error'
        let prefix = 'E: '
    elseif a:type == 'result'
        let prefix = '= '
    endif

    let line = prefix . a:text

    " Append to buffer
    let save_cursor = getcurpos()
    call s:AppendBufferContent(s:output_bufnr, [line])
    call setpos('.', save_cursor)

    " Auto-scroll output window
    if win_id2win(s:output_win_id) > 0
        let winnr = win_id2win(s:output_win_id)
        execute winnr . 'wincmd b'
    endif
endfunction

" Update watch window
function! magic_debugger#ui#UpdateWatchWindow()
    if s:watch_bufnr == -1
        return
    endif

    " Update watch expressions
    let lines = []
    " This would be populated from stored watch expressions
    call s:SetBufferContent(s:watch_bufnr, lines)
endfunction

" Handle stopped event
function! magic_debugger#ui#OnStopped(body)
    let reason = get(a:body, 'reason', 'unknown')

    " Update position if available
    if has_key(a:body, 'threadId')
        " Request stack trace for current thread
        call s:RequestStackTrace(a:body.threadId)
    endif

    " Show stop reason
    call magic_debugger#ui#AddOutput('output', 'Stopped: ' . reason)
endfunction

" Handle continued event
function! magic_debugger#ui#OnContinued()
    call magic_debugger#ui#AddOutput('output', 'Running...')
endfunction

" =============================================================================
" Internal Functions - Layout
" =============================================================================

" Create horizontal layout (stack and vars side by side at bottom)
function! s:CreateHorizontalLayout()
    " Create output window at bottom
    execute 'botright ' . g:magic_debugger_stack_height . 'split __MagicDebugOutput__'
    let s:output_win_id = win_getid()
    let s:output_bufnr = bufnr('%')
    call s:SetupOutputBuffer()

    " Create stack window
    execute 'botright ' . g:magic_debugger_stack_height . 'split __MagicDebugStack__'
    let s:stack_win_id = win_getid()
    let s:stack_bufnr = bufnr('%')
    call s:SetupStackBuffer()

    " Create variables window on the right
    execute 'botright vertical ' . g:magic_debugger_vars_width . 'split __MagicDebugVars__'
    let s:vars_win_id = win_getid()
    let s:vars_bufnr = bufnr('%')
    call s:SetupVarsBuffer()

    " Return to source window
    call magic_debugger#ui#FocusSource()
endfunction

" Create vertical layout (stack and vars on the right side)
function! s:CreateVerticalLayout()
    " Create variables window on the right
    execute 'topleft vertical ' . g:magic_debugger_vars_width . 'split __MagicDebugVars__'
    let s:vars_win_id = win_getid()
    let s:vars_bufnr = bufnr('%')
    call s:SetupVarsBuffer()

    " Create stack window below variables
    execute 'belowright ' . g:magic_debugger_stack_height . 'split __MagicDebugStack__'
    let s:stack_win_id = win_getid()
    let s:stack_bufnr = bufnr('%')
    call s:SetupStackBuffer()

    " Create output window below stack
    execute 'belowright ' . g:magic_debugger_stack_height . 'split __MagicDebugOutput__'
    let s:output_win_id = win_getid()
    let s:output_bufnr = bufnr('%')
    call s:SetupOutputBuffer()

    " Return to source window
    wincmd l
    wincmd l
endfunction

" =============================================================================
" Internal Functions - Buffer Setup
" =============================================================================

" Setup stack buffer
function! s:SetupStackBuffer()
    setlocal buftype=nofile
    setlocal bufhidden=hide
    setlocal noswapfile
    setlocal nomodifiable
    setlocal nowrap
    setlocal cursorline

    " Set syntax highlighting
    setlocal syntax=magicdebugstack

    " Create syntax rules
    syntax match MagicDebugFrameNum /^#\d\+/
    syntax match MagicDebugFunction /\v\w+\ze\(/
    syntax match MagicDebugLocation /at .\+$/

    highlight default link MagicDebugFrameNum Number
    highlight default link MagicDebugFunction Function
    highlight default link MagicDebugLocation Comment

    " Mappings
    nnoremap <buffer> <Enter> :call <SID>StackSelectLine()<CR>
    nnoremap <buffer> q :call magic_debugger#ui#FocusSource()<CR>
endfunction

" Setup variables buffer
function! s:SetupVarsBuffer()
    setlocal buftype=nofile
    setlocal bufhidden=hide
    setlocal noswapfile
    setlocal nomodifiable
    setlocal nowrap
    setlocal cursorline

    " Set syntax highlighting
    setlocal syntax=magicdebugvars

    " Create syntax rules
    syntax match MagicDebugVarName /^\w\+/
    syntax match MagicDebugEquals /=/
    syntax match MagicDebugValue /=\s*\zs.*$/

    highlight default link MagicDebugVarName Identifier
    highlight default link MagicDebugEquals Operator
    highlight default link MagicDebugValue String

    " Mappings
    nnoremap <buffer> <Enter> :call <SID>VarsExpand()<CR>
    nnoremap <buffer> q :call magic_debugger#ui#FocusSource()<CR>
endfunction

" Setup output buffer
function! s:SetupOutputBuffer()
    setlocal buftype=nofile
    setlocal bufhidden=hide
    setlocal noswapfile
    setlocal nomodifiable
    setlocal wrap

    " Set syntax highlighting
    setlocal syntax=magicdebugoutput

    " Create syntax rules
    syntax match MagicDebugPrompt /^>/
    syntax match MagicDebugError /^E:/
    syntax match MagicDebugResult /^=/

    highlight default link MagicDebugPrompt Statement
    highlight default link MagicDebugError ErrorMsg
    highlight default link MagicDebugResult String

    " Mappings
    nnoremap <buffer> q :call magic_debugger#ui#FocusSource()<CR>
endfunction

" Setup watch buffer
function! s:SetupWatchBuffer()
    setlocal buftype=nofile
    setlocal bufhidden=hide
    setlocal noswapfile
    setlocal nomodifiable
    setlocal nowrap
    setlocal cursorline
endfunction

" =============================================================================
" Internal Functions - Utilities
" =============================================================================

" Focus a window by ID
function! s:FocusWindow(win_id)
    if a:win_id < 0
        return
    endif

    let winnr = win_id2win(a:win_id)
    if winnr > 0
        execute winnr . 'wincmd w'
    endif
endfunction

" Set buffer content
function! s:SetBufferContent(bufnr, lines)
    let winnr = bufwinnr(a:bufnr)
    if winnr == -1
        return
    endif

    let save_cursor = getcurpos()

    " Make modifiable
    call setbufvar(a:bufnr, '&modifiable', 1)
    call setbufvar(a:bufnr, '&readonly', 0)

    " Clear and set content
    execute winnr . 'wincmd w'
    execute '%delete _'
    call setline(1, a:lines)

    " Make non-modifiable
    setlocal nomodifiable

    " Restore cursor
    call setpos('.', save_cursor)
endfunction

" Append to buffer content
function! s:AppendBufferContent(bufnr, lines)
    let winnr = bufwinnr(a:bufnr)
    if winnr == -1
        return
    endif

    " Make modifiable
    call setbufvar(a:bufnr, '&modifiable', 1)
    call setbufvar(a:bufnr, '&readonly', 0)

    " Append lines
    let last_line = getbufline(a:bufnr, '$')
    if len(last_line) == 1 && last_line[0] == ''
        call setbufline(a:bufnr, 1, a:lines)
    else
        call appendbufline(a:bufnr, '$', a:lines)
    endif

    " Make non-modifiable
    call setbufvar(a:bufnr, '&modifiable', 0)
    call setbufvar(a:bufnr, '&readonly', 1)
endfunction

" Request stack trace for thread
function! s:RequestStackTrace(thread_id)
    " This would send a stackTrace request via the main module
    " For now, we'll use a simpler approach
endfunction

" Handle stack line selection
function! s:StackSelectLine()
    let line = getline('.')
    let frame_match = matchlist(line, '^#\(\d\+\)')

    if len(frame_match) > 1
        let frame_id = str2nr(frame_match[1])
        " Would request to select this frame
        echo "Selected frame #" . frame_id
    endif
endfunction

" Handle variable expand/collapse
function! s:VarsExpand()
    " Would expand/collapse variable tree
endfunction

" =============================================================================
" Window Status Line
" =============================================================================

" Set custom status line for debug windows
function! s:SetStatusLine(bufnr, name)
    let winnr = bufwinnr(a:bufnr)
    if winnr == -1
        return
    endif

    execute winnr . 'wincmd w'
    setlocal statusline=%#ErrorMsg#\ %{a:name}\ %*%=%-10.(%l:%c%)\ %P
endfunction
