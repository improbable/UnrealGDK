param(
  [string] $gdk_home = (get-item "$($PSScriptRoot)").parent.FullName, ## The root of the UnrealGDK repo
  [string] $gcs_publish_bucket = "io-internal-infra-unreal-artifacts-production/UnrealEngine",
  [string] $msbuild_exe = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin\MSBuild.exe"
)

# Download Unreal Engine 
&$PSScriptRoot"\get-engine.ps1"

# Run the required setup steps
&$PSScriptRoot"\setup-gdk.ps1"

# Build the GDK plugin
&$PSScriptRoot"\build-gdk.ps1"
