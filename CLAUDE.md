# DECK — прошивка-«операционка» для M-VAVE FM-1, кассета OVERWORLD

DECK — «свой OP-1» на FM-1: 6 дорожек, 8 частей A–H, трасса-аранжировка, умная клавиатура (лад/аккорды), 8 типов
секвенсора, микшер, экспорт в .mid. Звук и картинка — из сменной кассеты; первая собранная кассета — **OVERWORLD**
(лоу-фай эмбиент в духе C418: пианино с войлоком, пады, колокольчики, пыльные барабаны, звуки мира, сад на экране).
ТЗ: `docs/DECK_SPEC.md`, `docs/OVERWORLD_TAPE.md`, порядок работы — `docs/CLAUDE_TASK.md` (копии из `..`; макеты `docs/ui_concepts`).
Репозиторий на GitHub. Прошивку `.fwsc` собирает **GitHub Actions** (`.github/workflows/firmware.yml`, Linux-тулчейн JieLi) на каждый push/PR → Artifacts; push в `main` → релиз `latest` с `deck-<кассета>.zip` (fwsc + install-deck.bat + fm1_install.py + инструкция). Сборка под Linux побайтно совпадает с домашней. Облачная сессия (телефон) может править код, гонять симулятор/тесты и ждать зелёную сборку в PR.
Пользователь — Даниил, по-русски, коротко; показывать звук и картинки, не код.

## Сборка и тесты
- Linux/облако: `JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/.jieli/ac79-sdk sh build.sh overworld` (тулчейн: `sh base/tools/get_toolchain.sh`, SDK: `sh base/bsp/tools/get_sdk.sh ~/.jieli/ac79-sdk`).

### На домашнем ПК
- `./build.sh overworld` → `out/deck-overworld.fwsc` (тулчейн JieLi из `../../jlwin`, SDK `../../jieli/sdk`).
- `./build.sh overworld --host` → симулятор `deck/build/host/deck_host` (w64devkit gcc).
- `sh tests/run_tests.sh` → звук (все пресеты в `out/sounds`, проверка `tools/sound_check.py`), метроном, CPU,
  сценарии `tests/scenarios/*.dk`, экспорт → `out/first_light.mid` (проверка mido). Должно быть **ALL OK**.
- Звуковой стенд без UI: `build/mk.sh` → `build/test/deck_test sounds|demo|drive|mood|chords|phrase|balance|bench`.
- `python tools/make_listen.py out` → `out/listen.html` (все WAV + демо-песня).
- Последняя сборка: образ 253 КБ, RAM 90300 из 98304, пул 266392 из 344064.
- Хост-компилятор w64devkit — i686: стенд собирается с `-msse2 -mfpmath=sse` (x87 искажал замеры CPU в 5–8 раз).

## Устройство
```
base/                 fm1-base (не трогать)
deck/                 оболочка (APP-папка для base/tools/build.py; fm1.toml генерит build.sh из fm1.toml.in)
  deck_api.h          контракт оболочки и кассеты: движки, эффекты, пресеты, кассета dk_tape_t, песня dk_song_t
  core.c (+seq.c, music.c)   -O2: часы 96 PPQ, голоса (общий пул 24, кража тихих с затуханием 6 мс, бюджет CPU),
                      микшер, посылы (плейт Dattorro + шиммер, плёночное эхо), метроном, секвенсор, генераторы
  dsp_unit.c          -O2: включает tapes/<кассета>/tape_dsp.c
  app.c               unity: song.c store.c export.c keys.c fields.c ui_map.c tape_ui.c ui.c(+pages.c) host.c
  ui_map.c            ВСЯ раскладка кнопок в одной таблице (менять тут)
tapes/overworld/      кассета: eng_*.c (8 движков), fx.c, dream.c (DRIVE), tape_dsp.c (пресеты, MOOD-ы, рисунки
                      FINGER, прогрессии), demo.c (FIRST LIGHT), tape_ui.c + flower.c (сад, тема, кассета, сплэш)
tests/                deck_test.c (звук), scenarios/*.dk (симулятор), run_tests.sh
tools/                sound_check.py, calibrate.py, compare_ow.py, balance.py, make_listen.py, deck_export.py, sheet.py
web/export.html       экспорт из браузера (WebMIDI)
out/                  прошивка, установщик, WAV, listen.html, PNG, .mid
```
Потоки: `dk_render` — в аудио-прерывании; UI шлёт команды `dk_cmd`, а данные песни правит сам между `dk_lock()` /
`dk_unlock()` (рендер в это время откладывает только тики секвенсора, звук идёт). Запись: рендер кладёт ноты в
очередь `dk_recq`, UI вставляет их в клип (`rec_flush`) и делает снимок для undo на каждый проход.

