@echo off
rem nib build - MSVC (VS2022), static CRT, /W4 /WX, C++20.
rem ACT I GATE (CLAUDE.md rule 2): no network DLL among the dependents. The build FAILS if one
rem appears. Act II is where this gate narrows rather than disappears - see BLUEPRINT section 7.
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
rem llama.cpp: headers and import libs from auricle's third_party, DLLs delay-loaded from
rem C:\llama.cpp at run time. Stage 1b is the first stage that needs a model; every stage below
rem Stage 2 must still RUN without one, which is why the DLLs are delay-loaded and --selftest
rem never touches a llama symbol (CLAUDE.md: a battery that needs a 9B stops being run).
set LLAMA=C:\auricle\third_party\llama.cpp
if not exist "%LLAMA%\lib\llama.lib" ( echo build: FAIL - llama import libs not found at %LLAMA%\lib & exit /b 1 )
set CXXFLAGS=/nologo /c /std:c++20 /O2 /W4 /WX /permissive- /EHsc /utf-8 /MT /Zc:__cplusplus /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /Isrc /I"%AURICLE%" /I"%LLAMA%\include"
cl %CXXFLAGS% src\nib.cpp src\changeset.cpp src\doc.cpp src\edit.cpp src\ingest.cpp src\resident.cpp src\selftest.cpp || exit /b 1
link /nologo /SUBSYSTEM:CONSOLE /OUT:nib.exe nib.obj changeset.obj doc.obj edit.obj ingest.obj resident.obj selftest.obj ^
  "%LLAMA%\lib\llama.lib" "%LLAMA%\lib\ggml.lib" "%LLAMA%\lib\ggml-base.lib" ^
  kernel32.lib user32.lib gdi32.lib comdlg32.lib delayimp.lib ^
  /DELAYLOAD:llama.dll /DELAYLOAD:ggml.dll /DELAYLOAD:ggml-base.dll || exit /b 1
rem All three are DELAY-loaded so that --selftest, --ingest and the editor still run on a machine
rem with no llama.cpp and no model: nothing touches a llama symbol until --resident asks for one.
rem ggml-base carries the backend-device enumeration (ggml_backend_dev_name/type), which is how a
rem silent CPU fallback is caught, so it is imported from directly and must be named here.
dumpbin /nologo /dependents nib.exe > build-dependents.txt || exit /b 1
findstr /i /c:"ws2_32" /c:"wininet" /c:"winhttp" /c:"urlmon" /c:"dnsapi" build-dependents.txt >nul
if not errorlevel 1 (
  echo build: FAIL - a network DLL is among the dependents:
  findstr /i /c:"ws2_32" /c:"wininet" /c:"winhttp" /c:"urlmon" /c:"dnsapi" build-dependents.txt
  exit /b 1
)
del build-dependents.txt
echo OK: nib.exe  (gate: no network DLL)
exit /b 0
