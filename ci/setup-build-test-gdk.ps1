param(
  [string] $gdk_home = (Get-Item "$($PSScriptRoot)").parent.FullName, ## The root of the UnrealGDK repo
  [string] $gcs_publish_bucket = "io-internal-infra-unreal-artifacts-production/UnrealEngine",
  [string] $msbuild_exe = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin\MSBuild.exe",
  [string] $build_home = (Get-Item "$($PSScriptRoot)").parent.parent.FullName, ## The root of the entire build. Should ultimately resolve to "C:\b\<number>\".
  [string] $unreal_path = "$build_home\UnrealEngine"
)

class TestSuite {
  [ValidateNotNullOrEmpty()][string]$test_repo_url
  [ValidateNotNullOrEmpty()][string]$test_repo_branch
  [ValidateNotNullOrEmpty()][string]$test_repo_relative_uproject_path
  [ValidateNotNullOrEmpty()][string]$test_repo_map
  [ValidateNotNullOrEmpty()][string]$test_project_root
  [ValidateNotNullOrEmpty()][string]$test_results_dir
  [ValidateNotNullOrEmpty()][string]$tests_path
  [bool]$run_with_spatial

  TestSuite([string] $test_repo_url, [string] $test_repo_branch, [string] $test_repo_relative_uproject_path, [string] $test_repo_map, [string] $test_project_root, [string] $test_results_dir, [string] $tests_path, [bool] $run_with_spatial) {
    $this.test_repo_url = $test_repo_url
    $this.test_repo_branch = $test_repo_branch
    $this.test_repo_relative_uproject_path = $test_repo_relative_uproject_path
    $this.test_repo_map = $test_repo_map
    $this.test_project_root = $test_project_root
    $this.test_results_dir = $test_results_dir
    $this.tests_path = $tests_path
    $this.run_with_spatial = $run_with_spatial
  }
}

[string]$test_repo_branch = "feature/ci"
[bool]$do_long_networking_tests = $False

# Allow overriding testing branch via environment variable
if (Test-Path env:TEST_REPO_BRANCH) {
  $test_repo_branch = $env:TEST_REPO_BRANCH
}
# Allow overriding running long networking tests
if ( ((Test-Path env:LONG_NETWORKING_TESTS) -And ($env:LONG_NETWORKING_TESTS -eq "true")) -Or ($env:NIGHTLY_BUILD -eq "true")) {
  $do_long_networking_tests = $True
}

$tests = @(
  [TestSuite]::new("git@github.com:improbable/UnrealGDKEngineNetTest.git", "$test_repo_branch", "Game\EngineNetTest.uproject", "NetworkingMap", "NetworkTestProject", "UnitTestResults", "SpatialGDK", $True),
  [TestSuite]::new("git@github.com:improbable/UnrealGDKEngineNetTest.git", "$test_repo_branch", "Game\EngineNetTest.uproject", "NetworkingMap", "NetworkTestProject", "SpatialNetworkingFastTestResults", "/Game/SpatialNetworkingMap", $True)
)

if($do_long_networking_tests) {
  # Do we want to run the tests on the SpatialNetworkingMap with native networking?
  # $tests += [TestSuite]::new("git@github.com:improbable/UnrealGDKEngineNetTest.git", "$test_repo_branch", "Game\EngineNetTest.uproject", "NetworkingMap", "NetworkTestProject", "VanillaShortNetworkingTestResults", "/Game/SpatialNetworkingMap", $False)
  $tests += [TestSuite]::new("git@github.com:improbable/UnrealGDKEngineNetTest.git", "$test_repo_branch", "Game\EngineNetTest.uproject", "NetworkingMap", "NetworkTestProject", "SpatialNetworkingSlowTestResults", "/Game/NetworkingMap", $True)
  $tests += [TestSuite]::new("git@github.com:improbable/UnrealGDKEngineNetTest.git", "$test_repo_branch", "Game\EngineNetTest.uproject", "NetworkingMap", "NetworkTestProject", "VanillaNetworkingSlowTestResults", "/Game/NetworkingMap", $False)
}

