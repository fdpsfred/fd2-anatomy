@echo off
rem build_test.bat - compile + link test_audio.exe inside DOSBox-X (Watcom 9.5a).
rem Assumes:
rem   C: = WATCOM_9.5a root (mounted by run script)
rem   D: = working dir containing test_audio.c + ailv3.h + ailv3.lib + fd2common.lib
rem Outputs into D:: test_audio.obj test_aud.exe test_aud.map

set WATCOM=c:\
set PATH=c:\binb;c:\bin
set INCLUDE=c:\h
set LIB=c:\lib386;c:\lib386\dos

d:
cd \

rem 1. Compile - smoke.c if present (smoke test), else test_audio.c (full).
rem    -bt=dos     target DOS
rem    -mf         flat model 32-bit
rem    -fpi87      387 FPU inline
rem    -3s         386 stack-call cdecl ABI - matches FD2.LE (CLIB3S)
rem    -zq -zp1 -d0
rem    -fo=        force output filename (avoids stale 'rename' overwrite race
rem                where COMMAND.COM silently fails if dst already exists)
if exist test_audio.obj del test_audio.obj
if not exist smoke.c goto build_full
c:\bin\wcc386 -bt=dos -mf -fpi87 -3s -zq -zp1 -d0 -i=. -i=c:\h -fo=test_audio.obj smoke.c > wcc386.log
if errorlevel 1 goto compile_failed
goto link_step

:build_full
c:\bin\wcc386 -bt=dos -mf -fpi87 -3s -zq -zp1 -d0 -i=. -i=c:\h -fo=test_audio.obj tau.c > wcc386.log
if errorlevel 1 goto compile_failed

:link_step
rem Consolidated mode: link fd2common.lib directly (real DPMI lock/unlock).
c:\bin\wlink @taudio.lnk > wlink.log
if errorlevel 1 goto link_failed

echo BUILD_OK > build_result.txt
goto end

:compile_failed
echo COMPILE_FAILED > build_result.txt
goto end

:link_failed
echo LINK_FAILED > build_result.txt
goto end

:end
