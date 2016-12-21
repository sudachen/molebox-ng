@echo off 
set MAKEIT_DIR=%~dp0
set WORKSPACE=MOLEBOX_OSS

cmd /c %XTERNAL%\makeit_proc.cmd dll %* 
