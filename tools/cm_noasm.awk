# Spec 020 (`make cm-noasm`): reads a vendored C file's preprocessed text and fails when a line that
# comes from third_party/core-math/ contains an asm statement. Line markers (# N "file") say which
# file the following lines come from. Set f to the source file's name, for the message.
/^# [0-9]+ "/ { own = index($3, "third_party/core-math/") > 0; next }
own && /(^|[^A-Za-z0-9_])(asm|__asm__|__asm)[ \t]*[(]/ { print f ": asm in vendored code: " $0; bad = 1 }
END { exit bad }
