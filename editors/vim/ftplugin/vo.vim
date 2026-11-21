" Editing settings for vo: `#` comments, four-space indentation (the form `vo fmt` writes).
if exists('b:did_ftplugin')
  finish
endif
let b:did_ftplugin = 1

setlocal commentstring=#\ %s
setlocal comments=:#
setlocal expandtab shiftwidth=4 softtabstop=4 tabstop=4
setlocal cindent cinkeys=0{,0},0),0],!^F,o,O cinoptions=(0,j1
setlocal suffixesadd=.vo

let b:undo_ftplugin = 'setlocal commentstring< comments< expandtab< shiftwidth< softtabstop< tabstop< cindent< cinkeys< cinoptions< suffixesadd<'
