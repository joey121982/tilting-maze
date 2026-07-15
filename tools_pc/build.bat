@echo off
REM Compileaza maze.exe din sursele C++ (maze.cpp + maze_core.cpp), pentru WINDOWS

setlocal
cd /d "%~dp0"

set SRC=maze.cpp ..\common\maze_core.cpp

where g++ >nul 2>nul
if not errorlevel 1 (
    echo Compilez cu g++ ...
    g++ -std=c++14 -O2 -o maze.exe %SRC%
    if not errorlevel 1 goto ok
    echo g++ a esuat, incerc urmatorul compilator.
)

where clang++ >nul 2>nul
if not errorlevel 1 (
    echo Compilez cu clang++ ...
    clang++ -std=c++14 -O2 -o maze.exe %SRC%
    if not errorlevel 1 goto ok
    echo clang++ a esuat, incerc urmatorul compilator.
)

where cl >nul 2>nul
if not errorlevel 1 (
    echo Compilez cu cl ^(MSVC^) ...
    cl /nologo /std:c++14 /EHsc /O2 /Fe:maze.exe %SRC%
    if not errorlevel 1 goto ok
)

echo.
echo EROARE: nu am gasit niciun compilator C++ ^(g++ / clang++ / cl^).
echo Instaleaza MinGW-w64 ^(g++^) sau Visual Studio Build Tools, apoi ruleaza din nou.
echo Detalii: EXPLICATIE_MAZE_GENERATOR.md, sectiunea 9.
exit /b 1

:ok
if exist maze.obj del maze.obj
if exist maze_core.obj del maze_core.obj
echo.
echo OK: maze.exe construit in %cd%
echo Test rapid:  maze.exe --seed 42
exit /b 0
