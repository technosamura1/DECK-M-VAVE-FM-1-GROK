#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# DECK's tests: the sound alone (every preset's WAV checked, OVERWORLD compared, the CPU), then the whole app in the
# simulator (the scenarios). Needs ./build.sh TAPE --host first.   sh tests/run_tests.sh [TAPE]
cd "$(dirname "$0")/.." || exit 1
TAPE="${1:-overworld}"
fail=0
[ -d ../../w64devkit/bin ] && PATH="$(cd ../.. && pwd)/w64devkit/bin:$PATH"
CC="${CC:-gcc}"
mkdir -p build/test out
FLAGS="-O2 -std=c99 -ffp-contract=off -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wdouble-promotion"
case "$($CC -dumpmachine)" in i686*|i386*) FLAGS="$FLAGS -msse2 -mfpmath=sse" ;; esac
$CC $FLAGS -DDK_HOST -Ibase/include -Ideck -Itapes/$TAPE -o build/test/deck_test tests/host/deck_test.c deck/core.c \
    deck/song.c deck/dsp_unit.c -lm || exit 1
echo "-- the sound"
rm -rf out/sounds
build/test/deck_test sounds out/sounds > build/test/sounds.txt || fail=1
python tools/sound_check.py out/sounds --json build/test/check.json > build/test/sound_check.txt && echo "  ok   every preset: no clicks, peaks, DC, tails, levels within 2 dB" || { echo "  FAIL sound check"; grep "<--" build/test/sound_check.txt | head; fail=1; }
python tools/finger_lint.py > build/test/finger_lint.txt && echo "  ok   FINGER banks: patterns well-formed, no duplicates" || { echo "  FAIL finger lint"; grep LINT build/test/finger_lint.txt | head; fail=1; }
rm -rf out/fingerbanks
build/test/deck_test fingerbank out/fingerbanks > build/test/fingerbank.txt || { echo "  FAIL fingerbank (silent or error)"; grep SILENT build/test/fingerbank.txt | head -5; fail=1; }
python tools/sound_check.py out/fingerbanks --nolevel > build/test/fingerbank_check.txt && echo "  ok   every FINGER bank pattern: no clicks, peaks, DC, tails" || { echo "  FAIL fingerbank sound check"; grep "<--" build/test/fingerbank_check.txt | head; fail=1; }
build/test/deck_test chords out/chords.wav > /dev/null || fail=1
build/test/deck_test bypass > build/test/bypass.txt && echo "  ok   SOUND bypass: every stage of the chain off and on again: no clicks, peak under -1 dBFS" || { echo "  FAIL bypass"; grep CLICK build/test/bypass.txt | head -5; fail=1; }
build/test/deck_test metro | sed 's/^/  /'
build/test/deck_test bench | sed 's/^/  /'
echo "-- the scenarios"
for t in tests/scenarios/*.dk; do
  n=$(basename "$t" .dk)
  mkdir -p build/sc/$n
  if (cd deck/build/host && ./deck_host ../../../$t ../../../build/sc/$n) > build/sc/$n/log.txt 2>&1; then echo "  ok   $n"; else echo "  FAIL $n"; grep FAIL build/sc/$n/log.txt | head -5; fail=1; fi
done
if [ -f build/sc/export/demo.syx ]; then
  python tools/deck_export.py --syx build/sc/export/demo.syx out/first_light.mid > build/sc/export/mid.txt && echo "  ok   export: $(tail -1 build/sc/export/mid.txt | cut -c1-90)" || { echo "  FAIL export"; fail=1; }
fi
[ $fail = 0 ] && echo "ALL OK" || echo "TESTS FAILED"
exit $fail
