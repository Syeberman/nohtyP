@echo off
setlocal
set PYTHONPATH=%~dp0
set PATH=%CD%;%PATH
python -m yp_test %*
endlocal
