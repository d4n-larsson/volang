#!/bin/sh
# `make quick [AREA=a,b]`: the tests of the areas a change touches, for the steps between edits.
# `make test` (everything) is still what completes a change (constitution, quality gates).
#
# With no AREA, the areas come from the files changed since the last commit (git diff, staged and
# not, and new files). A file this table does not know, or one every area depends on (vo.h, the VM,
# the runtime core, the Makefile), selects `all`: the whole of `make test`. Each step prints its time.
# VO_BIN (set by `make ARCH=...`) selects the build, as for every suite.
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1

# area: case directories (golden and step cases) | scripts (under tests/, with arguments)
cases() {
    case $1 in
    math) echo "math doc" ;;
    format) echo "format check lex doc" ;;
    regex) echo "regex lex doc" ;;
    sqlite) echo "sqlite sqlite_steps generic fmt lex doc" ;;
    structure) echo "structure test test_steps check" ;;
    serve_workers) echo "serve serve_concurrent serve_workers_steps replay" ;;
    web) echo "json_derive annotations web web_steps generic json" ;;
    effects) echo "effect_sets effects fmt" ;;
    const) echo "const check effects effect_sets fmt query annotations format regex sqlite web doc" ;; # spec 031
    typed_errors) echo "typed_errors check generic prop tasks test run web doc" ;; # spec 032
    map_keys) echo "map_keys check bytes generic prop tasks test fmt doc" ;; # spec 033
    build) echo "build cli run replay packages" ;; # spec 034
    crypto) echo "crypto effects effect_sets replay tasks doc" ;; # spec 035
    mcp) echo "cli query" ;; # spec 036
    grammars) echo "lex" ;; # spec 037
    lsp) echo "cli query edit fmt" ;; # spec 038
    bytes) echo "bytes bytes_steps run doc" ;;
    float) echo "math json run text doc" ;;
    json) echo "json doc" ;;
    text) echo "text doc" ;;
    time) echo "time sleep_steps doc" ;;
    random) echo "random doc" ;;
    collections) echo "collections ordered doc" ;;
    http) echo "http https https_steps serve serve_concurrent replay security" ;;
    fs) echo "fs fs_steps process process_steps security" ;;
    front) echo "lex parse check effects func generic multi fmt hole contract prop spec" ;;
    query) echo "query edit" ;;
    testing) echo "test test_steps coverage mutate spec" ;;
    tasks) echo "tasks tasks_steps" ;;
    trace) echo "trace replay" ;;
    packages) echo "packages multi" ;;
    docs) echo "doc" ;;
    cli) echo "cli run" ;;
    esac
}
scripts() { # one per line, with arguments
    case $1 in
    math) echo "math_vectors.py" ;;
    format) printf '%s\n' apply_fixes.py catalog.py ref_examples.py "format_diff.py --quick" ;;
    regex) printf '%s\n' apply_fixes.py catalog.py doc_conformance.py "regex_diff.py --quick" ;;
    sqlite) printf '%s\n' apply_fixes.py catalog.py doc_conformance.py "sqlite_scope.py --quick" "sqlite_fuzz.py --count 1000" ;;
    structure) printf '%s\n' apply_fixes.py catalog.py structure_corpus.py ;;
    serve_workers) printf '%s\n' doc_conformance.py "serve_workers.py --quick" ;;
    web) printf '%s\n' doc_conformance.py "web_fuzz.py --quick" ;;
    effects) printf '%s\n' fmt_corpus.py catalog.py effect_sets_records.py ;;
    const) printf '%s\n' apply_fixes.py catalog.py fmt_corpus.py ref_examples.py const_equiv.py ;; # spec 031
    typed_errors) printf '%s\n' catalog.py apply_fixes.py ref_examples.py doc_conformance.py ;; # spec 032
    map_keys) printf '%s\n' catalog.py apply_fixes.py ref_examples.py doc_conformance.py ;; # spec 033
    build) printf '%s\n' catalog.py ref_examples.py build_examples.py ;; # spec 034
    crypto) printf '%s\n' catalog.py ref_examples.py doc_conformance.py crypto_vectors.py ;; # spec 035
    mcp) printf '%s\n' catalog.py ref_examples.py mcp.py ;; # spec 036
    grammars) echo grammars.py ;; # spec 037
    lsp) printf '%s\n' lsp.py mcp.py ;; # spec 038
    bytes) echo doc_conformance.py ;;
    float) printf '%s\n' math_vectors.py json_corpus.py json_stream.py ref_examples.py doc_conformance.py ;;
    json) printf '%s\n' json_corpus.py json_stream.py doc_conformance.py ;;
    time) printf '%s\n' time_check.py doc_conformance.py ;;
    text|random|collections) echo doc_conformance.py ;;
    http) echo https_interop.py ;;
    front) printf '%s\n' apply_fixes.py catalog.py fmt_corpus.py "fuzz.py --iterations 50 --seed 1" ;;
    packages) echo packages_check.py ;;
    docs) printf '%s\n' ref_examples.py site_examples.py doc_conformance.py catalog.py ;;
    esac
}
# extra make targets
targets() {
    case $1 in
    math) echo "math-selftest cm-noasm" ;;
    http) echo "tls-selftest" ;;
    esac
}

