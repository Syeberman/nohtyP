@echo off
setlocal
set PYTHONPATH=%~dp0
set PATH=%CD%\%1;%PATH%
shift
python -m yp_test %$
endlocal
