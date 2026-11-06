param(
    [Parameter(Mandatory=$true)][string]$Compiler,
    [Parameter(Mandatory=$true)][string]$Sysroot,
    [string]$OutputDirectory = 'build/ps5-host'
)
# Compile source against the selected public PS5 SDK; this does not link an app.
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
Push-Location $repoRoot
try {
    $compilerPath = (Resolve-Path -LiteralPath $Compiler).Path
    $sysrootPath = (Resolve-Path -LiteralPath $Sysroot).Path
    New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
    $common = @('--target=x86_64-sie-ps5', "--sysroot=$sysrootPath",
        '-isystem', "$sysrootPath/include", '-isystem', "$sysrootPath/include_common",
        '-std=c11', '-D__PS5__', '-DSR_FIBER_SYSV', '-fPIC', '-Wall', '-Wextra',
        '-Iruntime/src/rt')
    $units = @(
        @('runtime/src/rt/platform/fiber_sysv.c', 'fiber_sysv.o'),
        @('runtime/src/rt/platform/fiber_sysv.S', 'fiber_switch.o'),
        @('runtime/src/rt/sched.c', 'sched.o'),
        @('tools/fiber_check.c', 'fiber_check.o')
    )
    foreach ($unit in $units) {
        & $compilerPath @common -c $unit[0] -o (Join-Path $OutputDirectory $unit[1])
        if ($LASTEXITCODE -ne 0) { throw "PS5 target compilation failed: $($unit[0])" }
    }
    Write-Output 'PS5 host objects compiled. Application linking and runtime remain unverified.'
} finally {
    Pop-Location
}