# The area of one changed file; "all" when every area depends on it or it is unknown; "" for none.
area_of() {
    case $1 in
    src/math.c|src/math_libm.c|std/math.vo|third_party/core-math/*|third_party/shim/fenv.h|third_party/shim/math.h|third_party/shim/errno.h|tests/math/*|tests/math_*.py|tools/math_vectors_gen.py|tools/vendor_core_math.sh|tools/cm_noasm.awk) echo math ;;
    src/check_format.c|tests/format_diff.py) echo format ;; # spec 024 (before src/check*.c)
    src/regex.c|src/check_regex.c|std/regex.vo|tests/regex_*.py) echo regex ;; # spec 025
    src/sqlite_*.c|src/check_sqlite.c|std/sqlite.vo|third_party/sqlite/*|tests/sqlite_*.py) echo sqlite ;; # spec 026
    src/check_structure.c|tests/structure_*.py|tests/structure_seeds/*) echo structure ;; # spec 027
    src/serve.c|src/serve_workers.c|std/http.vo|tests/serve_workers*.py) echo serve_workers ;; # spec 028
    src/json_derive.c|src/check_web.c|std/web.vo|std/json.vo|tests/web_*.py) echo web ;; # spec 029
    src/check_const.c|tests/const_*.py) echo const ;; # spec 031
    src/rt_map.c|tests/map_keys_bench.py) echo map_keys ;; # spec 033
    src/build.c|tests/build_examples.py) echo build ;; # spec 034
    src/crypto.c|src/sha256.c|std/crypto.vo|tests/crypto_vectors.py) echo crypto ;; # spec 035
    src/mcp.c|tests/mcp.py) echo mcp lsp ;; # spec 036 (the JSON reader is shared with vo lsp)
    src/lsp.c|src/jsonrpc.c|tests/lsp.py) echo lsp ;; # spec 038
    editors/*|tests/grammars.py|site/pygments_vo/pygments_vo.py) echo grammars ;; # spec 037
    src/check_effects.c|src/fmt.c|tests/effect_sets_*.py) echo effects ;; # spec 030
    src/num.c|src/stdprim.c) echo float ;;
    src/bytes.c|std/bytes.vo|tests/bytes/*|tests/bytes_*.py|tests/process_helpers/allbytes) echo bytes ;;
    std/json.vo) echo json ;;
    std/text.vo|src/text.c) echo text ;;
    std/time.vo|tests/sleep_check.py) echo time ;;
    std/random.vo) echo random ;;
    std/collections.vo|src/rt_coll.c) echo collections ;;
    third_party/shim/*) echo "math http" ;; # both vendored libraries
    std/http.vo|src/net.c|src/serve.c|src/tls.c|src/tls_libc.c|src/tls_*.h|src/replay.c|third_party/mbedtls/*|tests/tls/*|tests/http*.py|tests/https*.py|tests/tls*.py) echo http ;;
    std/fs.vo|std/process.vo|src/fs.c|src/process.c|tests/process_helpers/*) echo fs ;;
    src/lex.c|src/parse.c|src/ast.c|src/check*.c|src/types.c|src/diag.c|src/fmt.c|src/effects*.c|src/loader.c) echo front ;;
    src/query.c|src/edit.c) echo query ;;
    src/testrun.c|src/rt_test.c|src/mutate.c|src/prop.c) echo testing ;;
    src/task.c) echo tasks ;;
    src/packages.c|src/sha256.c) echo packages ;;
    src/doc.c|docs/*|site/*|tools/site.py|tools/embed.py) echo docs ;;
    tests/cases/*) rel=${1#tests/cases/}; d=${rel%%/*}; echo "dir:$d" ;;
    specs/*|README.md|.specify/*|sesi-linter-issues.md|examples/*|bench*/*|.claude/*) echo "" ;;
    *) echo all ;;
    esac
}

which=0
if [ "${1:-}" = --which ]; then which=1; shift; fi  # print the areas and stop
areas=${1:-}
if [ -z "$areas" ]; then
    changed=${QUICK_FILES:-$( (git diff --name-only HEAD; git ls-files --others --exclude-standard) | sort -u)}
    for f in $changed; do
        a=$(area_of "$f")
        [ -n "$a" ] && areas="$areas $a"
    done
    areas=$(echo $areas | tr ' ' '\n' | sort -u | tr '\n' ' ')
    if [ -z "$(echo $areas)" ]; then
        echo "quick: no tested files changed since the last commit (the build is up to date)"
        exit 0
    fi
    echo "quick: changed: $(echo $changed | tr '\n' ' ')"
fi
areas=$(echo "$areas" | tr ',' ' ')
if [ $which = 1 ]; then echo "quick: areas: $areas"; exit 0; fi
case " $areas " in
*" all "*) echo "quick: areas: all (a shared file changed): make test"; exec make -s --no-print-directory test ;;
esac
echo "quick: areas: $areas"

dirs="" ; scr="" ; tgt=""
for a in $areas; do
    case $a in
    dir:*) dirs="$dirs ${a#dir:}"; continue ;;
    esac
    c=$(cases "$a")
    if [ -z "$c" ]; then echo "quick: unknown area $a (known: math format regex sqlite structure serve_workers web effects const typed_errors map_keys build crypto bytes float json text time random collections http fs front query testing tasks trace packages docs cli)"; exit 2; fi
    dirs="$dirs $c"; scr="$scr
$(scripts "$a")"; tgt="$tgt $(targets "$a")"
done
dirs=$(echo $dirs | tr ' ' '\n' | sort -u | tr '\n' ' ')
scr=$(printf '%s\n' "$scr" | grep . | sort -u)
tgt=$(echo $tgt | tr ' ' '\n' | sort -u | tr '\n' ' ')

fail=0
step() { # name, command...
    name=$1; shift
    t0=$(date +%s%N)
    "$@" >"$root/build/quick.log" 2>&1; st=$?
    ms=$(( ($(date +%s%N) - t0) / 1000000 ))
    summary=$(grep -E '^(passed|[a-z_]+: |FAIL)' "$root/build/quick.log" | tail -1)
    if [ $st = 0 ]; then echo "  ok   $name (${ms} ms) $summary"
    else echo "  FAIL $name (${ms} ms)"; grep -E 'FAIL|Error|error' "$root/build/quick.log" | head -20; fail=1; fi
}
paths=""; for d in $dirs; do [ -d tests/cases/$d ] && paths="$paths tests/cases/$d"; done
[ -n "$paths" ] && step "golden cases:$dirs" tests/run.sh $paths
steps_paths=""; for p in $paths; do [ -n "$(find $p -name steps -print -quit)" ] && steps_paths="$steps_paths $p"; done
[ -n "$steps_paths" ] && step "step cases" python3 tests/steps.py $steps_paths
printf '%s\n' "$scr" | grep . > "$root/build/quick.scripts"
while IFS= read -r s; do
    [ -f "tests/${s%% *}" ] || { echo "  skip $s (not created yet)"; continue; }
    step "$s" sh -c "python3 tests/$s" </dev/null
done < "$root/build/quick.scripts"
for t in $tgt; do step "make $t" make -s --no-print-directory $t; done
[ $fail = 0 ] && echo "quick: passed (make test before the change is complete)" || echo "quick: FAILED"
exit $fail
