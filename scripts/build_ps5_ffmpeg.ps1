param(
    [Parameter(Mandatory=$true)][string]$PlatformRoot,
    [string]$GitShell,
    [string]$HostZig = 'C:/.pe/tools/zig/zig-x86_64-windows-0.15.2/zig.exe',
    [ValidateRange(1,2)][int]$Jobs = 2
)
# Public source-only dependency build. No console deployment or game execution.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$source = Join-Path $root 'build/ffmpeg-source'
$build = Join-Path $root 'build/ffmpeg'
$tools = Join-Path $root 'build/media-tools'
$revision = 'f46e514491172d15bd74b4abb1814cd2f05a763e' # FFmpeg n7.1.3
$oldPath = $env:PATH
function PosixPath([string]$path) {
    $normalized = $path.Replace('\','/')
    if ($normalized -notmatch '^([A-Za-z]):/(.*)$') { throw 'Expected absolute Windows tool/workspace path' }
    return '/' + $Matches[1].ToLowerInvariant() + '/' + $Matches[2]
}
function Package([string]$name, [string]$sha) {
    $file = Join-Path $tools $name
    if (!(Test-Path -LiteralPath $file)) {
        try { Invoke-WebRequest "https://repo.msys2.org/msys/x86_64/$name" -OutFile $file }
        catch { throw ('Official public package download failed: ' + $_.Exception.Message) }
    }
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha) {
        throw "Package digest mismatch: $name"
    }
    & tar -xf $file -C $tools
    if ($LASTEXITCODE) { throw "Package extraction failed: $name" }
}
Push-Location $root
try {
    $base = (Resolve-Path -LiteralPath $PlatformRoot).Path
    if (!$GitShell) {
        $gitRoot = Split-Path (Split-Path (Get-Command git).Source -Parent) -Parent
        $GitShell = Join-Path $gitRoot 'usr/bin/sh.exe'
    }
    $GitShell = (Resolve-Path -LiteralPath $GitShell).Path
    $HostZig = (Resolve-Path -LiteralPath $HostZig).Path
    New-Item -ItemType Directory -Force $tools, $build, "$tools/tmp" | Out-Null
    Package 'make-4.4.1-3-x86_64.pkg.tar.zst' 'af0bdba17f06fe037f0194069adaa31a8fe45f1a11381501896aea1fae37bd5d'
    Package 'coreutils-8.32-5-x86_64.pkg.tar.zst' '62dfee1c39fd15f99c39802b35e82643bc14fffc16d6c76d4001caa385ec77e3'
    if (!(Test-Path -LiteralPath $source)) {
        & git clone --no-checkout --depth 1 --branch n7.1.3 https://github.com/FFmpeg/FFmpeg.git $source
        if ($LASTEXITCODE) { throw 'Public FFmpeg checkout failed' }
        & git -C $source config core.autocrlf false
        & git -C $source checkout --detach $revision
        if ($LASTEXITCODE) { throw 'Pinned FFmpeg checkout failed' }
    }
    $head = (& git -C $source rev-parse HEAD).Trim()
    if ($LASTEXITCODE -or $head -ne $revision) { throw 'FFmpeg revision differs from selected release' }
    & git -C $source diff --quiet --no-ext-diff HEAD --
    if ($LASTEXITCODE) { throw 'FFmpeg tracked source content differs; preserve and review separately' }
    $untracked = @(& git -C $source ls-files --others --exclude-standard)
    if ($LASTEXITCODE -or $untracked.Count) { throw 'FFmpeg source has untracked files; preserve and review separately' }
    $script = @'
#!/bin/sh
set -eu
BASE='@BASE@'
ROOT='@ROOT@'
LLVM=$BASE/deps/llvm/bin
SDK=$BASE/deps/ps5-payload-sdk/target
export PATH="$LLVM:$PATH"
export TMPDIR="$ROOT/build/media-tools/tmp"
cd "$ROOT/build/ffmpeg"
"$ROOT/build/ffmpeg-source/configure" \
 --prefix="$ROOT/build/ffmpeg/install" --enable-cross-compile --arch=x86_64 --target-os=freebsd \
 --host-cc='@HOSTZIG@ cc' --host-os=mingw32 \
 --cc="$LLVM/clang.exe" --ld="$LLVM/ld.lld.exe" --ar="$LLVM/llvm-ar.exe" --ranlib="$LLVM/llvm-ranlib.exe" --nm="$LLVM/llvm-nm.exe" \
 --extra-cflags="--target=x86_64-sie-ps5 --sysroot=$SDK -isystem $SDK/include -fPIC -fno-stack-protector -fno-plt -femulated-tls" \
 --extra-ldflags="-m elf_x86_64 -L$SDK/lib -lc -lkernel -lSceLibcInternal -e main" \
 --disable-everything --disable-autodetect --disable-programs --disable-doc --disable-network \
 --disable-avformat --disable-avdevice --disable-avfilter --disable-postproc --disable-swresample \
 --enable-avcodec --enable-avutil --enable-swscale --enable-decoder=h264 --enable-parser=h264 \
 --disable-pthreads --disable-w32threads --disable-os2threads --disable-asm --disable-x86asm \
 --disable-runtime-cpudetect --disable-debug --enable-static --disable-shared
"$ROOT/build/media-tools/usr/bin/make.exe" -j@JOBS@ install
'@
    $script = $script.Replace('@BASE@',(PosixPath $base)).Replace('@ROOT@',(PosixPath $root)).Replace('@HOSTZIG@',(PosixPath $HostZig)).Replace('@JOBS@',"$Jobs").Replace("`r`n","`n")
    foreach ($path in @($base,$root,$HostZig)) { if ($path.Contains("'")) { throw 'Unsupported quote in tool path' } }
    $runner = Join-Path $build 'build-selected.sh'
    [IO.File]::WriteAllText($runner,$script)
    $env:PATH = "$tools/usr/bin;$(Split-Path $GitShell -Parent);$oldPath"
    & $GitShell $runner *> "$build/BUILD.log"
    if ($LASTEXITCODE) { throw 'FFmpeg configure/build failed; inspect build/ffmpeg/BUILD.log and ffbuild/config.log' }
    $archives = @('libavcodec','libavutil','libswscale') | ForEach-Object {
        $file = "$build/install/lib/$_.a"
        if (!(Test-Path -LiteralPath $file)) { throw "Missing target archive: $_" }
        @{name="$_.a";bytes=(Get-Item $file).Length;sha256=(Get-FileHash $file).Hash.ToLowerInvariant()}
    }
    @{
        repository='https://github.com/FFmpeg/FFmpeg';tag='n7.1.3';revision=$revision
        target='x86_64-sie-ps5';license='LGPL-2.1-or-later (GPL/version3/nonfree not enabled)'
        configureSha256=(Get-FileHash "$source/configure").Hash.ToLowerInvariant()
        selectedScriptSha256=(Get-FileHash $runner).Hash.ToLowerInvariant()
        targetCompilerSha256=(Get-FileHash "$base/deps/llvm/bin/clang.exe").Hash.ToLowerInvariant()
        targetStdlibHeaderSha256=(Get-FileHash "$base/deps/ps5-payload-sdk/target/include/stdlib.h").Hash.ToLowerInvariant()
        hostCompilerSha256=(Get-FileHash $HostZig).Hash.ToLowerInvariant()
        archives=$archives;decoder='h264';parser='h264';threads=$false;assembly=$false
        gameExecuted=$false;consoleExecuted=$false;sourcePatch=$false
    } | ConvertTo-Json -Depth 6 | Set-Content "$build/RESULT.json"
    Write-Output 'Native H264 dependency archives built; decoding and final application link remain unverified.'
} finally {
    $env:PATH = $oldPath
    Pop-Location
}
