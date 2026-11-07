"""Check the curated patches and mixes; keep noise-spectrum heuristics separate.

The preset check uses the existing engine-specific thresholds unchanged.
The mix check tests discontinuities/DC/peak/tails, not aliasing: a drum mix's
upper-octave energy cannot distinguish intended noise from oscillator aliasing.
"""
import json
from pathlib import Path
from sound_check import check

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "out"
ENG = ["PIANO","KEYS","PAD","GLASS","BASS","DRUMKIT","WORLD","KALIMBA","WINDS","SYNTH","PLUCK","LEAD"]
manifest=json.loads((OUT/"style-presets.json").read_text(encoding="utf8"))
all_checks=json.loads((ROOT/"build/test/style-check.json").read_text(encoding="utf8"))
result={"hardware_tested":False,"presets":[],"previews":[],"comparison":[]}
errors=[]
for p in manifest:
    name=f"b{p['bank']}_{p['slot']+1:02}_{p['name'].replace(' ','_')}.wav"
    records=[r for r in all_checks[ENG[p['engine']]] if Path(r['file']).name==name]
    assert len(records)==1,name
    r=records[0]
    result['presets'].append(r)
    if r['problems'] or abs(r['dev_db'])>2:
        errors.append((name,r['problems'],r['dev_db']))
for style in ('ambient','synthwave','8bit'):
    for path in sorted((OUT/'styles'/style).glob('*.wav')):
        r=check(str(path))
        result['previews'].append(r)
        # Same safety thresholds as sound_check, excluding its spectral heuristic.
        if r['peak_db']>-1 or r['dc_db']>-54 or r['tail_db']>-60 or r['clicks']:
            errors.append((str(path),'peak/DC/tail/click check'))
    before=check(str(OUT/'styles-before'/style/'demo.wav'))
    after=check(str(OUT/'styles'/style/'demo.wav'))
    result['comparison'].append(dict(style=style,before=before,after=after))
result['errors']=errors
result['note']='Upper-octave energy in drum mixes is informational, not proof of aliasing. See canonical per-engine preset checks.'
(OUT/'style-validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print(f"{len(result['presets'])} selected presets; {len(result['previews'])} previews; {len(errors)} failures")
for e in errors: print(e)
raise SystemExit(bool(errors))
