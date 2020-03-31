#!/bin/bash
set -euo pipefail

upload_build_configuration_step() {
    export ENGINE_COMMIT_HASH="${1}"
    export BUILD_PLATFORM="${2}"
    export BUILD_TARGET="${3}"
    export BUILD_STATE="${4}"

    if [[ ${BUILD_PLATFORM} == "Mac" ]]; then
        export BUILD_COMMAND="./ci/setup-build-test-gdk.sh"
        REPLACE_STRING="s|BUILDKITE_AGENT_PLACEHOLDER|macos|g;"
    else
        export BUILD_COMMAND="powershell ./ci/setup-build-test-gdk.ps1"
        REPLACE_STRING="s|BUILDKITE_AGENT_PLACEHOLDER|windows|g;"
    fi

    sed "$REPLACE_STRING" "ci/gdk_build.template.steps.yaml" | buildkite-agent pipeline upload
}

generate_build_configuration_steps () {
    # See https://docs.unrealengine.com/en-US/Programming/Development/BuildConfigurations/index.html for possible configurations 
    ENGINE_COMMIT_HASH="${1}"

    if [[ -z "${MAC_BUILD:-}" ]]; then
        # if the BUILD_ALL_CONFIGURATIONS environment variable doesn't exist, then...
        if [[ -z "${BUILD_ALL_CONFIGURATIONS+x}" ]]; then
            echo "Building for subset of supported configurations. Generating the appropriate steps..."

            # Win64 Development Editor build configuration
            upload_build_configuration_step "${ENGINE_COMMIT_HASH}" "Win64" "Editor" "Development"

            # Linux Development NoEditor build configuration
            upload_build_configuration_step "${ENGINE_COMMIT_HASH}" "Linux" "" "Development"
        else
            echo "Building for all supported configurations. Generating the appropriate steps..."

            # Editor builds (Test and Shipping build states do not exist for the Editor build target)
            for BUILD_STATE in "DebugGame" "Development"; do
                upload_build_configuration_step "${ENGINE_COMMIT_HASH}" "Win64" "Editor" "${BUILD_STATE}"
            done
        fi
    else
        if [[ -z "${BUILD_ALL_CONFIGURATIONS+x}" ]]; then
            # MacOS Development Editor build configuration
            upload_build_configuration_step "${ENGINE_COMMIT_HASH}" "Mac" "Editor" "Development"
        else
            # Editor builds (Test and Shipping build states do not exist for the Editor build target)
            for BUILD_STATE in "DebugGame" "Development"; do
                upload_build_configuration_step "${ENGINE_COMMIT_HASH}" "Mac" "Editor" "${BUILD_STATE}"
            done
        fi

    fi
}

# This script generates steps for each engine version listed in unreal-engine.version, 
# based on the gdk_build.template.steps.yaml template
if [[ -z "${ENGINE_VERSION}" ]]; then 
    echo "Generating build steps for each engine version listed in unreal-engine.version"  
    IFS=$'\n'
    for COMMIT_HASH in $(cat < ci/unreal-engine.version); do
        generate_build_configuration_steps "${COMMIT_HASH}"
    done
else
    echo "Generating steps for the specified engine version: ${ENGINE_VERSION}" 
    generate_build_configuration_steps "${ENGINE_VERSION}"
fi;