## Что сделано (итерации 1–10)
- 6 дорожек, 8 частей (метка, длина 1–16 тактов), клипы разной длины (полиметрия), 4096 событий на проект.
- Секвенсоры: STEP (сетка барабанов / курсор для нот, VEL, PROB, RATCHET, p-lock удержанием шага), LIVE (квант 0–100 %,
  дозапись проходами), ENDLESS, ARP (6 режимов, 4 деления, октавы, LATCH), DRIVE (DREAM: стили C418/LOFI/AMBIENT/
  CAMPFIRE по ролям, «зов и ответ», дыхание раз в 8 тактов, сбивки), FINGER (16 рисунков, вариации на чёрных, LATCH),
  HOLD (бурдон), TOMBOLA (шестиугольник, шарики, притяжение к сетке). BAKE: DRIVE → STEP.
- Клавиатура: SCALE/CHROMA, белые = ноты/аккорды, чёрные = аккорды (7 ступеней + 4 «острых» кассеты)/PERFORM/OCTAVE,
  23 типа аккордов, DIATONIC/FIXED, 5 голосоведений, автоголосоведение, бас, страм, SPLIT, DIM SKIP.
- Трасса: до 64 блоков, повторы, трюки HALF/DOUBLE/REVERSE/±12/DROP/TAPESTOP, вставка/удаление/дубль/вырезать/
  вставить, режимы PART/SONG, запись трассы вживую (REC на странице ROAD), шаблоны EMPTY/POP/CLUB/AMBIENT, гараж частей.
- Метроном: REC/ALWAYS/OFF, счёт 1–2 такта (4…3…2…1 на кассете), 5 тембров (OVERWORLD — WOOD), деление, громкость,
  мимо ревера/эха/сайдчейна, не попадает в экспорт; HOME+REC; TAP TEMPO (HOME+PLAY).
- Звук OVERWORLD: FELT TINE AIRPAD GLASS KALIMBA SOFTBASS DUSTKIT WORLD × 12 пресетов + 4 своих слота;
  эффекты VINYL TAPE LOFI CHORUS FILTER TREMOLO SPRING; посылы ECHO/REVERB (ROOM HALL CAVE SHIMMER);
  мастер DUST/SLEEP + лимитер; сайдчейн пад ← бочка 15 %; модуляторы SWING DICE PEDAL FOLLOW TREMOLO; MACRO у пресета;
  8 MOOD-ов (= биомы сада); 11 перформанс-эффектов; демо-песня FIRST LIGHT (8 блоков, 4:41).
- Экран: минимализм OP-1 (чёрный фон, 3 цветных поля внизу, подсказка), страницы PLAY (сад OVERWORLD как есть),
  SEQ, SOUND (4 вкладки), MIX, ROAD (дорога с машинкой) + гараж, KEYS (аккорд крупно), PROJECT, REC (кассета с бобинами,
  намоткой, головкой, 6 лентами-дорожками). Все надписи влезают (аудит `textaudit`: 0 обрезанных).
- Хранение: 4 проекта в data-области (0xA0000, по 36 КБ; заголовок «DPRJ» пишется последним, FNV), автосохранение
  через 4 с тишины, настройки и свои пресеты — в store. Undo/redo: 24 снимка клипа/трассы.
