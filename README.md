# DECK — прошивка-«операционка» для M-VAVE FM-1 (кассета OVERWORLD)

«Свой OP-1» на FM-1: 6 дорожек, 8 частей, трасса-аранжировка, умная клавиатура, 8 секвенсоров, микшер, экспорт в .mid.
Звук и картинка — из сменной кассеты; первая — **OVERWORLD** (лоу-фай эмбиент в духе C418, сад на экране).

- Прошивка: [`out/deck-overworld.fwsc`](out/deck-overworld.fwsc), установка — `out/install-deck.bat` (на ПК, FM-1 по USB).
- Инструкция: [`out/DECK_инструкция.pdf`](out/DECK_инструкция.pdf). Звуки: `out/listen.html` (WAV — только на домашнем ПК).
- Скриншоты экранов: `out/screens/`, `out/pages_1.png`, `out/pages_2.png`.
- ТЗ и макеты: `docs/` (DECK_SPEC, OVERWORLD_TAPE, OUTRUN_TAPE, CLAUDE_TASK, ui_concepts, scenes).
- Карта кода, кнопки, ограничения: [`CLAUDE.md`](CLAUDE.md) — её читает Claude Code в начале каждой сессии.

## Работа с телефона (Claude Code в браузере / приложении Claude)
1. claude.ai → Code → выбрать этот репозиторий → написать задачу по-русски, как дома.
2. В облаке собирается и проверяется **симулятор** (обычный gcc + python):
   `./build.sh overworld --host` и `sh tests/run_tests.sh` (ожидается ALL OK; звуки, сценарии, экспорт, скриншоты).
3. **Файл прошивки (.fwsc) в облаке не собрать** — нужен тулчейн JieLi, он только на домашнем ПК
   (`../../jlwin`, SDK `../../jieli/sdk`). Дома: `git pull`, `./build.sh overworld` → `out/deck-overworld.fwsc`.
4. Облачная сессия коммитит в свою ветку и открывает PR — посмотреть изменения и смёржить можно с телефона.

## Сборка дома (Windows, Git Bash)
```
./build.sh overworld            # прошивка → out/deck-overworld.fwsc
./build.sh overworld --host     # симулятор → deck/build/host/deck_host
sh tests/run_tests.sh           # все проверки → ALL OK
python tools/make_listen.py out # out/listen.html
```

Лицензия: MPL-2.0 (основа — fm1-base).
