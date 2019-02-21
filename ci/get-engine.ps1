. "$PSScriptRoot\common.ps1"

pushd "$($gdk_home)"

    # Fetch the version of Unreal Engine we need
    pushd "ci"
        # Allow users to override the engine version if required
        if (Test-Path env:ENGINE_COMMIT_HASH)
        {
            $unreal_version = (Get-Item -Path env:ENGINE_COMMIT_HASH).Value
            Write-Log "Using engine version defined by ENGINE_COMMIT_HASH: $($unreal_version)"
        } else {
            $unreal_version = Get-Content -Path "unreal-engine.version" -Raw
            Write-Log "Using engine version found in unreal-engine.version file: $($unreal_version)"
        }
    popd

    ## Create an UnrealEngine directory if it doesn't already exist
    New-Item -Name "UnrealEngine" -ItemType Directory -Force

    Start-Event "download-unreal-engine" "build-unreal-gdk-:windows:"
    pushd "UnrealEngine"
        $engine_gcs_path = "gs://$($gcs_publish_bucket)/$($unreal_version).zip"
        Write-Log "Downloading Unreal Engine artifacts from $($engine_gcs_path)"

        $gsu_proc = Start-Process -Wait -PassThru -NoNewWindow "gsutil" -ArgumentList @(`
            "cp", `
            "$($engine_gcs_path)", `
            "$($unreal_version).zip" `
        )
        if ($gsu_proc.ExitCode -ne 0) {
            Write-Log "Failed to download Engine artifacts. Error: $($gsu_proc.ExitCode)"
            Throw "Failed to download Engine artifacts"
        }

        Write-Log "Unzipping Unreal Engine"
        $zip_proc = Start-Process -Wait -PassThru -NoNewWindow "7z" -ArgumentList @(`
        "x", `  
        "$($unreal_version).zip" `    
        )   
        if ($zip_proc.ExitCode -ne 0) { 
            Write-Log "Failed to unzip Unreal Engine. Error: $($zip_proc.ExitCode)" 
            Throw "Failed to unzip Unreal Engine."  
        }
    popd
    Finish-Event "download-unreal-engine" "build-unreal-gdk-:windows:"

    $unreal_path = "$($gdk_home)\UnrealEngine"
    Write-Log "Setting UNREAL_HOME environment variable to $($unreal_path)"
    [Environment]::SetEnvironmentVariable("UNREAL_HOME", "$($unreal_path)", "Machine")

    $clang_path = "$($gdk_home)\UnrealEngine\ClangToolchain"
    Write-Log "Setting LINUX_MULTIARCH_ROOT environment variable to $($clang_path)"
    [Environment]::SetEnvironmentVariable("LINUX_MULTIARCH_ROOT", "$($clang_path)", "Machine")

popd