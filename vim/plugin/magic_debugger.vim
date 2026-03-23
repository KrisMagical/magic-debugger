" =============================================================================
" File: magic_debugger.vim
" Description: Magic Debugger autoload module - Core functionality
"
" This module provides the core functionality for the Magic Debugger Vim
" plugin, including session management, command execution, and state tracking.
"
" Phase 5: Frontend - Vim Plugin Core
" =============================================================================

" Prevent duplicate loading
if exists('g:loaded_magic_debugger_autoload')
    finish
endif
let g:loaded_magic_debugger_autoload = 1

" =============================================================================
" Internal State
" =============================================================================

" Session state dictionary
let s:session = {
    \ 'active': 0,
    \ 'adapter': '',
    \ 'program': '',
    \ 'pid': 0,
    \ 'state': 'inactive',
    \ 'current_file': '',
    \ 'current_line': 0,
    \ 'current_thread': 0,
    \ 'current_frame': 0,
    \ 'breakpoints': {},
    \ 'watch_expressions': [],
    \ 'job_id': -1,
    \ 'channel_id': -1,
\ }

" Sign IDs
let s:sign_id_base = 5000
let s:sign_current_base = 6000

" =============================================================================
" Initialization
" =============================================================================

" Initialize plugin state
function! magic_debugger#Init()
    " Reset session state
    let s:session.active = 0
    let s:session.adapter = ''
    let s:session.program = ''
    let s:session.state = 'inactive'
    let s:session.current_file = ''
    let s:session.current_line = 0
    let s:session.breakpoints = {}
    let s:session.watch_expressions = []
endfunction

" =============================================================================
" Session Management
" =============================================================================

" Start debugging session
" @param program Path to program to debug (optional, uses current file if not provided)
function! magic_debugger#Start(...)
    " Check if already debugging
    if s:session.active
        call s:EchoWarning('Debug session already active. Use :MagicDebugStop first.')
        return
    endif

    " Get program to debug
    let program = ''
    if a:0 > 0 && a:1 != ''
        let program = a:1
    else
        " Use current file or prompt
        if &modifiable
            let program = expand('%:p')
            if program == ''
                let program = input('Program to debug: ', '', 'file')
            endif
        else
            let program = input('Program to debug: ', '', 'file')
        endif
    endif

    if program == ''
        call s:EchoError('No program specified')
        return
    endif

    " Determine debugger type
    let debugger = s:DetectDebugger(program)
    let s:session.adapter = debugger
    let s:session.program = program

    " Start debug session
    call s:StartDebugSession(program, debugger)
endfunction

" Stop debugging session
function! magic_debugger#Stop()
    if !s:session.active
        call s:EchoWarning('No active debug session')
        return
    endif

    " Send stop command
    call s:SendCommand('stop')

    " Clean up
    call s:Cleanup()

    call s:EchoMessage('Debug session stopped')
endfunction

" Restart debugging session
function! magic_debugger#Restart()
    if !s:session.active
        call magic_debugger#Start()
        return
    endif

    let program = s:session.program
    let debugger = s:session.adapter

    " Stop current session
    call s:SendCommand('stop')
    call s:Cleanup()

    " Start new session
    call s:StartDebugSession(program, debugger)
endfunction

" Attach to running process
function! magic_debugger#Attach()
    let pid = input('Process ID to attach: ', '', 'customlist,s:CompletePIDs')
    if pid == ''
        return
    endif

    let s:session.pid = str2nr(pid)
    let debugger = g:magic_debugger_default_debugger
    let s:session.adapter = debugger

    call s:AttachToProcess(pid, debugger)
endfunction

" =============================================================================
" Execution Control
" =============================================================================

" Continue execution
function! magic_debugger#Continue()
    if !s:CheckSession() | return | endif
    call s:SendCommand('continue')
endfunction

" Step over
function! magic_debugger#Next()
    if !s:CheckSession() | return | endif
    call s:SendCommand('next')
endfunction

" Step into
function! magic_debugger#Step()
    if !s:CheckSession() | return | endif
    call s:SendCommand('step')
endfunction

" Step out
function! magic_debugger#Finish()
    if !s:CheckSession() | return | endif
    call s:SendCommand('finish')
endfunction

" Pause execution
function! magic_debugger#Pause()
    if !s:CheckSession() | return | endif
    call s:SendCommand('pause')
endfunction

" Run to cursor
function! magic_debugger#RunToCursor()
    if !s:CheckSession() | return | endif

    let file = expand('%:p')
    let line = line('.')

    call s:SendCommand('runto ' . file . ':' . line)
