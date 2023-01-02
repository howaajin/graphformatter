@echo OFF

setlocal

set DES_FILE="%~dp0\GraphFormatter.uplugin"
set BUILD_DIR=%~dp0\Build

if not exist %BUILD_DIR% mkdir %BUILD_DIR% 

call :Build 5.0
call :Build 5.1
goto RET

:Build
set KEY_NAME="HKEY_LOCAL_MACHINE\SOFTWARE\EpicGames\Unreal Engine\%~1"
set VALUE_NAME=InstalledDirectory
FOR /F "usebackq skip=2 tokens=2*" %%A IN (`REG QUERY %KEY_NAME% /v %VALUE_NAME% 2^>nul`) DO ( set UE_PATH=%%B)
if defined UE_PATH (
    @echo Unreal engine %~1 = %UE_PATH%
    @echo Building plugin for Unreal engine %~1...
    call "%UE_PATH%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin=%DES_FILE% -Package="%BUILD_DIR%\%~1"
    where 7z.exe >nul 2>nul
    IF NOT ERRORLEVEL 0 (
        @echo 7z.exe not found.
        @echo skip packaging...
        EXIT /B 0
    )
    7z a "%BUILD_DIR%\GraphFormatter%~1.zip" "%BUILD_DIR%\%~1\Resources" "%BUILD_DIR%\%~1\Source" "%BUILD_DIR%\%~1\GraphFormatter.uplugin"
) else (
    @echo Unreal engine %~1 not found. Skip build for %~1...
    EXIT /B 0
)
EXIT /B 0

:RET