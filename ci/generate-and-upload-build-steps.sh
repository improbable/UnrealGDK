#!/bin/bash
set -euo pipefail

function Generate-Build-Configuration-Steps {
    param( $engine_commit_hash )
    if [ -z "${NIGHTLY_BUILD}" ]; then 
        echo "This is a nightly build. Generating the appropriate steps..."
        for build_target_suffix in "" "Editor" "Server" "SimulatedPlayer"; do
            for build_state in "DebugGame" "Development" "Shipping" "Test"; do
                cat "ci/gdk_build.template.steps.yaml" | \
                sed "s|ENGINE_COMMIT_HASH_PLACEHOLDER|$engine_commit_hash|g" | \
                sed "s|BUILD_TARGET_SUFFIX_PLACEHOLDER|$build_target_suffix|g" | \
                sed "s|BUILD_STATE_PLACEHOLDER|$build_state|g" | \
                buildkite-agent pipeline upload
            done
        done
    else
        echo "This is not a nightly build. Generating the appropriate steps..."
        cat "ci/gdk_build.template.steps.yaml" | \
        sed "s|ENGINE_COMMIT_HASH_PLACEHOLDER|$engine_commit_hash|g" | \
        sed "s|BUILD_TARGET_SUFFIX_PLACEHOLDER|Editor|g" | \
        sed "s|BUILD_STATE_PLACEHOLDER|Development|g" | \
        buildkite-agent pipeline upload
    fi;
}

# This script generates steps for each engine version listed in unreal-engine.version, based on the gdk_build.template.steps.yaml template
if [ -z "${ENGINE_VERSION}" ]; then 
    echo "Generating build steps for each engine version listed in unreal-engine.version"  
    IFS=$'\n'
    for commit_hash in $(cat < ci/unreal-engine.version); do
        Generate-Build-Configuration-Steps -engine_commit_hash "$commit_hash"
    done
else
    echo "Generating steps for the specified engine version: $ENGINE_VERSION" 
    Generate-Build-Configuration-Steps -engine_commit_hash "$ENGINE_VERSION"
fi;