- COM: EXPORT SysEx → `tools/deck_export.py` / `web/export.html` → .mid (6 дорожек с именами, маркеры частей, CC74/CC7);
  MIDI OUT по каналам 1–6 + clock/start/stop; MIDI IN каналы 1–6 (+10 = барабаны, CC64 педаль); STEMS (полный микс + 6 соло,
  щелчок-метка между проходами).

## Качество звука (CLAUDE_TASK 9) — как проверено
- `tools/sound_check.py`: щелчки, пик ≤ −1 dBFS, DC, «яркий верх/алиасинг», хвосты, громкость ±2 дБ внутри движка.
  Уровни пресетов выставлены `tools/calibrate.py` (трим в 0,5 дБ). Сейчас: ALL CLEAN.
- `tools/compare_ow.py`: та же фраза на OVERWORLD и на DECK (пиано/пад/колокол/бас) — «не хуже» по всем четырём.
- `tools/balance.py`: баланс демо по секциям (пианино впереди, бас не гудит) — BALANCED.
- Референсов в `../refs/` не было — подгонка под референсы не делалась.
- CPU (хост, SSE): демо в самом плотном месте ~29–31 мкс на блок 128 при худшем OVERWORLD ~32; бюджет голосов
  (`DK_BUDGET` в core.c) и `dk_shed()` при перегрузе гасят тихие хвосты, а не ноты.

## Добавлено 2026-10-23
- Шапка: ноты, которые играешь (C4 E4 G4, аккорд впереди; барабан — имя), ~1,5 с после нажатия (`notes_show` в ui.c).
- Батарея в шапке (`plat_battery()`: АЦП FM1_ADC_BATT опрашивается вместо MASTER каждый 50-й раз в fm1_input.h).
  Пределы `BAT_LO 520` / `BAT_HI 640` в pages.c — догадка; сырое значение видно на PROJECT («BAT n») — откалибровать
  по полной и почти пустой батарее.
- Смена тональности: тост «KEY D MAJOR», MOOD показывает и тональность; KEYS — тональность крупно, подписи нот на
  клавишах (в режиме аккордов — ступени 1–7), тоника отмечена жёлтым.
- Эффекты TAPE/CHORUS/TREMOLO/SPRING: LFO раз в блок с линейной интерполяцией (fx 3,8 → 2,9 мкс/блок на хосте).

## Добавлено 2026-10-23 (2)
- ARP удобнее: своя кнопка; 2-й ряд SEQ — RHYTHM (STRAIGHT PULSE OFFBEAT DOTTED GALLOP RISE SKIP), GATE (SHORT NORMAL
  LONG TIE), PLAYS (KEYS / CHORD — аккорд песни, когда клавиши не зажаты); деления 1/4…1/32 с триолями; экран —
  ритм-полоса, ноты на своей высоте, текущий шаг подсвечен (`dk_arp_rpos`).
- Чистка экранов: в шапке у аккорда только имя; MIX — номера дорожек в их цветах, имя выбранной сверху; PROJECT —
  без дублей, полоска заполнения, ячейки слотов; FINGER — рисунки паттернов вместо названий; MOD — цель по имени ручки.

## Не проверено на железе (ничего!)
Прошивка ни разу не запускалась на FM-1. Под вопросом: реальная нагрузка CPU, отклик клавиш/энкодеров, скорость
перерисовки по полосам 240×80, отправка SysEx-экспорта по USB (если не пойдёт — экспортировать через живой MIDI OUT +
запись в Reaper), стирание флеша при автосохранении (звук на миг замолкает — сохраняем только в тишине).

