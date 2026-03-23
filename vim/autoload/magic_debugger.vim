" =============================================================================
" File: magic_debugger.vim
" Description: Magic Debugger - A DAP-based unified debugging plugin for Vim
" Author: Magic Debugger Team
" Version: 0.1.0
" License: MIT
"
" This plugin provides integration with Magic Debugger, a DAP-based debugging
" platform supporting LLDB, GDB, and shell script debugging.
"
" Features:
"   - Breakpoint management with visual markers
"   - Step-by-step execution (over, into, out)
"   - Variable inspection and modification
"   - Call stack navigation
"   - Thread management
"   - Watch expressions
"   - Real-time source highlighting
"
" Usage:
"   :MagicDebugStart    - Start debugging session
"   :MagicDebugStop     - Stop debugging session
"   :MagicDebugContinue - Continue execution
"   :MagicDebugStep     - Step into
"   :MagicDebugNext     - Step over
"   :MagicDebugFinish   - Step out
"   :MagicDebugToggleBreakpoint - Toggle breakpoint
"
" Phase 5: Frontend - Vim Plugin
" =============================================================================

" Prevent duplicate loading
if exists('g:loaded_magic_debugger')
    finish
endif
let g:loaded_magic_debugger = 1

" Save compatibility options
let s:save_cpo = &cpo
set cpo&vim

" =============================================================================
" Configuration Variables
" =============================================================================

" Enable/disable plugin
if !exists('g:magic_debugger_enabled')
    let g:magic_debugger_enabled = 1
endif

" Path to magic_debugger executable/library
if !exists('g:magic_debugger_path')
    let g:magic_debugger_path = 'magic_debugger'
endif

" Default debugger type (lldb, gdb, shell)
if !exists('g:magic_debugger_default_debugger')
    let g:magic_debugger_default_debugger = 'lldb'
endif

" Layout configuration
if !exists('g:magic_debugger_layout')
    let g:magic_debugger_layout = 'horizontal'  " 'horizontal' or 'vertical'
endif

" Window heights
if !exists('g:magic_debugger_source_height')
    let g:magic_debugger_source_height = 0  " 0 = auto
endif

if !exists('g:magic_debugger_stack_height')
    let g:magic_debugger_stack_height = 8
endif

if !exists('g:magic_debugger_vars_width')
    let g:magic_debugger_vars_width = 40
endif

" Breakpoint signs
if !exists('g:magic_debugger_sign_breakpoint')
    let g:magic_debugger_sign_breakpoint = 'B>'
endif

if !exists('g:magic_debugger_sign_breakpoint_disabled')
    let g:magic_debugger_sign_breakpoint_disabled = 'B#'
endif

if !exists('g:magic_debugger_sign_current')
    let g:magic_debugger_sign_current = '->'
endif

" Highlight groups
if !exists('g:magic_debugger_hl_current_line')
    let g:magic_debugger_hl_current_line = 'PmenuSel'
endif

if !exists('g:magic_debugger_hl_breakpoint')
    let g:magic_debugger_hl_breakpoint = 'ErrorMsg'
endif

if !exists('g:magic_debugger_hl_breakpoint_disabled')
    let g:magic_debugger_hl_breakpoint_disabled = 'Comment'
endif

" Auto-open on start
if !exists('g:magic_debugger_auto_open')
    let g:magic_debugger_auto_open = 1
endif

" Update interval (ms)
if !exists('g:magic_debugger_update_interval')
    let g:magic_debugger_update_interval = 100
endif

" =============================================================================
" Signs Definition
" =============================================================================

" Define signs for breakpoints and current line
function! s:DefineSigns()
    " Breakpoint sign
    execute 'sign define MagicBreakpoint text=' . g:magic_debugger_sign_breakpoint .
          \ ' texthl=' . g:magic_debugger_hl_breakpoint .
          \ ' linehl=MagicDebuggerBreakpointLine'

    " Disabled breakpoint sign
    execute 'sign define MagicBreakpointDisabled text=' .
          \ g:magic_debugger_sign_breakpoint_disabled .
          \ ' texthl=' . g:magic_debugger_hl_breakpoint_disabled .
          \ ' linehl=MagicDebuggerBreakpointDisabledLine'

    " Current line sign
    execute 'sign define MagicCurrentLine text=' . g:magic_debugger_sign_current .
          \ ' texthl=Search linehl=' . g:magic_debugger_hl_current_line

    " Combined breakpoint and current line
    sign define MagicBreakpointCurrent text=*> texthl=ErrorMsg linehl=PmenuSel
endfunction

" Define highlight groups
function! s:DefineHighlights()
    " Breakpoint line highlight
    highlight default link MagicDebuggerBreakpointLine DiffDelete

    " Disabled breakpoint line highlight
    highlight default link MagicDebuggerBreakpointDisabledLine Comment

    " Current line highlight
    highlight default link MagicDebuggerCurrentLine CursorLine

    " Variables highlight
    highlight default link MagicDebuggerVariable Identifier
    highlight default link MagicDebuggerValue String
    highlight default link MagicDebuggerType Type

    " Stack frame highlight
    highlight default link MagicDebuggerStackFrame Statement
    highlight default link MagicDebuggerStackFrameCurrent Search
endfunction

" =============================================================================
" Commands
" =============================================================================

" Session management
command! -nargs=? -complete=file MagicDebugStart call magic_debugger#Start(<q-args>)
command! MagicDebugStop call magic_debugger#Stop()
command! MagicDebugRestart call magic_debugger#Restart()
command! MagicDebugAttach call magic_debugger#Attach()

