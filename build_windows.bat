call :set_variable BUILD_TYPE Release %BUILD_TYPE%
call :set_variable MSVC_VERSION "Visual Studio 17 2022" %MSVC_VERSION%

echo BUILD_TYPE:  %BUILD_TYPE%
echo MSVC_VERSION: %MSVC_VERSION%

rmdir /q /s bin
rmdir /q /s lib

if exist build (
  rem build folder already exists.
) else (
  mkdir build
)

cd build

set PLATFORM=x64

cmake -G "%MSVC_VERSION%" --preset %BUILD_TYPE%-windows ..

if %errorlevel% neq 0 exit /b 1

msbuild machinist.sln /m /p:Configuration=%BUILD_TYPE% /p:Platform=%PLATFORM%
msbuild INSTALL.vcxproj /m:%NUMBER_OF_PROCESSORS% /p:Configuration=%BUILD_TYPE% /p:Platform=%PLATFORM%

if %errorlevel% neq 0 exit /b 1

cd ..

EXIT /B 0

:set_variable
set %~1=%~2