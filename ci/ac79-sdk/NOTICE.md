# Три файла из JieLi AC79 SDK — только для облачной сборки

`cpu/wl82/tools/uboot.boot`, `cpu/wl82/tools/cfg_tool.bin`, `cpu/wl82/tools/cfg/eq_cfg_hw.bin` — это всё, что
сборщику пакета (`base/bsp/tools/fm1pkg.py`) нужно из SDK для прошивки без Bluetooth/WiFi.

Источник: JieLi AC79 SDK (AC79NN_SDK_V1.2.1_2023-12-13, ревизия `d179b4484759423312073f5fbb232501aa491047`,
github.com/Jieli-Tech/fw-AC79_AIoT_SDK), лицензия Apache-2.0 — текст: `base/LICENSES/Apache-2.0.txt`.
Файлы не изменены; SHA-256 сверяются в CI с теми же, что проверяет `base/bsp/tools/get_sdk.sh`.
Лежат здесь, потому что исходный репозиторий SDK из GitHub Actions недоступен. Дома используется полный SDK.
