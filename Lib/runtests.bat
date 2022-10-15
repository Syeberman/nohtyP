@echo off
setlocal
set PYTHONPATH=%~dp0
set NOHTYP_LIBRARY=%~dp0\..\%1\nohtyP.dll
shift
python -m yp_test %$
endlocal
