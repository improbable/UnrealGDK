param(
  [string] $gdk_home = "$($PSScriptRoot)/..", ## The root of the UnrealGDK repo
  [string] $gcs_publish_bucket = "io-internal-infra-unreal-artifacts-production"
)

$ErrorActionPreference = 'Stop'

function Write-Log() {
  param(
    [string] $msg,
    [Parameter(Mandatory=$false)] [bool] $expand = $false
  )
  if ($expand) {
      Write-Output "+++ $($msg)"
  } else {
      Write-Output "--- $($msg)"
  }
}

Write-Output "Starting Unreal GDK build pipeline.."

pushd "$($gdk_home)"

    # Fetch the version of Unreal Engine we need
    pushd "ci"
        $unreal_version = Get-Content -Path "unreal-engine.version" -Raw
        Write-Log "Using Unreal Engine version $($unreal_version)"
    popd

    Write-Log "Create an UnrealEngine directory if it doesn't already exist"
    New-Item -Name "UnrealEngine" -ItemType Directory -Force

    pushd "UnrealEngine"
        Write-Log "Downloading the Unreal Engine artifacts from GCS"
        $gcs_unreal_location = "$($unreal_version).zip"

        $gsu_proc = Start-Process -Wait -PassThru -NoNewWindow "gsutil" -ArgumentList @(`
            "cp", `
            "gs://$($gcs_publish_bucket)/$($gcs_unreal_location)", `
            "$($unreal_version).zip" `
        )
        if ($gsu_proc.ExitCode -ne 0) {
            Write-Log "Failed to download Engine artifacts. Error: $($gsu_proc.ExitCode)"
            Throw "Failed to download Engine artifacts"
        }

        Write-Log "Unzipping Unreal Engine"
        Expand-Archive -Path "$($unreal_version).zip" -Force

    popd

    $unreal_path = "$($gdk_home)\UnrealEngine\$($unreal_version)\"
    Write-Log "Setting UNREAL_HOME environment variable to $($unreal_path)"
    [Environment]::SetEnvironmentVariable("UNREAL_HOME", "$($unreal_path)", "Machine")

    ## THIS REPLACES THE OLD SETUP.BAT SCRIPT

    # TODO: check for msbuild
    #call "%UNREAL_HOME%\Engine\Build\BatchFiles\GetMSBuildPath.bat"

    # Setup variables
    $root_dir = (get-item "$($PSScriptRoot)").parent.FullName
    $pinned_core_sdk_version = Get-Content -Path "$($root_dir)\SpatialGDK\Extras\core-sdk.version" -Raw
    $build_dir = "$($root_dir)\SpatialGDK\Build"
    $core_sdk_dir = "$($build_dir)\core_sdk"
    $worker_sdk_dir = "$($root_dir)\SpatialGDK\Source\SpatialGDK\Public\WorkerSDK"
    $worker_sdk_dir_old = "$($root_dir)\SpatialGDK\Source\Public\WorkerSdk"
    $binaries_dir = "$($root_dir)\SpatialGDK\Binaries\ThirdParty\Improbable"

    Write-Log "Creating folders.."
    New-Item -Path "$($worker_sdk_dir)" -ItemType Directory -Force
    New-Item -Path "$($core_sdk_dir)\schema" -ItemType Directory -Force
    New-Item -Path "$($core_sdk_dir)\tools" -ItemType Directory -Force
    New-Item -Path "$($core_sdk_dir)\worker_sdk" -ItemType Directory -Force
    New-Item -Path "$($binaries_dir)" -ItemType Directory -Force

    Write-Log "Downloading spatial packages.."
    Start-Process -Wait -PassThru -NoNewWindow -FilePath "spatial" -ArgumentList @(`
        "package", `
        "retrieve", `
        "worker_sdk", `
        "c-dynamic-x86_64-msvc_md-win32", `
        "$($pinned_core_sdk_version)", `
        "$($core_sdk_dir)\worker_sdk\c-dynamic-x86_64-msvc_md-win32.zip" `
    )

    Write-Log "Extracting spatial packages.."
    Expand-Archive -Path "$($core_sdk_dir)\worker_sdk\c-dynamic-x86_64-msvc_md-win32.zip" -DestinationPath "$($binaries_dir)\Win64\" -Force

    # Copy from binaries_dir
    Copy-Item "$($binaries_dir)\Win64\include" "$($worker_sdk_dir)" -Force -Recurse

    # TODO : Build utilities
    #%MSBUILD_EXE% /nologo /verbosity:minimal .\SpatialGDK\Build\Programs\Improbable.Unreal.Scripts\Improbable.Unreal.Scripts.sln /property:Configuration=Release

  <#pushd "SpatialGDK"
  
    # Finally, build the Unreal GDK 
    Write-Log "Build Unreal GDK"
    Start-Process -Wait -PassThru -NoNewWindow -FilePath "$($UNREAL_HOME)\Engine\Build\BatchFiles\RunUAT.bat" -ArgumentList @(`
        "BuildPlugin", `
        " -Plugin=`"$PWD/SpatialGDK.uplugin`"", `
        "-TargetPlatforms=Win64", `
        "-Package=`"$PWD/Intermediate/BuildPackage/Win64`"" `
    )

  popd#>

popd