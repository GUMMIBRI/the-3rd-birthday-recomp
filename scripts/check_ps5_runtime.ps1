param([Parameter(Mandatory=$true)][string]$PlatformRoot)
# Coherent native runtime batch. This is not a game link or gameplay check.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $base = (Resolve-Path -LiteralPath $PlatformRoot).Path
    $sdk = "$base/deps/ps5-payload-sdk/target"
    $sdl = "$base/attempt-resolution-mods-20261031/install-sdl/include"
    $platform = "$base/upstream/PS5_PayloadSDK-b83202be73e930050e00de0f1b4d9ec46c0391bf/platform/include"
    $out = 'build/ps5-runtime'
    New-Item -ItemType Directory -Force $out | Out-Null
    & python tools/toml_addrs.py versions/ULUS10567.toml "$out/game_addrs.h"
    if ($LASTEXITCODE) { throw 'Address metadata generation failed' }
    $flags = @('--target=x86_64-sie-ps5', "--sysroot=$sdk", '-isystem', "$sdk/include",
        '-I', $sdl, '-I', "$base/deps/radv-release/include", '-I', $platform,
        '-I', $out, '-Iruntime/src/rt', '-DSR_SDL3VK', '-DSR_VK_DISPATCH',
        '-DSR_FIBER_SYSV', '-DSR_PS5_DIRECT_STACKS', '-DSR_PS5_DIRECTORY_PROVIDER', '-DSR_NATIVE_GAME', '-D__PS5__', '-std=gnu11',
        '-O2', '-fPIC', '-fno-stack-protector', '-fno-plt', '-femulated-tls',
        '-ffunction-sections', '-fdata-sections')
    $units = @('recomp', 'vfpu_interp', 'ge', 'iso', 'hle', 'hle_ext', 'sched', 'atrac_hle', 'sas_hle', 'mpeg', 'pgf', 'twinstick', 'cheats', 'mods', 'savedata',
        'gpu_sdl3vk/sdl3vk', 'gpu_sdl3vk/ge_gpu', 'gpu_sdl3vk/texpack_ppsspp',
        'gpu_sdl3vk/vk_dispatch', 'gpu_sdl3vk/portable_text',
        'platform/host_dir', 'platform/gui_sdl', 'platform/osk_sdl', 'platform/png', 'platform/audio_sdl', 'platform/driver_ps5')
    $results = @()
    foreach ($unit in $units) {
        $name = $unit.Replace('/', '_')
        $output = & "$base/deps/llvm/bin/clang.exe" @flags -c "runtime/src/rt/$unit.c" -o "$out/$name.o" 2>&1
        $exit = $LASTEXITCODE
        $output | Set-Content "$out/$name.log"
        $results += @{source=$unit; compiled=($exit -eq 0); exitCode=$exit}
    }
    $results | ConvertTo-Json | Set-Content "$out/COMPILE-RESULTS.json"
    # The public directory provider uses a different retained SDK structure ABI.
    # Only its scalar adapter crosses into the main runtime, never DIR/stat types.
    $radv = "$base/deps/radv-sdk/ps5-payload-sdk/target"
    & "$base/deps/llvm/bin/clang.exe" --target=x86_64-sie-ps5 "--sysroot=$radv" `
        -isystem "$radv/include" -I $platform -std=gnu11 -O2 -fPIC -fno-stack-protector `
        -c runtime/src/rt/platform/native_directory_ps5.c -o "$out/platform_native_directory_ps5.o" *> "$out/native-directory.log"
    if ($LASTEXITCODE) { throw 'Native directory scalar provider compilation failed' }
    $failed = @($results | Where-Object { !$_.compiled })
    if ($failed.Count) { throw ('Runtime compilation failures: ' + (($failed | ForEach-Object {$_.source}) -join ', ')) }
    Write-Output "$($results.Count) native runtime translation units compiled; game link remains pending."
} finally { Pop-Location }
