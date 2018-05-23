#!/usr/bin/env bash

# Function declarations

error() {
   local SOURCE_FILE=$1
   local LINE_NO=$2
   echo "ERROR: ${SOURCE_FILE}: ${LINE_NO}"
}
trap 'error "${BASH_SOURCE}" "${LINENO}"' ERR

function isLinux() {
  [[ "$(uname -s)" == "Linux" ]];
}

function isMacOS() {
  [[ "$(uname -s)" == "Darwin" ]];
}

function isWindows() {
  ! ( isLinux || isMacOS );
}

function getPlatformName() {
  if isLinux; then
    echo "linux"
  elif isMacOS; then
    echo "mac"
  elif isWindows; then
    echo "windows"
  else
    echo "ERROR: Unknown platform." >&2
    exit 1
  fi
}

function isTeamCity() {
  # -n == string comparison "not null"
  [ -n "${TEAMCITY_CAPTURE_ENV+x}" ]
}

function isNotTeamCity() {
  # -z == string comparison "null, that is, 0-length"
  [ -z "${TEAMCITY_CAPTURE_ENV+x}" ]
}

function unpackTo() {
  local SOURCE=$1
  local TARGET=$2

  mkdir -p "${TARGET}"
  unzip -o -q "${SOURCE}" -d "${TARGET}"
}

function unpackToWithClean() {
  local SOURCE=$1
  local TARGET=$2

  rm -rf "${TARGET}"
  unpackTo "${SOURCE}" "${TARGET}"
}

function runSpatial() {
  local default_flags=(
    "--log_level=debug"
  )

  forceSpatialCliStructureV2
  spatial "$@" "${default_flags[@]}"
}

# Variable declarations

PROGRAMFILES_X86=$(cmd.exe /c "echo %ProgramFiles(x86)%")
MSBUILD="${PROGRAMFILES_X86}\MSBuild\14.0\Bin\MSBuild.exe"

TOOLS_OS="$(getPlatformName)"
IMP_NUGET_VERSION="20180320.121538.4d07aa9573"
IMP_NUGET="$(pwd)/imp-nuget"

GOPATH="$(pwd)/go"

export GOPATH