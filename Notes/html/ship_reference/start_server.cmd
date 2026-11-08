@echo off
rem Ship reference with the workshop stored in the repository (Notes\html\ship_reference\workshop)
cd /d "%~dp0..\..\.."
python tools\ship_reference_server.py
