[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$matrix = @(
    @{ Version = '7.3.0'; Label = 'gcc-7.3.0' },
    @{ Version = '9.3.0'; Label = 'gcc-9.3.0' }
)

Push-Location $projectRoot
try {
    foreach ($entry in $matrix) {
        $baseImage = "gcc:$($entry.Version)"
        $toolchainImage = "a2ui-qt-toolchain:$($entry.Label)"

        docker image inspect $baseImage *> $null
        if ($LASTEXITCODE -ne 0) {
            throw "Missing $baseImage. Bootstrap the pinned base image first."
        }

        docker build `
            --build-arg "GCC_IMAGE=$baseImage" `
            --tag $toolchainImage `
            --file toolchains/Dockerfile `
            .
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to build $toolchainImage"
        }

        docker run --rm `
            --volume "${projectRoot}:/workspace" `
            --workdir /workspace `
            $toolchainImage `
            sh /workspace/tools/container-build.sh $($entry.Label)
        if ($LASTEXITCODE -ne 0) {
            throw "Build or tests failed for $($entry.Label)"
        }
    }
}
finally {
    Pop-Location
}