. "$PSScriptRoot\common.ps1"

# Guard against other runs not cleaning up after themselves
foreach ($test in $tests) {
  $test_project_root = $test.test_project_root
  & $PSScriptRoot"\cleanup.ps1" `
    -project_path "$test_project_root"
}

# Download Unreal Engine
Start-Event "get-unreal-engine" "command"
& $PSScriptRoot"\get-engine.ps1" -unreal_path "$unreal_path"
Finish-Event "get-unreal-engine" "command"

# Run the required setup steps
Start-Event "setup-gdk" "command"
& $PSScriptRoot"\setup-gdk.ps1" -gdk_path "$gdk_in_engine" -msbuild_path "$msbuild_exe"
Finish-Event "setup-gdk" "command"

class CachedProject {
  [ValidateNotNullOrEmpty()][string]$test_repo_url
  [ValidateNotNullOrEmpty()][string]$test_repo_branch

  CachedProject([string] $test_repo_url, [string] $test_repo_branch) {
    $this.test_repo_url = $test_repo_url
    $this.test_repo_branch = $test_repo_branch
  }
}

$projects_cached = @()

foreach ($test in $tests) {
  $test_repo_url = $test.test_repo_url
  $test_repo_branch = $test.test_repo_branch
  $test_repo_relative_uproject_path = $test.test_repo_relative_uproject_path
  $test_repo_map = $test.test_repo_map
  $test_project_root = $test.test_project_root
  $test_results_dir = $test.test_results_dir
  $tests_path = $test.tests_path
  $run_with_spatial = $test.run_with_spatial

  $project_is_cached = $False
  foreach($cached_project in $projects_cached) {
    if( ($test_repo_url -eq $cached_project.test_repo_url) -and ($test_repo_branch -eq $cached_project.test_repo_branch) ){
      $project_is_cached = $True
    }
  }

  if(-Not $project_is_cached) {
    # Build the testing project
    Start-Event "build-project" "command"
    & $PSScriptRoot"\build-project.ps1" `
        -unreal_path "$unreal_path" `
        -test_repo_branch "$test_repo_branch" `
        -test_repo_url "$test_repo_url" `
        -test_repo_uproject_path "$build_home\$test_project_root\$test_repo_relative_uproject_path" `
        -test_repo_path "$build_home\$test_project_root" `
        -msbuild_exe "$msbuild_exe" `
        -gdk_home "$gdk_home" `
        -build_platform "$env:BUILD_PLATFORM" `
        -build_state "$env:BUILD_STATE" `
        -build_target "$env:BUILD_TARGET"
    
    $projects_cached += [CachedProject]::new($test_repo_url, $test_repo_branch)
    Finish-Event "build-project" "command"
  }

  # Only run tests on Windows, as we do not have a linux agent - should not matter
  if ($env:BUILD_PLATFORM -eq "Win64" -And $env:BUILD_TARGET -eq "Editor" -And $env:BUILD_STATE -eq "Development") {
    Start-Event "test-gdk" "command"
    & $PSScriptRoot"\run-tests.ps1" `
        -unreal_editor_path "$unreal_path\Engine\Binaries\Win64\UE4Editor.exe" `
        -uproject_path "$build_home\$test_project_root\$test_repo_relative_uproject_path" `
        -test_repo_path "$build_home\$test_project_root" `
        -log_file_path "$PSScriptRoot\$test_project_root\$test_results_dir\tests.log" `
        -report_output_path "$test_project_root\$test_results_dir" `
        -test_repo_map "$test_repo_map" `
        -tests_path "$tests_path" `
        -run_with_spatial $run_with_spatial
    Finish-Event "test-gdk" "command"

    Start-Event "report-tests" "command"
    & $PSScriptRoot"\report-tests.ps1" -test_result_dir "$PSScriptRoot\$test_project_root\$test_results_dir" -target_platform "$env:BUILD_PLATFORM"
    Finish-Event "report-tests" "command"
  }
}

