" Vim syntax file for vo (https://github.com/d4n-larsson/volang).
" The word lists are checked against the language by tests/grammars.py.
if exists('b:current_syntax')
  finish
endif

syn keyword voKeyword fn record variant test prop import export let var set new requires ensures
syn keyword voConditional if else match case
syn keyword voRepeat while for in break continue
syn keyword voStatement return try
syn keyword voOperator and or not
syn keyword voBoolean true false
syn keyword voType int float bool str bytes list map result void

" words that are part of the language only in one position
syn match voType /\<\(ordered_map\|ordered_set\|priority_queue\|deque\)\ze\s*\[/
syn match voKeyword /\<\(const\)\ze\s\+\h\w*\s*[:=]/
syn match voKeyword /\<\(effects\)\ze\s\+\u/
syn match voConstraint /\(\h\w*\s*:\s*\)\@<=\(ordered\|effects\|json\|key\)\ze\s*[,\]]/

syn match voTypeName /\<\u\w*/
syn match voFunction /\(\<fn\s\+\)\@<=\l\w*/
syn match voFloat /\<\d\+\(\.\d\+\([eE][+-]\=\d\+\)\=\|[eE][+-]\=\d\+\)\>/
syn match voNumber /\<\d\+\>/
syn match voNumber /\<0x\x\+\>\|\<0b[01]\+\>/
syn match voAnnotation /@\l\w*/

syn match voEscape /\\\([ntr"\\]\|u{\x\{1,6}}\)/ contained
syn region voString start=/"/ skip=/\\\\\|\\"/ end=/"\|$/ oneline contains=voEscape
" a raw string: any text between backticks, over any number of lines
syn region voString start=/`/ end=/`/

" the effects after `uses`, on its line
syn match voEffect /\<\(console\|input\|file\.read\|file\.write\|file\.delete\|net\|clock\|random\|env\|serve\|process\|entropy\)\>/ contained
syn region voUses matchgroup=voKeyword start=/\<uses\>/ end=/\ze[{;=\])]\|$/ oneline contains=voEffect,voString,voTypeName,voNumber

syn match voComment /#.*$/ contains=@Spell

hi def link voKeyword Keyword
hi def link voConditional Conditional
hi def link voRepeat Repeat
hi def link voStatement Statement
hi def link voOperator Operator
hi def link voBoolean Boolean
hi def link voType Type
hi def link voTypeName Type
hi def link voConstraint StorageClass
hi def link voFunction Function
hi def link voFloat Float
hi def link voNumber Number
hi def link voAnnotation PreProc
hi def link voEscape SpecialChar
hi def link voString String
hi def link voEffect Constant
hi def link voComment Comment

let b:current_syntax = 'vo'
