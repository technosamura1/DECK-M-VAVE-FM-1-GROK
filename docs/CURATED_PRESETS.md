# DECK — сокращённый выбор пресетов, 04.11.2026

В обычном выборе осталось 354 пресета. Убраны 66 близких вариантов, 60 из прежних наборов и 6 из последнего расширения.
Сравнение учитывает спектр и изменение громкости в шести частотных полосах по времени, без разницы общего уровня.
Порог отбора: среднее спектральное расстояние <1,25 дБ и расстояние огибающих <2,2 дБ внутри одного движка.
Это оценка сходства, а не утверждение, что звуки идентичны во всех регистрах и при любых настройках.
Сохранены основные тембры каждого банка и специальная подборка KEYS; среди остальных предпочтение новым наборам.

## Пользование

SOUNDS, ручка PRESET и HOME+белые теперь выбирают сокращённый список. Номера в меню компактные, без пропусков.
Пользовательские пресеты не фильтруются. Номера карточек listen.html обновлены под новое меню.
В старых инструкциях номера некоторых пресетов больше не соответствуют белым клавишам — ориентируйся по имени.

Для сохранённых проектов все 420 исходных ID и их параметры оставлены доступными внутри прошивки.
Это позволяет загрузить старый проект с убранным тембром без подмены звука. При переходе к следующему пресету
ручка уже идёт по сокращённому списку. Если открыт такой старый тембр, его название видно, но его ячейки в сетке нет.
Зарезервированные тембры нужны для совместимости: физическая таблица звуков не удалена из образа.

REC, кассетный экран, записанные инструменты и их FX этой работой не менялись.
Все прежние ограничения FX/голосов и предупреждения звукового набора сохраняются; сокращение каталога само
по себе не уменьшает CPU при воспроизведении уже записанной партии.

## Проверка

- Тест проходит все 420 raw-ID: каждый по-прежнему загружается, 354 отображаются, 66 исключены.
- Проверена взаимная конверсия номера ячейки и raw-ID, а также все 12 специальных клавиш KEYS.
- Сценарий реальных кнопок/ручки выбирает компактные ячейки и пропускает убранные слоты.
- Сценарии record, recmulti, layers проходят.
- Записанный слой сохраняет побитно одинаковое аудио при смене текущего звука/FX: e9c9154f в обоих случаях.
- Образ 349544 байта, RAM 83356/98304, pool 333372/344064. На физическом FM-1 новая сборка ещё не проверена.
- Повторный звуковой рендер не выполнялся: таблицы параметров и DSP не менялись, использован рендер всех 420
  из Expanded Palette. Прежние предупреждения BROKEN/SYNTH MONO и automation/fxlong не устранялись этой работой.

## Что убрано из выбора

