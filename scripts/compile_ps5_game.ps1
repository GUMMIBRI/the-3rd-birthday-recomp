param(
    [Parameter(Mandatory=$true)][string]$PlatformRoot,
    [Parameter(Mandatory=$true)][string]$PreparedDirectory
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $base = (Resolve-Path -LiteralPath $PlatformRoot).Path
    $prepared = (Resolve-Path -LiteralPath $PreparedDirectory).Path
    & python tools/verify_ps5_prepared.py $prepared
    if ($LASTEXITCODE) { throw 'Prepared game cohort failed verification' }
    $sdk = "$base/deps/ps5-payload-sdk/target"
    $objects = "$prepared/ps5-objects"
    New-Item -ItemType Directory -Force $objects | Out-Null
    $flags = @('--target=x86_64-sie-ps5', "--sysroot=$sdk", '-isystem', "$sdk/include",
        '-I', "$prepared/gen", '-Iruntime/src/rt', '-std=gnu11', '-O1',
        '-foptimize-sibling-calls', '-fPIC', '-fno-stack-protector', '-fno-plt',
        '-ffunction-sections', '-fdata-sections')
    # The verifier requires this list to match the manifest exactly; no stale
    # translated chunks or Windows DLL glue can be silently selected.
    $sources = @(Get-ChildItem "$prepared/gen/recomp_*.c" | Sort-Object Name | ForEach-Object {$_.FullName})
    $sources += "$prepared/game_image.c"
    foreach ($source in $sources) {
        & "$base/deps/llvm/bin/clang.exe" @flags -c $source -o "$objects/$([IO.Path]::GetFileNameWithoutExtension($source)).o"
        if ($LASTEXITCODE) { throw "Translated game compilation failed: $source" }
    }
    & python tools/verify_ps5_prepared.py $prepared
    if ($LASTEXITCODE) { throw 'Prepared game changed during compilation' }
    Get-ChildItem "$objects/*.o" | Get-FileHash | ConvertTo-Json | Set-Content "$prepared/PS5-OBJECTS.json"
    Write-Output 'Static game objects compiled from one verified cohort; application link/runtime remain separate.'
} finally { Pop-Location }
