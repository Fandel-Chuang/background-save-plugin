@echo off
echo Building Background Save Plugin Test Suite...
echo.

REM Create build directory
if not exist build mkdir build
cd build

echo Compiling source files...

REM Compile the main library
gcc -c ../src/background_save.c -I../include -o background_save.o
if errorlevel 1 (
    echo ERROR: Failed to compile background_save.c
    pause
    exit /b 1
)

REM Compile test utilities
gcc -c ../tests/test_utils.c -I../include -I../tests -o test_utils.o
if errorlevel 1 (
    echo ERROR: Failed to compile test_utils.c
    pause
    exit /b 1
)

REM Compile getopt (for Windows)
gcc -c ../tests/getopt.c -I../tests -o getopt.o
if errorlevel 1 (
    echo ERROR: Failed to compile getopt.c
    pause
    exit /b 1
)

REM Compile test main
gcc -c ../tests/test_main.c -I../include -I../tests -o test_main.o
if errorlevel 1 (
    echo ERROR: Failed to compile test_main.c
    pause
    exit /b 1
)

echo Linking test suite...

REM Link the test suite
gcc test_main.o test_utils.o getopt.o background_save.o -o test_suite.exe -lm
if errorlevel 1 (
    echo ERROR: Failed to link test suite
    pause
    exit /b 1
)

REM Compile functional test
gcc -c ../tests/test_functional.c -I../include -I../tests -o test_functional.o
if errorlevel 1 (
    echo ERROR: Failed to compile test_functional.c
    pause
    exit /b 1
)

echo Linking functional test...

REM Link the functional test
gcc test_functional.o test_utils.o background_save.o -o test_functional.exe -lm
if errorlevel 1 (
    echo ERROR: Failed to link functional test
    pause
    exit /b 1
)

REM Compile benchmark test
gcc -c ../tests/test_benchmark.c -I../include -I../tests -o test_benchmark.o
if errorlevel 1 (
    echo ERROR: Failed to compile test_benchmark.c
    pause
    exit /b 1
)

echo Linking benchmark test...

REM Link the benchmark test
gcc test_benchmark.o test_utils.o background_save.o -o test_benchmark.exe -lm
if errorlevel 1 (
    echo ERROR: Failed to link benchmark test
    pause
    exit /b 1
)

echo.
echo Build successful!
echo Test executables:
echo   - build\test_suite.exe (Test suite framework)
echo   - build\test_functional.exe (Functional tests)
echo   - build\test_benchmark.exe (Performance benchmarks)
echo   - build\rdb_inspector.exe (RDB file inspector)
echo.

REM Run the test suite
echo Running test suite...
echo.
test_suite.exe --all --verbose

echo.
echo Running functional tests...
echo.
test_functional.exe

pause
