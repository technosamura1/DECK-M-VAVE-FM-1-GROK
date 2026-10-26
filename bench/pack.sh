#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# bench/pack.sh NAME [OUTDIR]: the folder to hand to the player (firmware, bats, tool, instruction) and its zip.
# Run after:  python3 base/tools/build.py bench
set -e
cd "$(dirname "$0")/.."
NAME="${1:?name}"
OUT="${2:-dist-bench}"
rm -rf "$OUT/$NAME"
mkdir -p "$OUT/$NAME"
cp bench/build/bench.fwsc "$OUT/$NAME/bench.fwsc"
cp out/fm1_install.py tools/deck_bench.py bench/pack/install-bench.bat bench/pack/bench-deck.bat "$OUT/$NAME/"
cp "bench/pack/ИНСТРУКЦИЯ.txt" "$OUT/$NAME/"
(cd "$OUT" && rm -f "$NAME.zip" && zip -qr -X "$NAME.zip" "$NAME")
(cd "$OUT" && sha256sum "$NAME.zip" > "$NAME.zip.sha256")
cat "$OUT/$NAME.zip.sha256"
