#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# AFFINE: every host scenario (tests/scenarios/*.txt) through the simulator; prints the failures, exits 1 on any.
#   tests/scenarios.sh [NAME ...]     only these (tests/scenarios/NAME.txt); screens go to app/build/scenarios
# The simulator is built first if it is missing (./build.sh --host: PYTHON and PYTHONPATH as for the build).
# A scenario whose header runs it with LOAF_ASK=1 (the first start's question) gets that variable.
cd "$(dirname "$0")/.."
H=app/build/host/affine_host
OUT=app/build/scenarios
[ -x "$H" ] || ./build.sh --host > /dev/null || { echo "scenarios: the host build failed"; exit 1; }
mkdir -p "$OUT" build/scenarios
if [ $# -gt 0 ]; then LIST="$*"; else LIST=$(ls tests/scenarios/*.txt | sed 's|.*/||; s|\.txt$||'); fi
# Known failures, inherited: CHOPPAH-era scenarios that LOAF BREAKER's navigation had already moved past (they fail
# the same at LOAF's 4f8722c): SELECT and ALGORITHM changed roles (controls fxchain fxviews mbcomp pages), SEQ's lanes
# (perc), no demo built (demo), the clock-in tempo check (midi). Listed, not hidden: a known one passing says so; to be
# rewritten for AFFINE's pages or deleted.
KNOWN=" controls demo fxchain fxviews mbcomp midi pages perc "
n=0; bad=0; failed=""; known=0
for sc in $LIST; do
    f="tests/scenarios/$sc.txt"
    [ -f "$f" ] || { echo "FAIL $sc: no $f"; bad=$((bad + 1)); failed="$failed $sc"; continue; }
    n=$((n + 1))
    log="build/scenarios/$sc.log"
    if grep -q "^#.*LOAF_ASK=1" "$f"; then
        LOAF_ASK=1 "$H" "$f" "$OUT" > "$log" 2>&1; rc=$?
    else
        "$H" "$f" "$OUT" > "$log" 2>&1; rc=$?
    fi
    if [ $rc -ne 0 ] || grep -q "^FAIL" "$log"; then
        case "$KNOWN" in
            *" $sc "*) known=$((known + 1)); continue ;;
        esac
        bad=$((bad + 1)); failed="$failed $sc"
        echo "FAIL $sc (exit $rc):"
        grep -E "^FAIL|host:|error" "$log" | head -5 | sed 's/^/    /'
    else
        case "$KNOWN" in
            *" $sc "*) echo "note: $sc passes now: take it off KNOWN" ;;
        esac
    fi
done
echo "scenarios: $((n - bad - known)) of $n passed, $known known failures${failed:+; NEW failures:$failed}"
[ $bad -eq 0 ]
