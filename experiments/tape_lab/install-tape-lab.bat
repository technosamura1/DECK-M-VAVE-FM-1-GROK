@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo TAPE LAB: отдельная тестовая прошивка, одна RAM-петля до 2,97 с.
echo Для возврата в DECK установите прежний deck-overworld.fwsc.
py -3 -m pip install --quiet mido python-rtmidi
if errorlevel 1 goto done
py -3 fm1_install.py tape-lab.fwsc
:done
pause
