
@echo off
cd %~dp0
set MSRT=_140
set OUTDIR_ROOT=%~dp0.
set WORKSPACE=MOLEBOX_OSS

cmd /c Core1.Src\makeit.cmd 

for /F %%i in (projects.txt) do (	
	cmd /c %~dp0xternal\make_one.cmd %%i x86 dll %*
)