" Execution control
command! MagicDebugContinue call magic_debugger#Continue()
command! MagicDebugNext call magic_debugger#Next()
command! MagicDebugStep call magic_debugger#Step()
command! MagicDebugFinish call magic_debugger#Finish()
command! MagicDebugPause call magic_debugger#Pause()
command! MagicDebugRunToCursor call magic_debugger#RunToCursor()

" Breakpoints
command! -nargs=? MagicDebugToggleBreakpoint call magic_debugger#ToggleBreakpoint(<q-args>)
command! -nargs=1 MagicDebugAddBreakpoint call magic_debugger#AddBreakpoint(<q-args>)
command! -nargs=0 MagicDebugClearBreakpoints call magic_debugger#ClearBreakpoints()
command! MagicDebugListBreakpoints call magic_debugger#ListBreakpoints()

" Variables and expressions
command! -nargs=1 MagicDebugEval call magic_debugger#Eval(<q-args>)
command! -nargs=1 MagicDebugWatch call magic_debugger#Watch(<q-args>)
command! -nargs=1 MagicDebugSet call magic_debugger#SetVariable(<q-args>)

" Navigation
command! MagicDebugUp call magic_debugger#UpFrame()
command! MagicDebugDown call magic_debugger#DownFrame()
command! MagicDebugGotoPC call magic_debugger#GotoPC()

" UI commands
command! MagicDebugToggleUI call magic_debugger#ui#Toggle()
command! MagicDebugFocusSource call magic_debugger#ui#FocusSource()
command! MagicDebugFocusStack call magic_debugger#ui#FocusStack()
command! MagicDebugFocusVars call magic_debugger#ui#FocusVars()
command! MagicDebugFocusOutput call magic_debugger#ui#FocusOutput()

" Help
command! MagicDebugHelp call magic_debugger#ShowHelp()

" =============================================================================
" Mappings
" =============================================================================

" Create mappings prefix
if !exists('g:magic_debugger_map_prefix')
    let g:magic_debugger_map_prefix = '<Leader>d'
endif

" Default mappings
function! s:CreateMappings()
    let prefix = g:magic_debugger_map_prefix

    " Session management
    execute 'nnoremap <silent> ' . prefix . 's :MagicDebugStart<CR>'
    execute 'nnoremap <silent> ' . prefix . 'x :MagicDebugStop<CR>'
    execute 'nnoremap <silent> ' . prefix . 'r :MagicDebugRestart<CR>'

    " Execution control
    execute 'nnoremap <silent> ' . prefix . 'c :MagicDebugContinue<CR>'
    execute 'nnoremap <silent> ' . prefix . 'n :MagicDebugNext<CR>'
    execute 'nnoremap <silent> ' . prefix . 'i :MagicDebugStep<CR>'
    execute 'nnoremap <silent> ' . prefix . 'o :MagicDebugFinish<CR>'
    execute 'nnoremap <silent> ' . prefix . 'p :MagicDebugPause<CR>'

    " Breakpoints
    execute 'nnoremap <silent> ' . prefix . 'b :MagicDebugToggleBreakpoint<CR>'
    execute 'nnoremap <silent> ' . prefix . 'B :MagicDebugClearBreakpoints<CR>'

    " Navigation
    execute 'nnoremap <silent> ' . prefix . 'u :MagicDebugUp<CR>'
    execute 'nnoremap <silent> ' . prefix . 'd :MagicDebugDown<CR>'
    execute 'nnoremap <silent> ' . prefix . 'g :MagicDebugGotoPC<CR>'

    " Evaluation
    execute 'nnoremap <silent> ' . prefix . 'e :MagicDebugEval '
    execute 'nnoremap <silent> ' . prefix . 'w :MagicDebugWatch '

    " UI
    execute 'nnoremap <silent> ' . prefix . 't :MagicDebugToggleUI<CR>'
    execute 'nnoremap <silent> ' . prefix . '? :MagicDebugHelp<CR>'

    " Function key mappings (common debugger conventions)
    nnoremap <silent> <F5> :MagicDebugContinue<CR>
    nnoremap <silent> <F6> :MagicDebugNext<CR>
    nnoremap <silent> <F7> :MagicDebugStep<CR>
    nnoremap <silent> <F8> :MagicDebugFinish<CR>
    nnoremap <silent> <F9> :MagicDebugToggleBreakpoint<CR>
    nnoremap <silent> <F10> :MagicDebugNext<CR>
    nnoremap <silent> <S-F5> :MagicDebugStop<CR>
    nnoremap <silent> <C-F5> :MagicDebugRestart<CR>

    " Visual mode evaluation
    vnoremap <silent> <Plug>MagicDebugEvalVisual :<C-U>call magic_debugger#EvalVisual()<CR>
endfunction

" =============================================================================
" Autocommands
" =============================================================================

" Set up autocommands
function! s:SetupAutocmds()
    augroup MagicDebugger
        autocmd!
        " Clean up on Vim exit
        autocmd VimLeavePre * call magic_debugger#Cleanup()

        " Update signs on buffer read
        autocmd BufReadPost * call magic_debugger#UpdateBufferSigns()

        " Handle cursor movement
        autocmd CursorMoved * call magic_debugger#OnCursorMoved()
    augroup END
endfunction

" =============================================================================
" Initialization
" =============================================================================

function! s:Initialize()
    " Check if plugin is enabled
    if !g:magic_debugger_enabled
        return
    endif

    " Define signs and highlights
    call s:DefineSigns()
    call s:DefineHighlights()

    " Create mappings
    call s:CreateMappings()

    " Set up autocommands
    call s:SetupAutocmds()

    " Initialize internal state
    call magic_debugger#Init()
endfunction

" Run initialization
call s:Initialize()

" =============================================================================
" Restore compatibility options
" =============================================================================
let &cpo = s:save_cpo
unlet s:save_cpo
