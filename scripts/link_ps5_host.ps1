param(
    [Parameter(Mandatory=$true)][string]$PlatformRoot,
    [string]$OutputDirectory = 'build/ps5-app-host'
)
# Native application linkage milestone; never deploys or launches.
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
Push-Location $repoRoot
try {
    $base = (Resolve-Path -LiteralPath $PlatformRoot).Path
    $llvm = "$base/deps/llvm/bin"
    $sdk = "$base/deps/ps5-payload-sdk/target"
    $platform = "$base/upstream/PS5_PayloadSDK-b83202be73e930050e00de0f1b4d9ec46c0391bf"
    $psxs = "$base/upstream/PSXS5-5c17ea31aec5791c12451e915386fa98d670adb4"
    $archive = "$base/deps/radv-sdk/ps5-payload-sdk/target/lib/libps5platform.a"
    if ((Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne
        'a6b6f96ec969a3aa1c9de2dc3d0b744cb50bd44c4e2d1398538b52ee9ba3c00e') {
        throw 'Public platform archive does not match selected revision'
    }
    New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
    $out = (Resolve-Path -LiteralPath $OutputDirectory).Path
    $common = @('--target=x86_64-sie-ps5', "--sysroot=$sdk", '-isystem', "$sdk/include",
        '-isystem', "$sdk/include_common", '-I', "$platform/platform/include",
        '-DSR_FIBER_SYSV', '-DSR_PS5_DIRECT_STACKS', '-D__PS5__', '-fPIC',
        '-fno-stack-protector', '-fno-plt', '-femulated-tls', '-ffunction-sections',
        '-fdata-sections', '-O2', '-g', '-Iruntime/src/rt')
    $units = @(
        @('runtime/src/rt/platform/fiber_sysv.c', 'fiber.o'),
        @('runtime/src/rt/platform/fiber_sysv.S', 'switch.o'),
        @('tools/fiber_check.c', 'main.o')
    )
    foreach ($unit in $units) {
        & "$llvm/clang.exe" @common -c $unit[0] -o "$out/$($unit[1])"
        if ($LASTEXITCODE) { throw "Compile failed: $($unit[0])" }
    }
    & "$llvm/clang++.exe" -isystem "$sdk/include/c++/v1" @common -std=c++20 -fno-exceptions -fno-rtti `
        -isystem "$sdk/include/c++/v1" -c "$psxs/tooling/native/app_crt.cpp" -o "$out/app_crt.o"
    if ($LASTEXITCODE) { throw 'Native application CRT compile failed' }
    & "$llvm/ld.lld.exe" -m elf_x86_64 -pie --no-dynamic-linker --eh-frame-hdr `
        -z max-page-size=0x4000 --gc-sections --exclude-libs=ALL `
        -T "$psxs/tooling/psbc/ps5-pie-unwind.ld" `
        "--version-script=$psxs/tooling/native/app-symbols.map" -e _start `
        "-L$psxs/tooling/native" "-L$sdk/lib" "$out/app_crt.o" "$out/fiber.o" "$out/switch.o" "$out/main.o" `
        $archive -lc -lkernel -lSceLibcInternal -o "$out/third-birthday-host.elf"
    if ($LASTEXITCODE) { throw 'Native host application link failed' }
    & "$llvm/llvm-readelf.exe" --dynamic --dyn-syms --program-headers "$out/third-birthday-host.elf" |
        Set-Content "$out/IMPORTS-SEGMENTS.txt"
    & "$llvm/llvm-objdump.exe" -d "$out/third-birthday-host.elf" | Set-Content "$out/DISASSEMBLY.txt"
    Get-FileHash "$out/third-birthday-host.elf" | ConvertTo-Json | Set-Content "$out/HASH.json"
    Write-Output 'Native host app linked. Conversion, provider audit and runtime are not implied.'
} finally { Pop-Location }
