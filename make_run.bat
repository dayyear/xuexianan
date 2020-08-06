@echo %path% | findstr /r /c:"^mingw32-7.3.0/bin;bin;" >nul
@if %errorlevel% equ 1 @path mingw32-7.3.0/bin;bin;%path%
::mingw32-make clean
::mingw32-make clean & mingw32-make type=static -j
::mingw32-make clean & mingw32-make type=dynamic -j
mingw32-make type=dynamic -j
bin\xuexianan