endfunction

" =============================================================================
" Breakpoints
" =============================================================================

" Toggle breakpoint at current line
" @param condition Optional condition expression
function! magic_debugger#ToggleBreakpoint(...)
    if !s:CheckSession() | return | endif

    let file = expand('%:p')
    let line = line('.')
    let condition = a:0 > 0 ? a:1 : ''

    " Check if breakpoint exists
    let bp_key = file . ':' . line
    if has_key(s:session.breakpoints, bp_key)
        " Remove breakpoint
        let bp_id = s:session.breakpoints[bp_key].id
        call s:SendCommand('breakpoint remove ' . bp_id)
        call remove(s:session.breakpoints, bp_key)
        call s:ClearBreakpointSign(file, line)
    else
        " Add breakpoint
        let cmd = 'breakpoint set ' . file . ':' . line
        if condition != ''
            let cmd .= ' if ' . condition
        endif
        call s:SendCommand(cmd)
    endif
endfunction

" Add breakpoint at location
" @param location Location string (file:line or function name)
function! magic_debugger#AddBreakpoint(location)
    if !s:CheckSession() | return | endif
    call s:SendCommand('breakpoint set ' . a:location)
endfunction

" Clear all breakpoints
function! magic_debugger#ClearBreakpoints()
    if !s:CheckSession() | return | endif

    call s:SendCommand('breakpoint clear')

    " Clear all breakpoint signs
    for bp_key in keys(s:session.breakpoints)
        let parts = split(bp_key, ':')
        call s:ClearBreakpointSign(parts[0], str2nr(parts[1]))
    endfor

    let s:session.breakpoints = {}
endfunction

" List breakpoints
function! magic_debugger#ListBreakpoints()
    if empty(s:session.breakpoints)
        call s:EchoMessage('No breakpoints set')
        return
    endif

    echo "Breakpoints:"
    for [key, bp] in items(s:session.breakpoints)
        let status = bp.enabled ? 'enabled' : 'disabled'
        echo printf("  %d: %s (%s)", bp.id, key, status)
    endfor
endfunction

" =============================================================================
" Variables and Expressions
" =============================================================================

" Evaluate expression
" @param expr Expression to evaluate
function! magic_debugger#Eval(expr)
    if !s:CheckSession() | return | endif

    if a:expr == ''
        " Use word under cursor
        let expr = expand('<cword>')
    else
        let expr = a:expr
    endif

    call s:SendCommand('eval ' . expr)
endfunction

" Evaluate visual selection
function! magic_debugger#EvalVisual()
    let saved_reg = @"
    normal! gv"xy
    let expr = @"
    let @" = saved_reg

    call magic_debugger#Eval(expr)
endfunction

" Add watch expression
" @param expr Expression to watch
function! magic_debugger#Watch(expr)
    if !s:CheckSession() | return | endif

    call add(s:session.watch_expressions, a:expr)
    call s:SendCommand('watch add ' . a:expr)

    " Update watch window
    call magic_debugger#ui#UpdateWatchWindow()
endfunction

" Set variable value
" @param assignment Variable=value assignment
function! magic_debugger#SetVariable(assignment)
    if !s:CheckSession() | return | endif

    call s:SendCommand('set ' . a:assignment)
endfunction

" =============================================================================
" Navigation
" =============================================================================

" Go up one stack frame
function! magic_debugger#UpFrame()
    if !s:CheckSession() | return | endif

    call s:SendCommand('frame up')
endfunction

" Go down one stack frame
function! magic_debugger#DownFrame()
    if !s:CheckSession() | return | endif

    call s:SendCommand('frame down')
endfunction

" Go to program counter location
function! magic_debugger#GotoPC()
    if !s:CheckSession() | return | endif

    if s:session.current_file != '' && s:session.current_line > 0
        execute 'edit ' . s:session.current_file
        execute s:session.current_line
        normal! zz
    endif
endfunction

" =============================================================================
" UI Integration
" =============================================================================

