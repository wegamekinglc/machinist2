call :set_variable BUILD_TYPE Release %BUILD_TYPE%
call :set_variable ADDRESS_MODEL Win64 %ADDRESS_MODEL%
call :set_variable MACHINIST_DIR "%CD%" %MACHINIST_DIR%
call :set_variable MSVC_RUNTIME static %MSVC_RUNTIME%
call :set_variable MSVC_VERSION "Visual Studio 17 2022" %MSVC_VERSION%

echo BUILD_TYPE:  %BUILD_TYPE%
echo MACHINIST_DIR: %MACHINIST_DIR%
echo ADDRESS_MODEL: %ADDRESS_MODEL%
echo MSVC_RUNTIME: %MSVC_RUNTIME%
echo MSVC_VERSION: %MSVC_VERSION%

if exist build (
  rem build folder already exists.
) else (
  mkdir build
)

cd build

if "%ADDRESS_MODEL%"=="Win64" (
  set PLATFORM=x64
) else (
  if "%MSVC_VERSION%"=="Visual Studio 16 2019" (
    set PLATFORM=x64
  ) else (
    if "%MSVC_VERSION%"=="Visual Studio 17 2022" (
        set PLATFORM=x64
    ) else (
        set PLATFORM=Win32
    )
  )
)

if "%ADDRESS_MODEL%"=="Win64" (
  if "%MSVC_VERSION%"=="Visual Studio 16 2019" (
    set ADDRESS_MODEL=
  ) else (
    if "%MSVC_VERSION%"=="Visual Studio 17 2022" (
        set ADDRESS_MODEL=
    )
  )
)

if "%ADDRESS_MODEL%" =="Win64" (
cmake -G "%MSVC_VERSION% %ADDRESS_MODEL%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%MACHINIST_DIR% -DMSVC_RUNTIME=%MSVC_RUNTIME% ..
echo go into this branch
) else (
cmake -G "%MSVC_VERSION%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%MACHINIST_DIR% -DMSVC_RUNTIME=%MSVC_RUNTIME% ..
)

if %errorlevel% neq 0 exit /b 1

msbuild machinist.sln /m /p:Configuration=%BUILD_TYPE% /p:Platform=%PLATFORM%
msbuild INSTALL.vcxproj /m:%NUMBER_OF_PROCESSORS% /p:Configuration=%BUILD_TYPE% /p:Platform=%PLATFORM%

if %errorlevel% neq 0 exit /b 1

cd ..

:set_variable
set %~1=%~2
EXIT /B 0