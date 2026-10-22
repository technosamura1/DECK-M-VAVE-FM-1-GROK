@echo off
rem DECK -> GitHub: first create an EMPTY private repo on github.com/new (no README), then run this and paste its URL.
cd /d "%~dp0.."
set /p URL=Repo URL (https://github.com/USER/deck.git): 
git remote remove origin 2>nul
git remote add origin %URL%
git branch -M main
git push -u origin main
pause