" Update signs in current buffer
function! magic_debugger#UpdateBufferSigns()
    let file = expand('%:p')
    if file == ''
        return
    endif

    " Clear existing signs
    execute 'sign unplace * buffer=' . bufnr('%')

    " Place breakpoint signs
    for [bp_key, bp] in items(s:session.breakpoints)
        let parts = split(bp_key, ':')
        if len(parts) >= 2 && parts[0] == file
            let line = str2nr(parts[1])
            let sign_name = bp.enabled ? 'MagicBreakpoint' : 'MagicBreakpointDisabled'
            let sign_id = s:sign_id_base + line
            execute printf('sign place %d line=%d name=%s buffer=%d',
                         \ sign_id, line, sign_name, bufnr('%'))
        endif
    endfor

    " Place current line sign
    if s:session.current_file == file && s:session.current_line > 0
        let sign_id = s:sign_current_base + s:session.current_line
        execute printf('sign place %d line=%d name=MagicCurrentLine buffer=%d',
                     \ sign_id, s:session.current_line, bufnr('%'))
    endif
endfunction

" Handle cursor movement
function! magic_debugger#OnCursorMoved()
    " Could be used for variable preview on hover
endfunction

" Show help
function! magic_debugger#ShowHelp()
    let help_text = [
        \ 'Magic Debugger - Key Bindings',
        \ '',
        \ 'Session Management:',
        \ '  ' . g:magic_debugger_map_prefix . 's    Start debugging',
        \ '  ' . g:magic_debugger_map_prefix . 'x    Stop debugging',
        \ '  ' . g:magic_debugger_map_prefix . 'r    Restart debugging',
        \ '',
        \ 'Execution Control:',
        \ '  ' . g:magic_debugger_map_prefix . 'c    Continue',
        \ '  ' . g:magic_debugger_map_prefix . 'n    Step over (next)',
        \ '  ' . g:magic_debugger_map_prefix . 'i    Step into',
        \ '  ' . g:magic_debugger_map_prefix . 'o    Step out (finish)',
        \ '  ' . g:magic_debugger_map_prefix . 'p    Pause',
        \ '',
        \ 'Breakpoints:',
        \ '  ' . g:magic_debugger_map_prefix . 'b    Toggle breakpoint',
        \ '  ' . g:magic_debugger_map_prefix . 'B    Clear all breakpoints',
        \ '',
        \ 'Navigation:',
        \ '  ' . g:magic_debugger_map_prefix . 'u    Up stack frame',
        \ '  ' . g:magic_debugger_map_prefix . 'd    Down stack frame',
        \ '  ' . g:magic_debugger_map_prefix . 'g    Go to PC location',
        \ '',
        \ 'Variables:',
        \ '  ' . g:magic_debugger_map_prefix . 'e    Evaluate expression',
        \ '  ' . g:magic_debugger_map_prefix . 'w    Add watch expression',
        \ '',
        \ 'Function Keys:',
        \ '  F5        Continue',
        \ '  F6/F10    Step over',
        \ '  F7        Step into',
        \ '  F8        Step out',
        \ '  F9        Toggle breakpoint',
        \ '  S-F5      Stop',
        \ '  C-F5      Restart',
    \ ]

    echo join(help_text, "\n")
endfunction

" =============================================================================
" Internal Functions
" =============================================================================

" Check if session is active
function! s:CheckSession()
    if !s:session.active
        call s:EchoWarning('No active debug session. Use :MagicDebugStart first.')
        return 0
    endif
    return 1
endfunction

" Detect debugger type from file extension
function! s:DetectDebugger(file)
    let ext = fnamemodify(a:file, ':e')

    " Check for shell scripts
    if ext == 'sh' || ext == 'bash'
        return 'shell'
    endif

    " Check file shebang
    let first_line = readfile(a:file, '', 1)
    if len(first_line) > 0
        if first_line[0] =~# 'bash\|sh'
            return 'shell'
        endif
    endif

    " Default to configured debugger
    return g:magic_debugger_default_debugger
endfunction

" Start debug session
function! s:StartDebugSession(program, debugger)
    " Initialize session state
    let s:session.active = 1
    let s:session.state = 'starting'
    let s:session.program = a:program
    let s:session.adapter = a:debugger

    " Build command
    let cmd = s:BuildDebugCommand(a:program, a:debugger)

    " Start job/channel for communication
    if has('nvim')
        " Neovim job
        let s:session.job_id = jobstart(cmd, {
            \ 'on_stdout': function('s:OnStdout'),
            \ 'on_stderr': function('s:OnStderr'),
            \ 'on_exit': function('s:OnExit'),
        \ })
    else
        " Vim 8 channel
        let s:session.job_id = job_start(cmd, {
            \ 'out_cb': function('s:OnChannelOut'),
            \ 'err_cb': function('s:OnChannelErr'),
            \ 'exit_cb': function('s:OnChannelExit'),
        \ })
    endif

    if s:session.job_id <= 0
        call s:EchoError('Failed to start debugger')
        call s:Cleanup()
        return
    endif

    " Open UI if auto-open is enabled
    if g:magic_debugger_auto_open
        call magic_debugger#ui#Open()
    endif

    call s:EchoMessage('Started debugging: ' . a:program)
    let s:session.state = 'running'
