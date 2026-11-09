@echo off
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" FPSEditor Win64 Development -Project="%~dp0..\FPS.uproject" -WaitMutex -FromMsBuild -architecture=x64
exit /b %ERRORLEVEL%
