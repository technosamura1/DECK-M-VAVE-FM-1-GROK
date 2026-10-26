@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo BENCH: замер. FM-1 по USB, Reaper закрыт. Результат: bench_result.txt рядом с этим файлом.
echo.
py -3 -m pip install --quiet mido python-rtmidi
py -3 deck_bench.py
echo.
pause
