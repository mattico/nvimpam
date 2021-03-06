if ! exists('s:jobid')
  let s:jobid = 0
endif

if has("win32")
  let s:scriptdir = resolve(expand('<sfile>:p:h') . '\..')
  let s:bin = s:scriptdir . '\target\debug\nvimpam'

else
  let s:scriptdir = resolve(expand('<sfile>:p:h') . '/..')
  "let s:bin = s:scriptdir . '/files/redir.sh' 
  let s:bin = s:scriptdir . '/target/debug/nvimpam'
endif

function! nvimpam#init()
  call nvimpam#connect()
endfunction

function! nvimpam#connect()
  let result = s:StartJob()

  if 0 == result
    echoerr "Nvimpam: cannot start rpc process"
  elseif -1 == result
    echoerr "Nvimpam: rpc process is not executable: " . s:bin
    echoerr s:bin
  else
    let s:jobid = result
    call s:ConfigureJob(result)
  endif
endfunction

function! nvimpam#stop()
  call s:StopJob()
endfunction

function! nvimpam#reset()
  let s:jobid = 0
endfunction

function! s:ConfigureJob(jobid)
  augroup nvimPam
    " clear all previous autocommands
    autocmd!

    autocmd VimLeavePre * :call s:StopJob()

    "autocmd InsertChange * :call s:NotifyInsertChange()
    "autocmd InsertEnter * :call s:NotifyInsertEnter()
    "autocmd InsertLeave * :call s:NotifyInsertLeave()

    "autocmd CursorMovedI * :call s:NotifyCursorMovedI()
  augroup END
endfunction

function! nvimpam#updatefolds()
  call rpcnotify(s:jobid, 'RefreshFolds')
endfunction
"function! s:NotifyCursorMovedI()
"  let [ bufnum, lnum, column, off ] = getpos('.')
"  call rpcnotify(s:jobid, 'cursor-moved-i', lnum, column)
"endfunction

"function! s:NotifyInsertChange()
"  let [ bufnum, lnum, column, off ] = getpos('.')
"  call rpcnotify(s:jobid, 'insert-change', v:insertmode, lnum, column)
"endfunction
"
"function! s:NotifyInsertEnter()
"  let [ bufnum, lnum, column, off ] = getpos('.')
"  call rpcnotify(s:jobid, 'insert-enter', v:insertmode, lnum, column)
"endfunction
"
"function! s:NotifyInsertLeave()
"  call rpcnotify(s:jobid, 'insert-leave')
"endfunction

let s:stderr_chunks = ['']
function! s:OnStderr(id, data, event) dict
  let s:stderr_chunks[-1] .= a:data[0]
  call extend(s:stderr_chunks, a:data[1:])
endfunction

function! s:StartJob()
  if 0 == s:jobid
    let id = jobstart([s:bin], { 'rpc': v:true, 'on_stderr': function('s:OnStderr') })
    echom id
    return id
  else
    return 0
  endif
endfunction

function! s:StopJob()
  call writefile(s:stderr_chunks, "stderr")
  if 0 < s:jobid
    augroup nvimPam
      " clear all previous autocommands
      autocmd!
    augroup END
    echom s:jobid

    call rpcnotify(s:jobid, 'quit')
    let result = jobwait([s:jobid], 500)[0]

    if -1 == result
      " kill the job
      call jobstop(s:jobid)
    endif

    " reset job id back to zero
    let s:jobid = 0
  endif
endfunction
