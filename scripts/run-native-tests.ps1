$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$gcc = "C:\Users\marco\Documents\Playground\tools\winlibs\current\bin\g++.exe"
$buildDir = Join-Path $projectRoot "build-native-tests"
$binary = Join-Path $buildDir "security_helpers_tests.exe"

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$compileArgs = @(
    "-std=c++17",
    "-I.",
    "-I./src",
    "-I./test/test_security_helpers",
    "-I./.pio/libdeps/native/Unity/src",
    "test/test_security_helpers/test_main.cpp",
    "src/security/SecurityHelpers.cpp",
    ".pio/libdeps/native/Unity/src/unity.c",
    "-o",
    $binary
)

& $gcc @compileArgs

& $binary
