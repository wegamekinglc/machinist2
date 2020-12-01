@echo off

call :set_variable BUILD_TYPE Release
call :set_variable DAL_DIR "%CD%"
call :set_variable ADDRESS_MODEL
call :set_variable MSVC_RUNTIME dynamic
call :set_variable MSVC_VERSION Visual Studio 15 2017

if exist build (
  rem build folder already exists.
) else (
  mkdir build
)

cd build

if "%ADDRESS_MODEL%"=="Win64" (
  if "%MSVC_VERSION%"=="Visual Studio 16 2019" (
    set ADDRESS_MODEL=
  )
)

if "%ADDRESS_MODEL%"=="Win64" (
  set PLATFORM=x64
) else (
  set PLATFORM=Win32
)

if "%ADDRESS_MODEL%" =="Win64" (
cmake -G "%MSVC_VERSION% %ADDRESS_MODEL%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%DAL_DIR% -DMSVC_RUNTIME=%MSVC_RUNTIME% --target install ..
) else (
cmake -G "%MSVC_VERSION%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%DAL_DIR% -DMSVC_RUNTIME=%MSVC_RUNTIME% --target install ..
)

if %errorlevel% neq 0 exit /b 1

msbuild machinist.sln /m /p:Configuration=%BUILD_TYPE% /p:Platform=%PLATFORM%
msbuild INSTALL.vcxproj /m:%NUMBER_OF_PROCESSORS% /p:Configuration=%BUILD_TYPE% /p:Platform=%PLATFORM%

if %errorlevel% neq 0 exit /b 1

cd ..

@echo on

:set_variable
if "%~1%"=="" (
 set ~1=~2
)
echo %~1 is %~2
EXIT /B 0