| Движок | Убранный вариант | Оставленный близкий тембр |
|---|---|---|
| KALIMBA | WATER TINES | STARLIGHT |
| KALIMBA | HOLLOW LOG | MBIRA |
| KEYS | EMBER KEYS | MELLOW |
| KEYS | BRONZE DUST | SUITCASE |
| PIANO | SOFT PEDAL | VHS CHORD |
| BASS | ROUND HUM | EARTH SUB |
| KALIMBA | THUMB | DRY THUMB |
| PLUCK | KOTO | KOTO SPARK |
| SYNTH | SINE | BLUE GHOST |
| KALIMBA | CAMPFIRE | MBIRA |
| PIANO | SNOWFALL | MOSS PIANO |
| BASS | FELT BASS | EARTH SUB |
| PAD | DEEP | STRINGS |
| PIANO | NIGHT | LULLABY |
| KALIMBA | DEEP | MBIRA |
| KEYS | CLAV | SUITCASE |
| PIANO | MEMORY | SWEDEN |
| BASS | SQUARE | PLUCK |
| KEYS | ARCADE EP | COIN |
| PLUCK | PLUCK BIT | PIXEL KOTO |
| BASS | TRI SLIDE | DUST BASS |
| BASS | OUTRUN | MOTOR BASS |
| KEYS | NEON EP | VELVET |
| PIANO | OUTRUN KEYS | NEON PIANO |
| WINDS | BRASS STAB | SYNTH BRASS |
| BASS | ARP BASS | MOTOR BASS |
| PIANO | DRIVE PIANO | NEON PIANO |
| WINDS | LEAD HORN | SYNTH BRASS |
| BASS | ANALOG | MOOG |
| PAD | SUNSET | NIGHT DRIVE PAD |
| PIANO | RETRO GRAND | HALL |
| DRUMKIT | SYNTH POP | 16-BIT |
| WINDS | OBERHEIM | SYNTH BRASS |
| DRUMKIT | GATED | DRIVE |
| GLASS | CITY LIGHTS | CHAPEL BELL |
| KEYS | SPARKLE | TURBO |
| PLUCK | MIAMI NYLON | SYNTH LUTE |
| BASS | GLIDER | GLIDE |
| DRUMKIT | ELECTRO | OUTRUN |
| KEYS | RETRO TINE | SHIMMER |
| KEYS | MIAMI | ELECTRIC DREAM |
| SYNTH | DREAM PAD | WARM SUPER |
| BASS | ACID | BRONZE STEP |
| BASS | MIDNIGHT | DETUNED |
| BASS | LASER | RESO BASS |
| DRUMKIT | 909 HOUSE | 909 TECHNO |
| DRUMKIT | TAIKO | WAR |
| KEYS | DISTANT BELLS | CRYSTAL |
| PIANO | HEARTH | MOSS PIANO |
| GLASS | FROST BELLS | ICE ORBIT |
| KEYS | MOONLIT | SUNSET EP |
| KEYS | ARCANE | CRYSTAL |
| PIANO | SNOWHOLD | MOSS PIANO |
| BASS | ANCIENT | NIGHT |
| DRUMKIT | BATTLE | WAR DRUMS |
| PLUCK | NORDIC LYRE | LUTE |
| PIANO | LAMENT | SOLITUDE |
| DRUMKIT | MARCH | TRIBAL |
| BASS | BRONZE | ROUND |
| PAD | FALKREATH | NIGHT DRIVE PAD |
| SYNTH | WIND SYNTH | BREATH |
| PAD | IVORY CHAPEL | ORGAN GLOW |
| PLUCK | HARP GLISS | ANGEL HARP |
| WINDS | LAMENT | DUDUK |
| PLUCK | FOLK NYLON | BARD'S LUTE |
| SYNTH | SOLEMN | BED |

## Полный список выбора

