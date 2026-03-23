" =============================================================================
" File: cmd.vim
" Description: Magic Debugger Command Module - Debug commands and keybindings
"
" This module provides additional command functionality for Magic Debugger:
"   - Command completion
"   - Command history
"   - Custom command handlers
"
" Phase 5: Frontend - Vim Plugin Commands
" =============================================================================

" Prevent duplicate loading
if exists('g:loaded_magic_debugger_cmd')
    finish
endif
let g:loaded_magic_debugger_cmd = 1

" =============================================================================
" Command Table
" =============================================================================

" Available commands and their descriptions
let s:commands = {
    \ 'start': {
        \ 'desc': 'Start debugging session',
        \ 'usage': 'start [program]',
        \ 'complete': 'file',
    \ },
    \ 'stop': {
        \ 'desc': 'Stop debugging session',
        \ 'usage': 'stop',
        \ 'complete': '',
    \ },
    \ 'restart': {
        \ 'desc': 'Restart debugging session',
        \ 'usage': 'restart',
        \ 'complete': '',
    \ },
    \ 'continue': {
        \ 'desc': 'Continue execution',
        \ 'usage': 'continue',
        \ 'complete': '',
    \ },
    \ 'next': {
        \ 'desc': 'Step over',
        \ 'usage': 'next',
        \ 'complete': '',
    \ },
    \ 'step': {
        \ 'desc': 'Step into',
        \ 'usage': 'step',
        \ 'complete': '',
    \ },
    \ 'finish': {
        \ 'desc': 'Step out',
        \ 'usage': 'finish',
        \ 'complete': '',
    \ },
    \ 'pause': {
        \ 'desc': 'Pause execution',
        \ 'usage': 'pause',
        \ 'complete': '',
    \ },
    \ 'breakpoint': {
        \ 'desc': 'Manage breakpoints',
        \ 'usage': 'breakpoint set|remove|clear|list',
        \ 'complete': 'customlist,s:CompleteBreakpoint',
    \ },
    \ 'eval': {
        \ 'desc': 'Evaluate expression',
        \ 'usage': 'eval <expression>',
        \ 'complete': '',
    \ },
    \ 'watch': {
        \ 'desc': 'Add watch expression',
        \ 'usage': 'watch <expression>',
        \ 'complete': '',
    \ },
    \ 'set': {
        \ 'desc': 'Set variable value',
        \ 'usage': 'set <variable>=<value>',
        \ 'complete': '',
    \ },
    \ 'frame': {
        \ 'desc': 'Navigate call stack',
        \ 'usage': 'frame up|down|select <n>',
        \ 'complete': 'customlist,s:CompleteFrame',
    \ },
    \ 'thread': {
        \ 'desc': 'Manage threads',
        \ 'usage': 'thread list|select <id>',
        \ 'complete': 'customlist,s:CompleteThread',
    \ },
    \ 'help': {
        \ 'desc': 'Show help',
        \ 'usage': 'help [command]',
        \ 'complete': 'customlist,s:CompleteCommand',
    \ },
\ }

" Command aliases
let s:aliases = {
    \ 'c': 'continue',
    \ 'n': 'next',
    \ 's': 'step',
    \ 'f': 'finish',
    \ 'b': 'breakpoint',
    \ 'p': 'eval',
    \ 'r': 'restart',
    \ 'q': 'stop',
    \ '?': 'help',
\ }

" Command history
let s:history = []
let s:history_index = 0
let s:max_history = 100

" =============================================================================
" Public Functions
" =============================================================================

" Execute a command string
function! magic_debugger#cmd#Execute(cmdstr)
    " Parse command
    let parts = split(a:cmdstr, '\s\+')
    if empty(parts)
        return
    endif

    let cmd = parts[0]
    let args = len(parts) > 1 ? parts[1:] : []

    " Resolve alias
    if has_key(s:aliases, cmd)
        let cmd = s:aliases[cmd]
    endif

    " Add to history
    call s:AddHistory(a:cmdstr)

    " Execute command
    if has_key(s:commands, cmd)
        call call('s:Cmd_' . cmd, [args])
    else
        echoerr 'Unknown command: ' . cmd
    endif
