param(
    [Parameter(Mandatory=$true)][string]$PlatformRoot,
    [Parameter(Mandatory=$true)][string]$PreparedDirectory,
    [Parameter(Mandatory=$true)][string]$ConsoleIsoPath,
    [string]$AppRoot = '/mnt/usb0/homebrew/PPSA77025',
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$LinkName = 'native-link',
    [string]$RuntimeDirectory = 'build/ps5-runtime'
)
# Private real-game native application link. Never installs, starts or publishes.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if ($AppRoot -notmatch '^/mnt/usb[0-9]/homebrew/[A-Za-z0-9_-]+$' -or
        $ConsoleIsoPath -notmatch '^/mnt/usb[0-9]/[^\x00-\x1f\x7f\\:]+$' -or
        $ConsoleIsoPath.Split('/') -contains '..') { throw 'Invalid selected USB paths' }
    $base = (Resolve-Path -LiteralPath $PlatformRoot).Path.Replace('\','/')
    $prepared = (Resolve-Path -LiteralPath $PreparedDirectory).Path.Replace('\','/')
    $runtime = (Resolve-Path -LiteralPath $RuntimeDirectory).Path.Replace('\','/')
    $sdk = "$base/deps/ps5-payload-sdk/target"
    $radvSdk = "$base/deps/radv-sdk/ps5-payload-sdk/target"
    $llvm = "$base/deps/llvm/bin"
    $psxs = "$base/upstream/PSXS5-5c17ea31aec5791c12451e915386fa98d670adb4"
    $platform = "$base/upstream/PS5_PayloadSDK-b83202be73e930050e00de0f1b4d9ec46c0391bf"
    & python tools/verify_ps5_prepared.py $prepared
    if ($LASTEXITCODE) { throw 'Private prepared cohort verification failed' }
    $gameObjects = @(Get-Content "$prepared/PS5-OBJECTS.json" -Raw | ConvertFrom-Json)
    foreach ($object in $gameObjects) {
        if ((Get-FileHash $object.Path).Hash -ne $object.Hash) { throw 'Translated object changed since compilation' }
    }
    $out = "$prepared/$LinkName"
    New-Item -ItemType Directory -Force $out | Out-Null
    # Selected paths are a private installation plan, not observed console files.
    $config = "#define SR_PS5_APP_ROOT `"$AppRoot`"`n#define SR_PS5_ISO_PATH `"$ConsoleIsoPath`"`n"
    [IO.File]::WriteAllText("$out/ps5_app_config.h", $config)
    $flags = @('--target=x86_64-sie-ps5', "--sysroot=$sdk", '-isystem', "$sdk/include",
        '-I', "$base/attempt-resolution-mods-20261031/install-sdl/include", '-I', "$base/deps/radv-release/include",
        '-I', "$platform/platform/include", '-I', $runtime, '-I', $out, '-Iruntime/src/rt',
        '-DSR_NATIVE_GAME', '-DSR_PS5_SELECTED_APP', '-DSR_SDL3VK', '-DSR_VK_DISPATCH',
        '-DSR_FIBER_SYSV', '-DSR_PS5_DIRECT_STACKS', '-DSR_PS5_DIRECTORY_PROVIDER',
        '-std=gnu11', '-O2', '-g', '-fPIC', '-fno-stack-protector', '-fno-plt', '-femulated-tls',
        '-ffunction-sections', '-fdata-sections')
    function Compile([string]$source, [string]$name, [string[]]$extra = @()) {
        & "$llvm/clang.exe" @flags @extra -c $source -o "$out/$name.o" *> "$out/$name.log"
        if ($LASTEXITCODE) { Get-Content "$out/$name.log" -Tail 20; throw "Link-support compile failed: $name" }
    }
    Compile 'runtime/src/rt/platform/driver_ps5.c' 'driver_ps5'
    Compile 'runtime/src/rt/platform/fiber_sysv.c' 'fiber_sysv'
    Compile 'runtime/src/rt/platform/fiber_sysv.S' 'fiber_switch'
    Compile 'runtime/src/rt/platform/native_optional.c' 'native_optional'
    Compile 'runtime/src/rt/platform/h264_ffmpeg.c' 'h264_ffmpeg' @('-Ibuild/ffmpeg/install/include')
    foreach ($name in @('emutls','udivmodti4','udivti3','divti3','modti3','umodti3','x86')) {
        Compile "$base/deps/compiler-rt/$name.c" $name
    }
    foreach ($name in @('app_crt','app_cpp_runtime')) {
        & "$llvm/clang++.exe" -isystem "$sdk/include/c++/v1" @flags -std=c++20 -fno-exceptions -fno-rtti `
            -c "$psxs/tooling/native/$name.cpp" -o "$out/$name.o" *> "$out/$name.log"
        if ($LASTEXITCODE) { Get-Content "$out/$name.log" -Tail 20; throw "Native application C++ support failed: $name" }
    }
    $stockRadv = "$base/deps/radv-release/lib/libvulkan_radeon.ps5.a"
    if ((Get-FileHash $stockRadv).Hash.ToLowerInvariant() -ne '39921c4e1c19fecc5c7914e022cd10be01e0325ca990cc027960a1777cf85b96' -or
        (Get-FileHash "$base/attempt-resolution-mods-20261031/install-sdl/lib/libSDL3.a").Hash.ToLowerInvariant() -ne
        '0acbbede2b256cce605dfaa788b3ce1d9989588dad61426b2014a685a63f5dbf') {
        throw 'Selected public RADV/SDL archive differs from pinned build'
    }
    $privateRadv = "$out/radv-native.a"
    $renames = @('opendir','fdopendir','readdir','rewinddir','dirfd','closedir') | ForEach-Object {"--redefine-sym=$_=ps5_$_"}
    & "$llvm/llvm-objcopy.exe" @renames $stockRadv $privateRadv
    if ($LASTEXITCODE) { throw 'Private RADV derivation failed' }
    @{
        input=(Get-FileHash $stockRadv).Hash; output=(Get-FileHash $privateRadv).Hash
        tool=(Get-FileHash "$llvm/llvm-objcopy.exe").Hash; renames=$renames; removedMembers=@()
    } | ConvertTo-Json | Set-Content "$out/RADV-DERIVATION.json"
    Copy-Item "$sdk/lib/libc.a" "$out/libc-native.a" -Force
    & "$llvm/llvm-ar.exe" d "$out/libc-native.a" dlfcn.o
    if ($LASTEXITCODE) { throw 'Native libc derivation failed' }
    $args = @('-m','elf_x86_64','-pie','--no-dynamic-linker','--eh-frame-hdr','-z','max-page-size=0x4000',
        '--gc-sections','--exclude-libs=ALL','--error-limit=0',"-L$psxs/tooling/native",'-T',"$psxs/tooling/psbc/ps5-pie-unwind.ld",
        "--version-script=$psxs/tooling/native/app-symbols.map",'-e','_start')
    # Public ps5platform compatibility symbols only, not PE game adapters.
    $template = Get-Content "$base/../public-build21-source-20261106/support/LINK-ARGS.template.rsp"
    $args += @($template | ForEach-Object { $_.Trim('"') } | Where-Object { $_ -match '^--defsym=[^=]+=ps5_' })
    $args += '--defsym=readlink=sr_native_readlink'
    foreach ($name in @('opendir','fdopendir','readdir','rewinddir','dirfd','closedir')) { $args += "--defsym=$name=ps5_$name" }
    foreach ($name in @('malloc','calloc','realloc','free','posix_memalign','aligned_alloc','memalign','malloc_usable_size',
        'reallocf','reallocarray','getline','getdelim','pthread_create','pthread_join','pthread_detach','sceVideoOutSubmitFlip','sceVideoOutGetFlipStatus')) { $args += "--wrap=$name" }
    $args += @($gameObjects | ForEach-Object { $_.Path.Replace('\','/') })
    $args += @(Get-ChildItem "$runtime/*.o" | Where-Object {$_.BaseName -ne 'platform_driver_ps5' -and $_.BaseName -ne 'h264_ffmpeg'} | ForEach-Object {$_.FullName.Replace('\','/')})
    $args += @(Get-ChildItem "$out/*.o" | ForEach-Object {$_.FullName.Replace('\','/')})
    $args += @(Get-ChildItem build/ps5-at3/*.o | ForEach-Object {$_.FullName.Replace('\','/')})
    $args += "$base/attempt-resolution-mods-20261031/install-sdl/lib/libSDL3.a"
    $args += @('--whole-archive',$privateRadv,'--no-whole-archive','--start-group',
        'build/ffmpeg/install/lib/libavcodec.a','build/ffmpeg/install/lib/libswscale.a','build/ffmpeg/install/lib/libavutil.a',
        "$radvSdk/lib/libc++.a","$radvSdk/lib/libc++abi.a","$radvSdk/lib/libunwind.a","$radvSdk/lib/libps5platform.a",'--end-group',
        "-L$sdk/lib",'--as-needed','-lc_stub_weak','-lkernel','-lSceLibcInternal','-lSceVideoOut','-lSceAudioOut',
        '-lSceUserService','-lScePad','-lSceSysmodule','-lSceNet','-lSceAgc','-lSceAgcDriver',"$out/libc-native.a",'-o',"$out/third-birthday-native.elf")
    [IO.File]::WriteAllLines("$out/LINK.rsp", @($args | ForEach-Object {'"' + $_ + '"'}))
    & "$llvm/ld.lld.exe" "@$out/LINK.rsp" *> "$out/LINK.log"
    if ($LASTEXITCODE) { Get-Content "$out/LINK.log" -Tail 50; throw 'Real native game link failed; inspect exact unresolved providers' }
    & python tools/verify_ps5_prepared.py $prepared
    if ($LASTEXITCODE) { throw 'Private game cohort changed during application link' }
    Get-FileHash "$out/third-birthday-native.elf" | ConvertTo-Json | Set-Content "$out/HASH.json"
    Write-Output 'Real native game ELF linked. Provider/runtime/conversion audits and human play remain required.'
} finally { Pop-Location }
