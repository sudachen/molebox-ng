
@echo off
cd %~dp0
set WORKSPACE=MOLEBOX_OSS
set OUTDIR_ROOT=%~dp0.

cmd /c Core1.Src\makeit.cmd 

for /F %%i in (projects.txt) do (	
	cmd /c %XTERNAL%\make_one.cmd %%i x86 dll release %*
)
