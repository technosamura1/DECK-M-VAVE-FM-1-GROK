@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo DECK + OVERWORLD: установка на FM-1
echo Лучше на ВТОРОЙ FM-1 (для экспериментов). Полная батарея, ПК не должен уснуть.
echo Закрой Reaper и все музыкальные программы. FM-1 подключи кабелем с данными, напрямую, без хаба.
echo.
py -3 -m pip install --quiet mido python-rtmidi
py -3 fm1_install.py deck-overworld.fwsc
echo.
pause
