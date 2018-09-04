@echo off

pushd "%~dp0..\"

call "Build\Scripts\BuildWorkerConfig.bat"

set BUILD_EXE_PATH="Binaries\ThirdParty\Improbable\Programs\Build.exe"

if not exist %BUILD_EXE_PATH% (
	echo Error: Build executable not found! Please run BuildGDK.bat in your SpatialGDK directory to generate it.
	exit /b 1
)

%BUILD_EXE_PATH% %*

popd

exit /b %ERRORLEVEL%
