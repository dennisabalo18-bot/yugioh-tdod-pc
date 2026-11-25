@echo off
rem Arma voces_latino\ (los audios listos y los textos del globo en espanol
rem latino) desde dialogos\guion.json y dialogos\audio\. Correrlo cada vez
rem que generes o cambies frases en el programa de doblaje.
cd /d "%~dp0"
py -3 -I "%~dp0..\tools\voces_mod.py"
pause
