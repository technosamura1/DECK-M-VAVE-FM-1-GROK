# DECK — прошивка-«операционка» для M-VAVE FM-1 (кассета OVERWORLD)

«Свой OP-1» на FM-1: 6 дорожек, 8 частей, трасса-аранжировка, умная клавиатура, 8 секвенсоров, микшер, экспорт в .mid.
Звук и картинка — из сменной кассеты; первая — **OVERWORLD** (лоу-фай эмбиент в духе C418, сад на экране).

- Прошивка: **Releases → latest** (собирается автоматически) или [`out/deck-overworld.fwsc`](out/deck-overworld.fwsc); установка — `install-deck.bat` (на ПК, FM-1 по USB).
- Инструкция: [`out/DECK_инструкция.pdf`](out/DECK_инструкция.pdf). Звуки: `out/listen.html` (WAV — только на домашнем ПК).
- Скриншоты экранов: `out/screens/`, `out/pages_1.png`, `out/pages_2.png`.
- ТЗ и макеты: `docs/` (DECK_SPEC, OVERWORLD_TAPE, OUTRUN_TAPE, CLAUDE_TASK, ui_concepts, scenes).
- Карта кода, кнопки, ограничения: [`CLAUDE.md`](CLAUDE.md) — её читает Claude Code в начале каждой сессии.

## Свежая прошивка — без сборки
GitHub сам собирает прошивку после каждого изменения.
- **Готовая из `main`:** вкладка **Releases → latest** → скачай `deck-overworld.zip`.
- **Из любой ветки/PR:** вкладка **Actions** → нужный запуск «firmware» → внизу **Artifacts**.

Установка: распакуй zip на ПК → подключи FM-1 по USB → `install-deck.bat`. Лучше на второй FM-1.

## Работа с телефона (Claude Code в браузере / приложении Claude)
1. claude.ai → Code → выбрать этот репозиторий → написать задачу по-русски, как дома.
2. Claude правит код, собирает симулятор, гоняет тесты (звуки, сценарии, скриншоты) и открывает PR.
3. В PR дождись зелёной галочки «firmware» — значит прошивка собралась (её можно скачать из Artifacts этого запуска).
4. Нажми **Merge** → через пару минут свежая прошивка появится в **Releases → latest**.

## Сборка дома (Windows, Git Bash)
```
./build.sh overworld            # прошивка → out/deck-overworld.fwsc
./build.sh overworld --host     # симулятор → deck/build/host/deck_host
sh tests/run_tests.sh           # все проверки → ALL OK
python tools/make_listen.py out # out/listen.html
```

Лицензия: MPL-2.0 (основа — fm1-base).