endfunction

" Complete command
function! magic_debugger#cmd#Complete(A, L, P)
    let parts = split(a:L, '\s\+')

    if len(parts) == 0 || (len(parts) == 1 && a:A != '')
        " Complete command name
        return s:CompleteCommand(a:A, a:L, a:P)
    elseif len(parts) >= 1
        " Complete command arguments
        let cmd = parts[0]

        " Resolve alias
        if has_key(s:aliases, cmd)
            let cmd = s:aliases[cmd]
        endif

        if has_key(s:commands, cmd) && has_key(s:commands[cmd], 'complete')
            let complete_type = s:commands[cmd].complete
            if complete_type != ''
                if complete_type == 'file'
                    return s:CompleteFile(a:A, a:L, a:P)
                elseif complete_type =~# '^customlist,'
                    let func = substitute(complete_type, '^customlist,', '', '')
                    return call(func, [a:A, a:L, a:P])
                endif
            endif
        endif
    endif

    return []
endfunction

" Get history
function! magic_debugger#cmd#GetHistory()
    return s:history
endfunction

" Navigate history
function! magic_debugger#cmd#HistoryPrev()
    if s:history_index > 0
        let s:history_index -= 1
        return s:history[s:history_index]
    endif
    return ''
endfunction

function! magic_debugger#cmd#HistoryNext()
    if s:history_index < len(s:history) - 1
        let s:history_index += 1
        return s:history[s:history_index]
    elseif s:history_index == len(s:history) - 1
        let s:history_index = len(s:history)
        return ''
    endif
    return ''
endfunction

" Show command help
function! magic_debugger#cmd#ShowHelp(...)
    if a:0 > 0 && a:1 != ''
        " Show help for specific command
        let cmd = a:1
        if has_key(s:aliases, cmd)
            let cmd = s:aliases[cmd]
        endif

        if has_key(s:commands, cmd)
            echohl Title
            echo cmd
            echohl None
            echo '  ' . s:commands[cmd].desc
            echo '  Usage: ' . s:commands[cmd].usage
        else
            echoerr 'Unknown command: ' . a:1
        endif
    else
        " Show all commands
        echohl Title
        echo 'Magic Debugger Commands:'
        echohl None

        for [cmd, info] in items(s:commands)
            echo printf('  %-12s %s', cmd, info.desc)
        endfor

        echo ''
        echo 'Use :MagicDebugHelp <command> for more information'
    endif
endfunction

" =============================================================================
" Command Implementations
" =============================================================================

function! s:Cmd_start(args)
    let program = len(a:args) > 0 ? a:args[0] : ''
    call magic_debugger#Start(program)
endfunction

function! s:Cmd_stop(args)
    call magic_debugger#Stop()
endfunction

function! s:Cmd_restart(args)
    call magic_debugger#Restart()
endfunction

function! s:Cmd_continue(args)
    call magic_debugger#Continue()
endfunction

function! s:Cmd_next(args)
    call magic_debugger#Next()
endfunction

function! s:Cmd_step(args)
    call magic_debugger#Step()
endfunction

function! s:Cmd_finish(args)
    call magic_debugger#Finish()
endfunction

function! s:Cmd_pause(args)
    call magic_debugger#Pause()
endfunction

