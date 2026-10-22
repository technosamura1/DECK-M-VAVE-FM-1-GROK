#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# DECK with a tape:  ./build.sh TAPE [--host] [--release X.Y]     (TAPE: overworld)
#   the firmware: deck/build/deck.fwsc (copied to out/deck-TAPE.fwsc); --host: the simulator deck/build/host/deck_host
# On this PC: JieLi's Windows toolchain (../../jlwin), the SDK (../../jieli/sdk), w64devkit's gcc for the host.
set -e
cd "$(dirname "$0")"
TAPE="${1:-overworld}"
[ $# -gt 0 ] && shift
[ -d "tapes/$TAPE" ] || { echo "no tape: tapes/$TAPE"; exit 1; }
sed "s/@TAPE@/$TAPE/g" deck/fm1.toml.in > deck/fm1.toml
FM1="$(cd ../.. && (pwd -W 2>/dev/null || pwd))"
export PYTHONUTF8=1
export JIELI_WIN="${JIELI_WIN:-$FM1/jlwin/x/C\$/JL}"
export AC79_SDK="${AC79_SDK:-$FM1/jieli/sdk}"
export X0X_FONTSET=terminus
export PYTHONPATH="$FM1/scripts/sitepatch"
case " $* " in
*" --host "*) export PATH="$(cd ../.. && pwd)/w64devkit/bin:$PATH" CC="${CC:-gcc}" ;;
esac
python base/tools/build.py deck "$@"
case " $* " in
*" --host "*) ;;
*) mkdir -p out && cp deck/build/deck.fwsc "out/deck-$TAPE.fwsc" && echo "firmware out/deck-$TAPE.fwsc" ;;
esac
