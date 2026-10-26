@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo BENCH: замер процессора на FM-1 (звука нет). Лучше на ВТОРУЮ FM-1.
echo Полная батарея, ПК не должен уснуть. Закрой Reaper и все музыкальные программы.
echo FM-1 подключи кабелем с данными, напрямую, без хаба. Откат: install-deck.bat из архива DECK.
echo.
py -3 -m pip install --quiet mido python-rtmidi
py -3 fm1_install.py bench.fwsc
echo.
pause