function! s:Cmd_breakpoint(args)
    if empty(a:args)
        echoerr 'Usage: breakpoint set|remove|clear|list'
        return
    endif

    let subcmd = a:args[0]
    let subargs = a:args[1:]

    if subcmd == 'set'
        if empty(subargs)
            call magic_debugger#ToggleBreakpoint()
        else
            call magic_debugger#AddBreakpoint(join(subargs))
        endif
    elseif subcmd == 'remove'
        if empty(subargs)
            echoerr 'Usage: breakpoint remove <id>'
        else
            " Would remove breakpoint by ID
            echo 'Remove breakpoint: ' . subargs[0]
        endif
    elseif subcmd == 'clear'
        call magic_debugger#ClearBreakpoints()
    elseif subcmd == 'list'
        call magic_debugger#ListBreakpoints()
    else
        echoerr 'Unknown breakpoint subcommand: ' . subcmd
    endif
endfunction

function! s:Cmd_eval(args)
    if empty(a:args)
        echoerr 'Usage: eval <expression>'
        return
    endif
    call magic_debugger#Eval(join(a:args))
endfunction

function! s:Cmd_watch(args)
    if empty(a:args)
        echoerr 'Usage: watch <expression>'
        return
    endif
    call magic_debugger#Watch(join(a:args))
endfunction

function! s:Cmd_set(args)
    if empty(a:args)
        echoerr 'Usage: set <variable>=<value>'
        return
    endif
    call magic_debugger#SetVariable(join(a:args))
endfunction

function! s:Cmd_frame(args)
    if empty(a:args)
        echoerr 'Usage: frame up|down|select <n>'
        return
    endif

    let subcmd = a:args[0]

    if subcmd == 'up'
        call magic_debugger#UpFrame()
    elseif subcmd == 'down'
        call magic_debugger#DownFrame()
    elseif subcmd == 'select' && len(a:args) > 1
        " Would select frame by index
        echo 'Select frame: ' . a:args[1]
    else
        echoerr 'Unknown frame subcommand: ' . subcmd
    endif
endfunction

function! s:Cmd_thread(args)
    if empty(a:args)
        echoerr 'Usage: thread list|select <id>'
        return
    endif

    let subcmd = a:args[0]

    if subcmd == 'list'
        echo 'Threads would be listed here'
    elseif subcmd == 'select' && len(a:args) > 1
        echo 'Select thread: ' . a:args[1]
    else
        echoerr 'Unknown thread subcommand: ' . subcmd
    endif
endfunction

function! s:Cmd_help(args)
    call magic_debugger#cmd#ShowHelp(len(a:args) > 0 ? a:args[0] : '')
endfunction

" =============================================================================
" Completion Functions
" =============================================================================

function! s:CompleteCommand(A, L, P)
    let commands = keys(s:commands) + keys(s:aliases)
    return filter(commands, 'v:val =~# "^" . a:A')
endfunction

function! s:CompleteFile(A, L, P)
    return getcompletion(a:A, 'file')
endfunction

function! s:CompleteBreakpoint(A, L, P)
    let subcmds = ['set', 'remove', 'clear', 'list']
    let parts = split(a:L, '\s\+')

    if len(parts) <= 2
        return filter(subcmds, 'v:val =~# "^" . a:A')
    endif

    return []
endfunction

function! s:CompleteFrame(A, L, P)
    let subcmds = ['up', 'down', 'select']
    let parts = split(a:L, '\s\+')

    if len(parts) <= 2
        return filter(subcmds, 'v:val =~# "^" . a:A')
    endif

    return []
endfunction

function! s:CompleteThread(A, L, P)
    let subcmds = ['list', 'select']
    let parts = split(a:L, '\s\+')

    if len(parts) <= 2
        return filter(subcmds, 'v:val =~# "^" . a:A')
    endif

    return []
endfunction

" =============================================================================
" Internal Functions
" =============================================================================

function! s:AddHistory(cmdstr)
    " Don't add duplicates
    if !empty(s:history) && s:history[-1] == a:cmdstr
        return
    endif

    call add(s:history, a:cmdstr)

    " Limit history size
    if len(s:history) > s:max_history
        call remove(s:history, 0)
    endif

    let s:history_index = len(s:history)
endfunction