## Кнопки (из `deck/ui_map.c`)
| Кнопка | Тап | Удержание | HOME + |
|---|---|---|---|
| FX | SOUND; ещё раз — вкладка ENGINE/ENV/FX/MOD | — | — |
| SEL | SEQ; ещё раз — 2-й ряд | + белая 1–8: тип секвенсора | — |
| ENV | ROAD | 0,6 с: гараж частей | — |
| LFO | MIX; ещё раз — 2-й ряд | + крутить ручку SOUND: MOD 1 на неё | — |
| PLAY | старт/стоп | 0,8 с: PART/SONG | тапы: TAP TEMPO |
| REC | запись (со счётом); на ROAD — запись трассы | 1 с: очистить клип (TOMBOLA — вытряхнуть) | метроном вкл/выкл |
| GLO | следующая дорожка | 0,7 с: мьют | — |
| ARP | ARP на дорожке; ещё раз — LATCH | + белая A–H: часть (с конца такта) | ARP выкл (вернуть прежний секвенсор) |
| SEQ | FILL пока держишь | — | BAKE (DRIVE → STEP) |
| EDIT | KEYS; ещё раз — следующий ряд | 0,7 с: UNDO | REDO |
| SAVE | PROJECT; ещё раз — ряд | + белая 13–16: сохранить пресет | — |
| HOME | PLAY | SHIFT | белая 1–16: пресет; чёрная 1–6: дорожка; чёрная 7–11: COPY PASTE DOUBLE VARIATE HUMANIZE |
| OCT− / OCT+ | октава дорожки | — | клип на ступень лада вниз/вверх |

Энкодеры: SELECT / ALGO / PRESET = зелёный / голубой / жёлтый (поля страницы; на PLAY — TEMPO / MACRO / MOOD).
HOME + энкодер: скрытые параметры движка (TYPE…), FX B, слот/цель модулятора. K1–K4: DUST, SIDECHAIN, SLEEP, SWING.
Страницы: PROJECT — белые 1–4 открыть проект, 5 новый (шаблон), 6 демо; чёрные 1 EXPORT, 2 STEMS, 3 сохранить.
MIX — белые 1–6 мьют, чёрные 1–6 соло. ROAD — белые A–H добавить блок, чёрные: HALF DOUBLE REVERSE +12 −12 TAPESTOP
DROP(дорожка) DELETE DUP CUT PASTE. KEYS — выделил ROOT (энкодер 1) и нажал клавишу = тоника.

## Как сделать новую кассету (чек-лист)
1. `tapes/<имя>/tape_dsp.c`: движки (`dk_engine_t`, состояние голоса ≤ `DK_VSTATE` floats, `CHECK_SIZE`), эффекты
   (`dk_fx_t`, буфер `DK_FXBUF`), мастер, пресеты (12 на движок, MACRO-цели), стили DRIVE (`drive()` по ролям),
   прогрессии и «острые» аккорды, 16+16 рисунков FINGER, 11 перформанс-эффектов, MOOD-ы, демо (`demo()`), дорожки по
   умолчанию, звук метронома; одна структура `const dk_tape_t TAPE`.
2. `tapes/<имя>/tape_ui.c`: тема (`TH_ENC` три цвета энкодеров, `TH_PART`, `TH_TRACK`, цвета кассеты `CS_*`,
   `TAPE_TITLE`), картина PLAY (`tape_play_draw(y0)`, 240×144), `tape_ui_init/step/keep`, `tape_mood`, `app_splash`.
3. `./build.sh <имя> --host`, `sh tests/run_tests.sh <имя>`, `tools/calibrate.py` для уровней пресетов,
   `tools/make_listen.py out`, `./build.sh <имя>`.
4. Оболочка (deck/) о звуке и картинках кассеты ничего не знает — правки в ней не нужны.

## Как заменить картинку сцены
У OVERWORLD картинок нет (сад рисуется кодом, как в прошлой прошивке). Для OUTRUN: положить `scenes/src/<имя>.jpg`,
`make_scenes.py` → `scene_pack.py`, пересобрать — по ТЗ OUTRUN (кассета ещё не сделана).

## Прошивка на FM-1 (решает пользователь)
`out/install-deck.bat` (рядом `deck-overworld.fwsc`, `fm1_install.py`). Лучше на второй FM-1. Откат: тем же
установщиком OVERWORLD (`..\..\overworld\install-overworld.bat`); чёрный экран — Pico + Transporter
(Desktop\FM1Trasmitter-for-pico\ВОССТАНОВЛЕНИЕ_FM-1.txt). Проекты DECK лежат в 0xA0000…0xC4000 — там, где у OVERWORLD
были петли: после возврата на OVERWORLD его петли будут пустыми (магия не совпадёт, мусора не будет).
