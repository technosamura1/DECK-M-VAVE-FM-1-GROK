# EXPANDED PALETTE — 04.11.2026

40 дополнительных пресетов для полноценного DECK/OVERWORLD: теперь всего 420.
Предыдущие заводские пресеты и их номера сохранены, новые заняли свободные места в существующих банках.
REC, кассета, самостоятельные тембры/FX дублей и GROOVE по умолчанию для новых барабанных дорожек сохранены.

## Послушать

Открой listen.html, раздел EXPANDED PALETTE. Четыре короткие композиции на шести дорожках:
AMBER WATER (ambient, 72 BPM), STEEL MOTOR (synthwave, 108 BPM), POCKET CITY (игровой, 132 BPM),
CEDAR ROOM (акустический характер, 96 BPM). Ниже — все сорок тембров по отдельности.
Все звуки синтезированы движками DECK; акустические названия описывают характер, это не сэмплы реальных инструментов.
Композиции используют одну гармоническую последовательность Am–F–C–G, чтобы сравнивать наборы.

## Выбрать на FM-1

Выбери мелодическую дорожку → HOME + PRESET (SOUNDS) → инструмент → банк → номер пресета.
OCT меняет инструмент, чёрные клавиши выбирают банк, белые — пресет. Номера с 1.
Отдельный новый банк не добавлялся: в CINE лежат деревянные клавиши, маллеты и калимбы;
в SYN — электрические и яркие тембры; в 8BIT — игровые аккорды и духовые.

| Название | Инструмент | Банк | Номер |
|---|---|---|---|
| STUDIO WOOD | PIANO | CINE | 07 |
| PAPER HAMMER | PIANO | CINE | 08 |
| SOFT PEDAL | PIANO | CINE | 09 |
| TOY ATTIC | PIANO | CINE | 10 |
| CITY STAB | PIANO | SYN | 05 |
| VHS CHORD | PIANO | SYN | 06 |
| DIAMOND KEY | PIANO | SYN | 07 |
| VELVET DRIVE | PIANO | SYN | 08 |
| EMBER KEYS | KEYS | CINE | 05 |
| FROZEN TINE | KEYS | CINE | 06 |
| BRONZE DUST | KEYS | CINE | 07 |
| SLOW CURRENT | KEYS | CINE | 08 |
| POCKET ORGAN | PAD | 8BIT | 01 |
| PIXEL CHOIR | PAD | 8BIT | 02 |
| CART STRINGS | PAD | 8BIT | 03 |
| TINY CLOUD | PAD | 8BIT | 04 |
| BAMBOO HIT | GLASS | CINE | 07 |
| FELT VIBES | GLASS | CINE | 08 |
| TIN LANTERN | GLASS | CINE | 09 |
| ICE ORBIT | GLASS | CINE | 10 |
| CEDAR BASS | BASS | CINE | 09 |
| BOWED EARTH | BASS | CINE | 10 |
| ROUND HUM | BASS | CINE | 11 |
| BRONZE STEP | BASS | CINE | 12 |
| DRY THUMB | KALIMBA | CINE | 01 |
| AMBER SEED | KALIMBA | CINE | 02 |
| WATER TINES | KALIMBA | CINE | 03 |
| HOLLOW LOG | KALIMBA | CINE | 04 |
| POCKET PIPE | WINDS | 8BIT | 01 |
| ROUND OCARINA | WINDS | 8BIT | 02 |
| REED SPRITE | WINDS | 8BIT | 03 |
| PIXEL HORN | WINDS | 8BIT | 04 |
| NEON NYLON | PLUCK | SYN | 07 |
| STEEL MOTOR | PLUCK | SYN | 08 |
| KOTO SPARK | PLUCK | SYN | 09 |
| HARP AURA | PLUCK | SYN | 10 |
| CHROME REED | LEAD | SYN | 09 |
| SOFT COMET | LEAD | SYN | 10 |
| FM CRYSTAL | LEAD | SYN | 11 |
| STREET SUPER | LEAD | SYN | 12 |

## Проверка и сборка

- Все 40 проходят штатные проверки пика, DC, щелчков, хвостов, спектрального индикатора и выравнивания громкости.
- Проверка отличимости: 0 почти одинаковых пар среди новой сороковки.
- Четыре микса: обнаруженных щелчков 0, пики −7,9 / −3,7 / −5,1 / −4,8 dBFS, хвосты дописаны до затухания.
  Спектральный порог чистого тембра не применяется как доказательство алиасинга в миксе с шумовыми ударными.
- Рендер всех 420 пресетов оставляет два предупреждения у прежних BROKEN (уровень −2,2 дБ от медианы)
  и SYNTH MONO (щелчок). Они не относятся к новым сорока; старые automation/fxlong этой работой не исправлялись.
- Тест двух записанных пианино со своими FX и третьего живого звука проходит.
  Старый дубль побитно одинаков при смене текущего тембра/FX: хеш e9c9154f в обоих случаях.
- Образ 349256 байт; RAM 83356/98304, pool 333372/344064 — расход RAM не вырос относительно Small Worlds.
- Новых алгоритмов генерации и голосовых состояний не добавлялось. Цена разных тембров зависит от движка;
  большое число длинных голосов и FX по-прежнему может нагрузить процессор. На самом FM-1 сборка ещё не проверена.

## Файлы

`deck-expanded-palette.zip` — полная прошивка с установщиком. Внутри файл называется deck-overworld.fwsc.
`expanded-palette-listen.zip` — страница с WAV, распаковывается целиком; для прослушивания установка не нужна.
Устройство автоматически не прошивалось, новый релиз GitHub не публиковался.

Исходники новых пресетов — tapes/overworld/tape_dsp.c; карта — out/palette-presets.json.
Рендер звуков: deck_test sounds OUTDIR; примеры: deck_test palette.
Генератор страницы: python tools/make_style_listen.py.
Одноразовый скрипт добавления к прежней таблице: tools/expand_palette.py; финальные тримы откалиброваны после рендера.
