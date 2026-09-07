$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$compiler = Join-Path $env:USERPROFILE '.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-g++.exe'
if (!(Test-Path -LiteralPath $compiler)) {
    throw 'Install the project PlatformIO toolchain first, or compile test/fms_policy_test.cpp with any C++14 compiler.'
}
Push-Location $repoRoot
try {
    & $compiler -std=c++14 -Wall -Wextra -Werror -fsyntax-only test/fms_policy_test.cpp
    if ($LASTEXITCODE -ne 0) { throw 'FMS policy tests failed' }
    python -m unittest discover -s test -p 'test_fms_bench.py' -v
    if ($LASTEXITCODE -ne 0) { throw 'Bench fixture tests failed' }
    Write-Output 'Policy assertions and bench fixture tests passed. Hardware bench tests are separate.'
} finally {
    Pop-Location
}
