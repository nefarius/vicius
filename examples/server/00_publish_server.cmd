@echo off
@setlocal

set MYDIR=%~dp0
pushd "%MYDIR%"

where podman > nul 2>&1 && (
    set ce=podman
) || (
    set ce=docker
)

%ce% build -t nefarius.azurecr.io/nefarius-vicius-server:latest .
if %ERRORLEVEL% == 0 (
	%ce% push nefarius.azurecr.io/nefarius-vicius-server:latest
)

popd
endlocal