endfunction

" Build debug command
function! s:BuildDebugCommand(program, debugger)
    let cmd = g:magic_debugger_path

    " Add debugger type
    let cmd .= ' --debugger=' . a:debugger

    " Add program
    let cmd .= ' ' . shellescape(a:program)

    return cmd
endfunction

" Attach to process
function! s:AttachToProcess(pid, debugger)
    " Similar to StartDebugSession but with --attach flag
    let cmd = g:magic_debugger_path
    let cmd .= ' --debugger=' . a:debugger
    let cmd .= ' --attach=' . a:pid

    let s:session.active = 1
    let s:session.state = 'attached'

    call s:EchoMessage('Attached to process ' . a:pid)
endfunction

" Send command to debugger
function! s:SendCommand(cmd)
    if s:session.job_id < 0
        return
    endif

    let full_cmd = a:cmd . "\n"

    if has('nvim')
        call chansend(s:session.job_id, full_cmd)
    else
        call ch_sendraw(s:session.job_id, full_cmd)
    endif

    " Log command
    call magic_debugger#ui#AddOutput('command', '> ' . a:cmd)
endfunction

" Handle stdout (Neovim)
function! s:OnStdout(job_id, data, event)
    for line in a:data
        call s:ProcessOutput(line)
    endfor
endfunction

" Handle stderr (Neovim)
function! s:OnStderr(job_id, data, event)
    for line in a:data
        call magic_debugger#ui#AddOutput('error', line)
    endfor
endfunction

" Handle exit (Neovim)
function! s:OnExit(job_id, data, event)
    call s:Cleanup()
    call s:EchoMessage('Debugger exited with code ' . a:data)
endfunction

" Handle channel output (Vim 8)
function! s:OnChannelOut(channel, msg)
    call s:ProcessOutput(a:msg)
endfunction

" Handle channel error (Vim 8)
function! s:OnChannelErr(channel, msg)
    call magic_debugger#ui#AddOutput('error', a:msg)
endfunction

" Handle channel exit (Vim 8)
function! s:OnChannelExit(channel, msg)
    call s:Cleanup()
endfunction

" Process debugger output
function! s:ProcessOutput(line)
    " Parse JSON responses
    if a:line =~# '^{'
        call s:ParseJSONResponse(a:line)
    else
        " Plain text output
        call magic_debugger#ui#AddOutput('output', a:line)
    endif
endfunction

" Parse JSON response from debugger
function! s:ParseJSONResponse(json)
    try
        let response = json_decode(a:json)
    catch
        call magic_debugger#ui#AddOutput('error', 'Parse error: ' . a:json)
        return
    endtry

    " Handle different response types
    if has_key(response, 'type')
        if response.type == 'event'
            call s:HandleEvent(response)
        elseif response.type == 'response'
            call s:HandleResponse(response)
        endif
    endif
endfunction

" Handle debugger event
function! s:HandleEvent(event)
    if !has_key(a:event, 'event')
        return
    endif

    let event_name = a:event.event

    if event_name == 'stopped'
        " Execution stopped
        let s:session.state = 'stopped'

        if has_key(a:event, 'body')
            let body = a:event.body

            " Update current location
            if has_key(body, 'threadId')
                let s:session.current_thread = body.threadId
            endif

            " Update UI
            call magic_debugger#ui#OnStopped(body)
        endif

    elseif event_name == 'continued'
        " Execution continued
        let s:session.state = 'running'
        call magic_debugger#ui#OnContinued()

    elseif event_name == 'terminated'
        " Debug session terminated
        let s:session.state = 'terminated'
        call s:Cleanup()
        call s:EchoMessage('Debug session terminated')

    elseif event_name == 'breakpoint'
        " Breakpoint changed
        if has_key(a:event, 'body')
            call s:UpdateBreakpoint(a:event.body)
        endif

    elseif event_name == 'output'
        " Output from debuggee
        if has_key(a:event, 'body') && has_key(a:event.body, 'output')
            call magic_debugger#ui#AddOutput('output', a:event.body.output)
        endif
    endif
endfunction

