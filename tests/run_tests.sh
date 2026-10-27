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
python tools/dup_check.py out/sounds > build/test/dup_check.txt && echo "  ok   no two presets sound the same (spectrum + motion + parameters)" || { echo "  FAIL duplicate presets"; head -8 build/test/dup_check.txt; fail=1; }
build/test/deck_test chords out/chords.wav > /dev/null || fail=1
build/test/deck_test latency > build/test/latency.txt; n=$(sed -n 's/.* \([0-9]*\) slow.*/\1/p' build/test/latency.txt)
[ "${n:-99}" -le 13 ] && echo "  ok   LATENCY: key to sound (every preset): $(tail -1 build/test/latency.txt)" || { echo "  FAIL latency: $n slow presets"; grep SLOW build/test/latency.txt | grep -v AIRPAD; fail=1; }
build/test/deck_test bypass > build/test/bypass.txt && echo "  ok   SOUND bypass: every stage of the chain off and on again: no clicks, peak under -1 dBFS" || { echo "  FAIL bypass"; grep CLICK build/test/bypass.txt | head -5; fail=1; }
build/test/deck_test automation > build/test/automation.txt && echo "  ok   AUTOMATION: a curve on every target, three engines: no click, peak under -1 dBFS, no DC, not silent" || { echo "  FAIL automation"; grep "BAD\|SILENT" build/test/automation.txt | head -5; fail=1; }
build/test/deck_test pitch > build/test/pitch.txt && echo "  ok   PITCH: every BASS preset sounds C2, G2, C3 at its pitch (or a sub-octave by design), none above it" || { echo "  FAIL pitch"; grep BAD build/test/pitch.txt | head -5; fail=1; }
build/test/deck_test fxlong > build/test/fxlong.txt && echo "  ok   FX LONG: the effects' delay lines keep their fine position past 2^24 samples (7 minutes of sound)" || { echo "  FAIL fxlong"; grep BAD build/test/fxlong.txt | head -5; fail=1; }
build/test/deck_test metro | sed 's/^/  /'
build/test/deck_test stress24 > build/test/stress24.txt && echo "  ok   24 voices at once (MOURNING x3 + heavy pads, the FX chain on every track): the pool fills, peak under full scale, no DC" || { echo "  FAIL stress24"; grep FAIL build/test/stress24.txt | head -3; fail=1; }
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
