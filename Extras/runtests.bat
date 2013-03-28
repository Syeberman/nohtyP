@echo off
setlocal
set PYTHONPATH=%~dp0
set PATH=%CD%;C:\Program Files\Python33;%PATH%
python -m yp_test %*
endlocal