" Handle debugger response
function! s:HandleResponse(response)
    if !has_key(a:response, 'command')
        return
    endif

    let cmd = a:response.command
    let success = get(a:response, 'success', 0)

    if cmd == 'stackTrace' && success
        call magic_debugger#ui#UpdateStack(a:response.body)
    elseif cmd == 'scopes' && success
        call magic_debugger#ui#UpdateScopes(a:response.body)
    elseif cmd == 'variables' && success
        call magic_debugger#ui#UpdateVariables(a:response.body)
    elseif cmd == 'threads' && success
        call magic_debugger#ui#UpdateThreads(a:response.body)
    elseif cmd == 'evaluate' && success
        call magic_debugger#ui#ShowEvalResult(a:response.body)
    endif
endfunction

" Update breakpoint state
function! s:UpdateBreakpoint(body)
    if !has_key(a:body, 'breakpoint')
        return
    endif

    let bp = a:body.breakpoint
    let bp_key = ''
    let bp_data = {
        \ 'id': get(bp, 'id', 0),
        \ 'enabled': get(bp, 'enabled', 1),
        \ 'line': get(bp, 'line', 0),
        \ 'condition': get(bp, 'condition', ''),
    \ }

    if has_key(bp, 'source') && has_key(bp.source, 'path')
        let bp_key = bp.source.path . ':' . bp.line
        let bp_data.file = bp.source.path
    endif

    if bp_key != ''
        let s:session.breakpoints[bp_key] = bp_data
        call s:PlaceBreakpointSign(bp_data.file, bp_data.line, bp_data.enabled)
    endif
endfunction

" Place breakpoint sign
function! s:PlaceBreakpointSign(file, line, enabled)
    let bufnr = bufnr(a:file)
    if bufnr == -1
        return
    endif

    let sign_name = a:enabled ? 'MagicBreakpoint' : 'MagicBreakpointDisabled'
    let sign_id = s:sign_id_base + a:line

    execute printf('sign place %d line=%d name=%s buffer=%d',
                 \ sign_id, a:line, sign_name, bufnr)
endfunction

" Clear breakpoint sign
function! s:ClearBreakpointSign(file, line)
    let bufnr = bufnr(a:file)
    if bufnr == -1
        return
    endif

    let sign_id = s:sign_id_base + a:line
    execute printf('sign unplace %d buffer=%d', sign_id, bufnr)
endfunction

" Update current execution position
function! magic_debugger#UpdatePosition(file, line)
    " Clear previous position sign
    if s:session.current_file != ''
        let prev_bufnr = bufnr(s:session.current_file)
        if prev_bufnr != -1 && s:session.current_line > 0
            let sign_id = s:sign_current_base + s:session.current_line
            execute printf('sign unplace %d buffer=%d', sign_id, prev_bufnr)
        endif
    endif

    " Update position
    let s:session.current_file = a:file
    let s:session.current_line = a:line

    " Open file and place sign
    let bufnr = bufnr(a:file)
    if bufnr == -1
        execute 'edit ' . a:file
        let bufnr = bufnr('%')
    endif

    let sign_id = s:sign_current_base + a:line
    execute printf('sign place %d line=%d name=MagicCurrentLine buffer=%d',
                 \ sign_id, a:line, bufnr)

    " Move cursor
    execute 'buffer ' . bufnr
    execute a:line
    normal! zz
endfunction

" Clean up session
function! s:Cleanup()
    " Clear signs
    sign unplace *

    " Close UI windows
    call magic_debugger#ui#Close()

    " Stop job
    if s:session.job_id > 0
        if has('nvim')
            call jobstop(s:session.job_id)
        else
            call job_stop(s:session.job_id)
        endif
    endif

    " Reset state
    let s:session.active = 0
    let s:session.state = 'inactive'
    let s:session.job_id = -1
    let s:session.current_file = ''
    let s:session.current_line = 0
endfunction

function! magic_debugger#Cleanup()
    call s:Cleanup()
endfunction

" =============================================================================
" Utility Functions
" =============================================================================

" Echo message
function! s:EchoMessage(msg)
    echohl None
    echo '[MagicDebugger] ' . a:msg
endfunction

" Echo warning
function! s:EchoWarning(msg)
    echohl WarningMsg
    echo '[MagicDebugger] ' . a:msg
    echohl None
endfunction

" Echo error
function! s:EchoError(msg)
    echohl ErrorMsg
    echo '[MagicDebugger] ' . a:msg
    echohl None
endfunction

" Complete PIDs for attach command
function! s:CompletePIDs(A, L, P)
    " Would list available processes
    return []
endfunction