| Движок | Банк | Белая клавиша | Тембр |
|---|---|---|---|
| PIANO | AMB | 01 | MOSS PIANO |
| PIANO | AMB | 02 | SWEDEN |
| PIANO | AMB | 03 | LULLABY |
| PIANO | AMB | 04 | MORNING |
| PIANO | AMB | 05 | ATTIC |
| PIANO | AMB | 06 | HALL |
| PIANO | AMB | 07 | DISTANT |
| PIANO | AMB | 08 | GRAND |
| PIANO | AMB | 09 | LOFI KEYS |
| PIANO | 8BIT | 01 | 8-BIT PIANO |
| PIANO | 8BIT | 02 | GAME KEYS |
| PIANO | 8BIT | 03 | PIXEL |
| PIANO | 8BIT | 04 | 16-BIT KEYS |
| PIANO | SYN | 01 | NEON PIANO |
| PIANO | SYN | 02 | CITY STAB |
| PIANO | SYN | 03 | VHS CHORD |
| PIANO | SYN | 04 | DIAMOND KEY |
| PIANO | SYN | 05 | VELVET DRIVE |
| PIANO | CINE | 01 | TAVERN |
| PIANO | CINE | 02 | SOLITUDE |
| PIANO | CINE | 03 | ELDER KEYS |
| PIANO | CINE | 04 | STUDIO WOOD |
| PIANO | CINE | 05 | PAPER HAMMER |
| PIANO | CINE | 06 | TOY ATTIC |
| KEYS | AMB | 01 | RAINY DAY |
| KEYS | AMB | 02 | SUITCASE |
| KEYS | AMB | 03 | DUSTY |
| KEYS | AMB | 04 | VELVET |
| KEYS | AMB | 05 | BELLTONE |
| KEYS | AMB | 06 | STUDY |
| KEYS | AMB | 07 | WURLI |
| KEYS | AMB | 08 | SHIMMER |
| KEYS | AMB | 09 | BROKEN |
| KEYS | AMB | 10 | MELLOW |
| KEYS | AMB | 11 | NIGHT |
| KEYS | 8BIT | 01 | FM CHIP |
| KEYS | 8BIT | 02 | COIN |
| KEYS | 8BIT | 03 | SNES EP |
| KEYS | SYN | 01 | NEON DX |
| KEYS | SYN | 02 | ELECTRIC DREAM |
| KEYS | SYN | 03 | SUNSET EP |
| KEYS | SYN | 04 | TURBO |
| KEYS | CINE | 01 | CRYSTAL |
| KEYS | CINE | 02 | FROZEN TINE |
| KEYS | CINE | 03 | SLOW CURRENT |
| PAD | AMB | 01 | SWEDEN |
| PAD | AMB | 02 | AIR |
| PAD | AMB | 03 | HORIZON PAD |
| PAD | AMB | 04 | SHIMMER |
| PAD | AMB | 05 | STRINGS |
| PAD | AMB | 06 | CAVE |
| PAD | AMB | 07 | DAWN |
| PAD | AMB | 08 | FOG |
| PAD | AMB | 09 | GLOW |
| PAD | AMB | 10 | SKY |
| PAD | AMB | 11 | SLOW |
| PAD | 8BIT | 01 | POCKET ORGAN |
| PAD | 8BIT | 02 | PIXEL CHOIR |
| PAD | 8BIT | 03 | CART STRINGS |
| PAD | 8BIT | 04 | TINY CLOUD |
| PAD | SYN | 01 | SUNSET PAD |
| PAD | SYN | 02 | NIGHT DRIVE PAD |
| PAD | SYN | 03 | NEON STRINGS |
| PAD | SYN | 04 | ORGAN GLOW |
| PAD | SYN | 05 | RETRO WARM |
| PAD | SYN | 06 | DARK WAVE |
| PAD | SYN | 07 | ARP BED |
| PAD | CINE | 01 | CHOIR AH |
| PAD | CINE | 02 | CHOIR OH |
| PAD | CINE | 03 | CHOIR EE |
| PAD | CINE | 04 | SOVNGARDE |
| PAD | CINE | 05 | DRAGON |
| PAD | CINE | 06 | DEEP DRONE |
| PAD | CINE | 07 | WINTERHOLD |
| PAD | CINE | 08 | DAWNSTAR |
| PAD | CINE | 09 | BLACKREACH |
| PAD | CINE | 10 | HIGH HROTHGAR |
| GLASS | AMB | 01 | DEW BELL |
| GLASS | AMB | 02 | CELESTA |
| GLASS | AMB | 03 | MARIMBA |
| GLASS | AMB | 04 | MUSIC BOX |
| GLASS | AMB | 05 | VIBES |
| GLASS | AMB | 06 | SNOW |
| GLASS | AMB | 07 | STARS |
| GLASS | AMB | 08 | CHAPEL |
| GLASS | AMB | 09 | WOOD |
| GLASS | AMB | 10 | DROPS |
| GLASS | AMB | 11 | GLOCK |
| GLASS | AMB | 12 | CHIMES |
| GLASS | 8BIT | 01 | RUIN BELL |
| GLASS | 8BIT | 02 | TINY STEPS |
| GLASS | 8BIT | 03 | LOST TOY |
| GLASS | 8BIT | 04 | MEMORY DUST |
| GLASS | SYN | 01 | NEON BELLS |
| GLASS | SYN | 02 | SYNTH VIBES |
| GLASS | SYN | 03 | NIGHT CELESTA |
| GLASS | SYN | 04 | DREAM BOX |
| GLASS | SYN | 05 | RETRO MALLET |
| GLASS | CINE | 01 | CHAPEL BELL |
| GLASS | CINE | 02 | TEMPLE |
| GLASS | CINE | 03 | SHRINE |
| GLASS | CINE | 04 | TOLL |
| GLASS | CINE | 05 | DAWN CHIME |
| GLASS | CINE | 06 | BAMBOO HIT |
| GLASS | CINE | 07 | FELT VIBES |
| GLASS | CINE | 08 | TIN LANTERN |
| GLASS | CINE | 09 | ICE ORBIT |
| BASS | AMB | 01 | EARTH SUB |
| BASS | AMB | 02 | ROUND |
| BASS | AMB | 03 | UPRIGHT |
| BASS | AMB | 04 | THUMB |
| BASS | AMB | 05 | MOOG |
| BASS | AMB | 06 | GLIDE |
| BASS | AMB | 07 | DEEP |
| BASS | AMB | 08 | REESE |
| BASS | AMB | 09 | JAZZ |
| BASS | AMB | 10 | NIGHT |
| BASS | AMB | 11 | PLUCK |
| BASS | 8BIT | 01 | DUST BASS |
| BASS | 8BIT | 02 | PULSE 25 |
| BASS | 8BIT | 03 | GB BASS |
| BASS | 8BIT | 04 | ARCADE |
| BASS | 8BIT | 05 | BOSS SAW |
| BASS | 8BIT | 06 | CHIP SUB |
| BASS | SYN | 01 | MOTOR BASS |
| BASS | SYN | 02 | RESO BASS |
| BASS | SYN | 03 | DETUNED |
| BASS | SYN | 04 | PULSE DRIVE |
| BASS | SYN | 05 | SUB WAVE |
| BASS | CINE | 01 | SUB DRONE |
| BASS | CINE | 02 | RUMBLE |
| BASS | CINE | 03 | WAR HORN LOW |
| BASS | CINE | 04 | BOWED BASS |
| BASS | CINE | 05 | DRAGON BREATH |
| BASS | CINE | 06 | CELLO LOW |
| BASS | CINE | 07 | CEDAR BASS |
| BASS | CINE | 08 | BOWED EARTH |
| BASS | CINE | 09 | BRONZE STEP |
| DRUMKIT | AMB | 01 | DUSTY |
| DRUMKIT | AMB | 02 | 707 |
| DRUMKIT | AMB | 03 | PAPER KIT |
| DRUMKIT | AMB | 04 | BOOM BAP |
| DRUMKIT | AMB | 05 | 808 |
| DRUMKIT | AMB | 06 | WOODEN |
| DRUMKIT | AMB | 07 | 909 |
| DRUMKIT | AMB | 08 | CHIP |
| DRUMKIT | AMB | 09 | 606 |
| DRUMKIT | AMB | 10 | CAMPFIRE |
| DRUMKIT | AMB | 11 | WAR |
| DRUMKIT | AMB | 12 | SYNTH |
| DRUMKIT | 8BIT | 01 | POCKET KIT |
| DRUMKIT | 8BIT | 02 | GAME BOY |
| DRUMKIT | 8BIT | 03 | ARCADE |
| DRUMKIT | 8BIT | 04 | C64 |
| DRUMKIT | 8BIT | 05 | 16-BIT |
| DRUMKIT | 8BIT | 06 | CRUNCH |
| DRUMKIT | 8BIT | 07 | SNES KIT |
| DRUMKIT | 8BIT | 08 | PIXEL |
| DRUMKIT | 8BIT | 09 | 606 TIGHT |
| DRUMKIT | 8BIT | 10 | 606 FUNK |
| DRUMKIT | 8BIT | 11 | CR-78 SOFT |
| DRUMKIT | 8BIT | 12 | CR-78 DISCO |
| DRUMKIT | SYN | 01 | NIGHT KIT |
| DRUMKIT | SYN | 02 | OUTRUN |
| DRUMKIT | SYN | 03 | DRIVE |
| DRUMKIT | SYN | 04 | NEON |
| DRUMKIT | SYN | 05 | SUNSET |
| DRUMKIT | SYN | 06 | 808 CLASSIC |
| DRUMKIT | SYN | 07 | 808 LONG |
| DRUMKIT | SYN | 08 | 909 TECHNO |
| DRUMKIT | CINE | 01 | WAR DRUMS |
| DRUMKIT | CINE | 02 | DEEP SKINS |
| DRUMKIT | CINE | 03 | TRIBAL |
| DRUMKIT | CINE | 04 | HEARTBEAT |
| DRUMKIT | CINE | 05 | THUNDER |
| DRUMKIT | CINE | 06 | 808 SUB BOOM |
| DRUMKIT | CINE | 07 | 808 DIRTY |
| DRUMKIT | CINE | 08 | 909 HARD |
| DRUMKIT | CINE | 09 | 707 PUNCH |
| WORLD | AMB | 01 | RAIN |
| WORLD | AMB | 02 | WIND |
| WORLD | AMB | 03 | FIRE |
| WORLD | AMB | 04 | STREAM |
| WORLD | AMB | 05 | CAVE DROPS |
| WORLD | AMB | 06 | CRICKETS |
| WORLD | AMB | 07 | BIRDS |
| WORLD | AMB | 08 | OCEAN |
| WORLD | AMB | 09 | THUNDER |
| WORLD | AMB | 10 | SNOW STEPS |
| WORLD | AMB | 11 | STORM |
| WORLD | AMB | 12 | DRIZZLE |
| WORLD | CINE | 01 | BLIZZARD |
| WORLD | CINE | 02 | TAVERN FIRE |
| WORLD | CINE | 03 | DRAGON STORM |
| WORLD | CINE | 04 | MOUNTAIN |
| KALIMBA | AMB | 01 | SEED PLUCK |
| KALIMBA | AMB | 02 | MBIRA |
| KALIMBA | AMB | 03 | BRIGHT |
| KALIMBA | AMB | 04 | BOX |
| KALIMBA | AMB | 05 | WOODEN |
| KALIMBA | AMB | 06 | DREAM |
| KALIMBA | AMB | 07 | LOFI |
| KALIMBA | AMB | 08 | STARLIGHT |
| KALIMBA | AMB | 09 | TINY |
| KALIMBA | CINE | 01 | DRY THUMB |
| KALIMBA | CINE | 02 | AMBER SEED |
| WINDS | AMB | 01 | FLUTE |
| WINDS | AMB | 02 | PAN FLUTE |
| WINDS | AMB | 03 | OCARINA |
| WINDS | AMB | 04 | SHAKUHACHI |
| WINDS | AMB | 05 | CLARINET |
| WINDS | AMB | 06 | TRUMPET |
| WINDS | AMB | 07 | MUTED |
| WINDS | AMB | 08 | HORN |
| WINDS | AMB | 09 | BRASS |
| WINDS | AMB | 10 | FLUGEL |
| WINDS | AMB | 11 | DUDUK |
| WINDS | AMB | 12 | SYNBRASS |
| WINDS | 8BIT | 01 | POCKET PIPE |
| WINDS | 8BIT | 02 | ROUND OCARINA |
| WINDS | 8BIT | 03 | REED SPRITE |
| WINDS | 8BIT | 04 | PIXEL HORN |
| WINDS | SYN | 01 | SYNTH BRASS |
| WINDS | SYN | 02 | NEON REED |
| WINDS | SYN | 03 | NEON FLUTE |
| WINDS | CINE | 01 | NORDIC HORN |
| WINDS | CINE | 02 | WAR HORN |
| WINDS | CINE | 03 | BATTLE BRASS |
| WINDS | CINE | 04 | CALL |
| WINDS | CINE | 05 | DUDUK |
| WINDS | CINE | 06 | BARD FLUTE |
| WINDS | CINE | 07 | PIPES |
| WINDS | CINE | 08 | DISTANT HORN |
| WINDS | CINE | 09 | PAN WOODS |
| WINDS | CINE | 10 | CARNYX |
| WINDS | CINE | 11 | HUNTER |
| SYNTH | AMB | 01 | SOFT SAW |
| SYNTH | AMB | 02 | DUST LEAD |
| SYNTH | AMB | 03 | WARM SUPER |
| SYNTH | AMB | 04 | BED |
| SYNTH | AMB | 05 | SQUARE |
| SYNTH | AMB | 06 | HAZE |
| SYNTH | AMB | 07 | BELL |
| SYNTH | AMB | 08 | PLUCK |
| SYNTH | AMB | 09 | BREATH |
| SYNTH | AMB | 10 | CRUSH |
| SYNTH | AMB | 11 | ACID |
| SYNTH | 8BIT | 01 | SMALL SOUL |
| SYNTH | 8BIT | 02 | BONE PULSE |
| SYNTH | 8BIT | 03 | SAVE POINT |
| SYNTH | 8BIT | 04 | RED HEART |
| SYNTH | 8BIT | 05 | DUST ROAD |
| SYNTH | 8BIT | 06 | ODD FRIEND |
| SYNTH | 8BIT | 07 | GOLD HALL |
| SYNTH | 8BIT | 08 | BLUE GHOST |
| SYNTH | 8BIT | 09 | STAR ROOM |
| SYNTH | 8BIT | 10 | LAST HEART |
| SYNTH | 8BIT | 11 | SNOW TOWN |
| SYNTH | 8BIT | 12 | DEEP DREAM |
| SYNTH | SYN | 01 | NIGHT DRIVE |
| SYNTH | SYN | 02 | OUTRUN LEAD |
| SYNTH | SYN | 03 | SUNSET SAW |
| SYNTH | SYN | 04 | NEON PLUCK |
| SYNTH | SYN | 05 | MIAMI BRASS |
| SYNTH | SYN | 06 | RETRO SQUARE |
| SYNTH | SYN | 07 | LASER LEAD |
| SYNTH | SYN | 08 | MOTOR ARP |
| SYNTH | SYN | 09 | DARK DRIVE |
| SYNTH | SYN | 10 | VAPOR |
| SYNTH | SYN | 11 | MIDNIGHT |
| SYNTH | CINE | 01 | DRONE SAW |
| SYNTH | CINE | 02 | DEEP SUPER |
| SYNTH | CINE | 03 | RUMBLE |
| SYNTH | CINE | 04 | ANCIENT |
| SYNTH | CINE | 05 | HORN SWELL |
| SYNTH | CINE | 06 | SINE SUB |
| SYNTH | CINE | 07 | DARK PAD |
| SYNTH | CINE | 08 | ELDER |
| SYNTH | CINE | 09 | TENSION |
| SYNTH | CINE | 10 | VOID |
| PLUCK | AMB | 01 | NYLON |
| PLUCK | AMB | 02 | HARP |
| PLUCK | AMB | 03 | LUTE |
| PLUCK | AMB | 04 | DULCIMER |
| PLUCK | AMB | 05 | STEEL |
| PLUCK | AMB | 06 | NIGHT HARP |
| PLUCK | AMB | 07 | WORN |
| PLUCK | AMB | 08 | ECHO KOTO |
| PLUCK | AMB | 09 | CAMPFIRE |
| PLUCK | AMB | 10 | ANGEL |
| PLUCK | AMB | 11 | MUTE |
| PLUCK | 8BIT | 01 | CHIP HARP |
| PLUCK | 8BIT | 02 | 8-BIT LUTE |
| PLUCK | 8BIT | 03 | PIXEL KOTO |
| PLUCK | 8BIT | 04 | GB NYLON |
| PLUCK | 8BIT | 05 | ARCADE HARP |
| PLUCK | SYN | 01 | NEON HARP |
| PLUCK | SYN | 02 | SYNTH LUTE |
| PLUCK | SYN | 03 | DX PLUCK |
| PLUCK | SYN | 04 | KOTO WAVE |
| PLUCK | SYN | 05 | GLASS HARP |
| PLUCK | SYN | 06 | NEON NYLON |
| PLUCK | SYN | 07 | STEEL MOTOR |
| PLUCK | SYN | 08 | KOTO SPARK |
| PLUCK | SYN | 09 | HARP AURA |
| PLUCK | CINE | 01 | LUTE |
| PLUCK | CINE | 02 | BARD'S LUTE |
| PLUCK | CINE | 03 | HARP |
| PLUCK | CINE | 04 | ANGEL HARP |
| PLUCK | CINE | 05 | DULCIMER |
| PLUCK | CINE | 06 | TAVERN STEEL |
| PLUCK | CINE | 07 | KOTO |
| PLUCK | CINE | 08 | DARK LUTE |
| PLUCK | CINE | 09 | MOURNING |
| LEAD | AMB | 01 | SOFT LEAD |
| LEAD | AMB | 02 | SQUARE |
| LEAD | AMB | 03 | DUSTY |
| LEAD | AMB | 04 | SUPER |
| LEAD | AMB | 05 | FM BELL |
| LEAD | AMB | 06 | FLUTE |
| LEAD | AMB | 07 | RAIN |
| LEAD | AMB | 08 | GLIDE |
| LEAD | AMB | 09 | REED |
| LEAD | AMB | 10 | ACID |
| LEAD | AMB | 11 | WIDE |
| LEAD | AMB | 12 | SNOW |
| LEAD | 8BIT | 01 | BONE SOLO |
| LEAD | 8BIT | 02 | SOUL SOLO |
| LEAD | 8BIT | 03 | GHOST CALL |
| LEAD | 8BIT | 04 | TINY HERO |
| LEAD | 8BIT | 05 | ODD VOICE |
| LEAD | 8BIT | 06 | RED ALERT |
| LEAD | 8BIT | 07 | MOON SLIDE |
| LEAD | 8BIT | 08 | LAST BOSS |
| LEAD | SYN | 01 | NEON SOLO |
| LEAD | SYN | 02 | NEON SUPERLEAD |
| LEAD | SYN | 03 | SUNSET LEAD |
| LEAD | SYN | 04 | SYNTH MONO |
| LEAD | SYN | 05 | LASER SOLO |
| LEAD | SYN | 06 | DX LEAD |
| LEAD | SYN | 07 | VAPOR SOLO |
| LEAD | SYN | 08 | CHROME |
| LEAD | SYN | 09 | CHROME REED |
| LEAD | SYN | 10 | SOFT COMET |
| LEAD | SYN | 11 | FM CRYSTAL |
| LEAD | SYN | 12 | STREET SUPER |
| LEAD | CINE | 01 | HORN LEAD |
| LEAD | CINE | 02 | EPIC SUPERLEAD |
| LEAD | CINE | 03 | LONELY TRUMPET |
| LEAD | CINE | 04 | DUDUK |
| LEAD | CINE | 05 | ELEGY |
| LEAD | CINE | 06 | TENSE FM |
| LEAD | CINE | 07 | ARCTIC BELL LEAD |
| LEAD | CINE | 08 | WAR HORN |

## Файлы

`deck-curated.zip`: полноценная прошивка, привычный установщик и этот список.
`curated-listen.zip`: обновлённая страница с избранными тембрами и прежними композициями; распаковывается целиком.
Демо оставлены как записанные примеры и могут включать совместимые архивные тембры.

Данные отбора: out/preset-curation.json; маски меню: tapes/overworld/browse_mask.h.
Воспроизведение отбора: рендер всех raw-пресетов стендом, затем
`python tools/curate_presets.py --sounds OUTDIR --refresh` и `python tools/make_style_listen.py`.
Тесты: tests/host/curation_regression.c и tests/scenarios/curated.dk.
