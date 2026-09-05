@echo off
rem nib build - MSVC (VS2022), static CRT, /W4 /WX, C++20.
rem ACT I GATE (CLAUDE.md rule 2): no network DLL among the dependents. The build FAILS if one
rem appears. This is a BUILD-PHASE gate: it ends at Stage 6 (discovery), where SPEC 9.2's narrower
rem gate and the runtime module gate (`nib --about`) replace it - see ROADMAP.
setlocal enabledelayedexpansion
where cl >nul 2>nul
if errorlevel 1 (
  set "VCV=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  if not exist "!VCV!" ( echo build: vcvars64.bat not found & exit /b 1 )
  call "!VCV!" >nul
)
cd /d "%~dp0"
rem The intake seam is auricle's, included UNMODIFIED (SPEC 5.1.1) rather than copied, so the two
rem cannot drift. If C:\auricle is not present, this is the one dependency that breaks the build.
set AURICLE=C:\auricle\src
if not exist "%AURICLE%\fusor\source.h" ( echo build: FAIL - intake seam not found at %AURICLE%\fusor\source.h & exit /b 1 )
if not exist "%AURICLE%\core\ring.h" ( echo build: FAIL - source.h needs core\ring.h, not found under %AURICLE% & exit /b 1 )
rem llama.cpp: headers and import libs from auricle's third_party, DLLs delay-loaded from
rem C:\llama.cpp at run time. Stage 1b is the first stage that needs a model; every stage below
rem Stage 2 must still RUN without one, which is why the DLLs are delay-loaded and --selftest
rem never touches a llama symbol (CLAUDE.md: a battery that needs a 9B stops being run).
set LLAMA=C:\auricle\third_party\llama.cpp
if not exist "%LLAMA%\lib\llama.lib" ( echo build: FAIL - llama import libs not found at %LLAMA%\lib & exit /b 1 )
set CXXFLAGS=/nologo /c /std:c++20 /O2 /W4 /WX /permissive- /EHsc /utf-8 /MT /Zc:__cplusplus /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /Isrc /I"%AURICLE%" /I"%LLAMA%\include"
cl %CXXFLAGS% src\nib.cpp src\changeset.cpp src\doc.cpp src\edit.cpp src\ingest.cpp src\resident.cpp src\tape.cpp src\wire.cpp src\selftest.cpp || exit /b 1
link /nologo /SUBSYSTEM:CONSOLE /OUT:nib.exe nib.obj changeset.obj doc.obj edit.obj ingest.obj resident.obj tape.obj wire.obj selftest.obj ^
  "%LLAMA%\lib\llama.lib" "%LLAMA%\lib\ggml.lib" "%LLAMA%\lib\ggml-base.lib" ^
  kernel32.lib user32.lib gdi32.lib comdlg32.lib bcrypt.lib delayimp.lib ^
  /DELAYLOAD:llama.dll /DELAYLOAD:ggml.dll /DELAYLOAD:ggml-base.dll || exit /b 1
rem All three are DELAY-loaded so that --selftest, --ingest and the editor still run on a machine
rem with no llama.cpp and no model: nothing touches a llama symbol until --resident asks for one.
rem ggml-base carries the backend-device enumeration (ggml_backend_dev_name/type), which is how a
rem silent CPU fallback is caught, so it is imported from directly and must be named here.
rem bcrypt is the platform's SHA-256, for the model's hash on the tape (CLAUDE.md rule 8). It is a
rem hard import and it is not a network DLL; the gate below still names only the five.
dumpbin /nologo /dependents nib.exe > build-dependents.txt || exit /b 1
findstr /i /c:"ws2_32" /c:"wininet" /c:"winhttp" /c:"urlmon" /c:"dnsapi" build-dependents.txt >nul
if not errorlevel 1 (
  echo build: FAIL - a network DLL is among the dependents:
  findstr /i /c:"ws2_32" /c:"wininet" /c:"winhttp" /c:"urlmon" /c:"dnsapi" build-dependents.txt
  exit /b 1
)
rem The delay-load is ASSERTED, not assumed (QC 2026-09-04, F.3): drop a /DELAYLOAD above and the
rem exe would still build, and --selftest would stop running on a machine without C:\llama.cpp.
rem dumpbin lists the hard imports first and the delay-loads after a header line; each of the
rem three must appear, and only after that line.
rem `find` and `more` are named by full path: under Git Bash, cmd's PATH resolves a bare `find`
rem to Git's Unix find, which reads "/c /v" as directories and walks the whole drive (2026-09-04).
set DLINE=
for /f "delims=:" %%L in ('findstr /n /c:"delay load dependencies" build-dependents.txt') do set DLINE=%%L
if not defined DLINE ( echo build: FAIL - no delay-load section: the llama DLLs would be hard imports & exit /b 1 )
%SystemRoot%\System32\more.com +%DLINE% build-dependents.txt > build-delay.txt
for %%D in (llama.dll ggml.dll ggml-base.dll) do (
  findstr /i /c:"%%D" build-delay.txt >nul || ( echo build: FAIL - %%D is not delay-loaded & exit /b 1 )
)
set NALL=
for /f %%C in ('findstr /i /c:"llama.dll" /c:"ggml.dll" /c:"ggml-base.dll" build-dependents.txt ^| %SystemRoot%\System32\find.exe /c /v ""') do set NALL=%%C
if not "%NALL%"=="3" ( echo build: FAIL - a llama DLL appears outside the delay-load list ^(%NALL% mentions^) & exit /b 1 )
del build-delay.txt
del build-dependents.txt
echo OK: nib.exe  (gates: no network DLL; llama delay-loaded)
exit /b 0
