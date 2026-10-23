#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""out/listen.html: every sound to listen to (CLAUDE_TASK 9.5): the demo song, each engine's presets, OVERWORLD's old
sound beside the new, the DRIVE styles, the moods, the chords. Plain HTML, the WAVs beside it.
  python tools/make_listen.py out"""
import html
import os
import sys
import wave

out = sys.argv[1] if len(sys.argv) > 1 else "out"
ABOUT = {
    "FELT": "пианино с войлоком — главный инструмент (FM-пиано OVERWORLD + мягкая атака, срез верха, стук клавиши)",
    "TINE": "лоу-фай электропиано (VENUS из OVERWORLD) с тремоло и «плаванием»",
    "AIRPAD": "тёплые пады OVERWORLD: SWEDEN, AIR, WARM, SHIMMER, STRINGS",
    "GLASS": "колокольчики: стекло, челеста, маримба, музыкальная шкатулка, вибрафон",
    "SOFTBASS": "мягкий бас: SUB, UPRIGHT (контрабас), MOOG",
    "DUSTKIT": "пыльные лоу-фай барабаны: бочка, щётка, римшот, шейкер, хет, клэп, дерево",
    "WORLD": "звуки мира: дождь, ветер, костёр, ручей, капли, сверчки, птицы, океан, гроза, снег",
    "KALIMBA": "калимба: язычок, щипок пальцем, деревянный корпус",
    "BEATKIT": "сэмпловые барабаны: киты в духе 707, 808, 909, CR-78, пыльный бум-бэп, кассета (TUNE DECAY TONE CRUSH KICK)",
    "FM": "четыре оператора: DX-пиано, лоу-фай EP, бас, колокола, брасс, орган, клавинет, кото, пэд, лид",
    "PLUCK": "щипковые струны (Karplus-Strong): нейлон, сталь, арфа, кото, бас, глушёная гитара",
}
ORDER = ["FELT", "TINE", "AIRPAD", "GLASS", "KALIMBA", "SOFTBASS", "DUSTKIT", "WORLD", "BEATKIT", "FM", "PLUCK"]
MOODS = ["PLAINS", "RAIN", "CAVE", "SNOW", "OCEAN", "NIGHT", "CAMPFIRE", "STUDY", "BEATS", "SUNSET"]
STYLES = ["C418", "LOFI", "AMBIENT", "CAMPFIRE"]
COMPARE = ["Пианино (FELT)", "Пад (AIRPAD SWEDEN)", "Колокол (GLASS CELESTA)", "Бас (SOFTBASS SUB)"]


def secs(path):
    try:
        with wave.open(path) as w:
            return w.getnframes() / w.getframerate()
    except Exception:
        return 0.0


def row(rel, label, sub=""):
    p = os.path.join(out, rel)
    if not os.path.exists(p):
        return ""
    return (f'<div class="row"><button onclick="play(this,\'{html.escape(rel)}\')">&#9654;</button>'
            f'<span class="nm">{html.escape(label)}</span><span class="sub">{html.escape(sub)}</span>'
            f'<span class="t">{secs(p):.1f} с</span></div>\n')


parts = []
parts.append("<h2>Демо-песня</h2>")
parts.append(row("demo_first_light.wav", "FIRST LIGHT", "INTRO · PRE · VERSE · CHORUS · BRIDGE · BREAK · CHORUS · OUTRO"))
parts.append(row("chords.wav", "Аккорды на белых клавишах", "мажор · минор · лидийский, голосоведение, страм"))
parts.append("<h2>Движки и пресеты</h2><p class='dim'>Каждый пресет: одна нота, аккорд, короткая фраза.</p>")
for eng in ORDER:
    d = os.path.join(out, "sounds", eng)
    if not os.path.isdir(d):
        continue
    parts.append(f'<details open><summary><b>{eng}</b> <span class="dim">— {ABOUT.get(eng, "")}</span></summary>')
    for f in sorted(os.listdir(d)):
        if f.endswith(".wav"):
            parts.append(row(f"sounds/{eng}/{f}", f[3:-4].replace("_", " "), f"пресет {int(f[:2])}"))
    parts.append("</details>")
parts.append("<h2>OVERWORLD: было → стало</h2><p class='dim'>Одна и та же фраза: старый звук прошивки OVERWORLD и новый движок DECK.</p>")
for i, n in enumerate(COMPARE):
    parts.append(f"<div class='pair'><b>{n}</b>" + row(f"compare/ow_{i}.wav", "было (OVERWORLD)") + row(f"compare/deck_{i}.wav", "стало (DECK)") + "</div>")
parts.append("<h2>DREAM — автопилот (DRIVE)</h2><p class='dim'>Все шесть дорожек в DRIVE, разные стили.</p>")
for i, s in enumerate(STYLES):
    parts.append(row(f"drive/style_{i}.wav", s))
parts.append("<h2>MOOD-ы (биомы)</h2>")
for i, s in enumerate(MOODS):
    parts.append(row(f"moods/mood_{i}.wav", s))

page = f"""<!doctype html>
<html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>DECK · OVERWORLD — звуки</title>
<style>
:root {{ --bg:#000; --fg:#ece8dc; --dim:#6a6a6a; --line:#2a2a2a; --a:#7ccc60; --b:#6ebaf4; --c:#f6d65c; }}
body {{ margin:0; background:var(--bg); color:var(--fg); font:15px/1.5 ui-monospace, Menlo, Consolas, monospace; }}
main {{ max-width:760px; margin:0 auto; padding:28px 16px 80px; }}
h1 {{ font-weight:300; letter-spacing:6px; margin:0; }} h2 {{ font-weight:400; color:var(--a); margin:34px 0 8px; }}
.dim {{ color:var(--dim); }} details {{ margin:10px 0; }} summary {{ cursor:pointer; padding:4px 0; }}
.row {{ display:flex; align-items:center; gap:12px; padding:5px 0; border-bottom:1px solid var(--line); }}
.row button {{ width:34px; height:28px; background:none; color:var(--fg); border:1px solid var(--dim); cursor:pointer; }}
.row button.on {{ color:var(--bg); background:var(--b); border-color:var(--b); }}
.nm {{ min-width:180px; }} .sub {{ color:var(--dim); flex:1; }} .t {{ color:var(--dim); }}
.pair {{ margin:12px 0; }} #now {{ position:fixed; left:0; right:0; bottom:0; background:#111; padding:8px 16px; }}
audio {{ width:100%; }}
</style></head><body><main>
<h1>DECK · OVERWORLD</h1>
<div class="dim">Лоу-фай эмбиент в духе C418. Нажми ▶ — звук играет внизу. Все файлы — 44.1 кГц, из симулятора прошивки.</div>
{''.join(parts)}
</main>
<div id="now"><audio id="au" controls></audio></div>
<script>
let cur = null;
function play(b, f) {{
  const a = document.getElementById('au');
  if (cur) cur.classList.remove('on');
  if (cur === b && !a.paused) {{ a.pause(); cur = null; return; }}
  a.src = f; a.play(); b.classList.add('on'); cur = b;
}}
document.getElementById('au').onended = () => {{ if (cur) cur.classList.remove('on'); cur = null; }};
</script></body></html>"""
open(os.path.join(out, "listen.html"), "w", encoding="utf-8").write(page)
print(os.path.join(out, "listen.html"))
