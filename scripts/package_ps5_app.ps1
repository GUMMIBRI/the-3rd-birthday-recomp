param(
    [Parameter(Mandatory=$true)][string]$PlatformRoot,
    [string]$EbootPath = 'build/private-game-20261106-r2/native-link/converted-r1/eboot.bin',
    [string]$SceAuditPath = 'build/checks/GAME-SCE-VERIFY.json',
    [string]$OutputDirectory = 'build/private-delivery-r3',
    [string]$ParamSchemaReference = 'C:/.pe/ps5-native/release-v1.1.6-build20-20261106/PACKAGE-ROOT/PPSA77022/sce_sys/param.json',
    [switch]$Finalize
)
# Local private staging only. Never uploads, launches, removes or overwrites an app.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$ownedRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/private-delivery-r3'))
function Digest([string]$file) { return (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() }
function JsonFile([string]$file) { return Get-Content -LiteralPath $file -Raw | ConvertFrom-Json }
function Require([bool]$condition, [string]$message) { if (!$condition) { throw $message } }
Push-Location $root
try {
    $base = (Resolve-Path -LiteralPath $PlatformRoot).Path
    $eboot = (Resolve-Path -LiteralPath $EbootPath).Path
    $converted = Split-Path $eboot -Parent
    $sce = Join-Path $converted 'eboot.elf'
    $resultFile = Join-Path $converted 'RESULT.json'
    $result = JsonFile $resultFile
    Require ((Digest $eboot) -eq $result.ebootSHA256.ToLowerInvariant()) 'EBOOT does not match reviewed conversion result'
    Require ((Get-Item $eboot).Length -eq $result.ebootBytes) 'EBOOT length differs from reviewed result'
    Require ((Digest $sce) -eq $result.convertedElfSHA256.ToLowerInvariant()) 'SCE ELF does not match conversion result'
    Require ($result.nativeWritesEnabled -eq $false) 'This candidate requires disabled native save writes'
    Require ($result.selfIntegrityValid -eq $true -and $result.retainedSectionsVerified -eq $true) 'Conversion/SELF checks are incomplete'
    $retained = JsonFile (Join-Path $converted 'RETAINED.json')
    Require ($retained.passed -eq $true -and $retained.retainedSections -gt 0) 'Retained-section gate failed'
    $pre = JsonFile (Join-Path (Split-Path $converted -Parent) 'GAME-AUDIT.json')
    Require ($pre.passed -eq $true -and $pre.sha256 -eq $result.sourceNativeElfSHA256) 'Current native game audit does not match conversion input'
    Require ($pre.inline_syscalls.Count -eq 0 -and $pre.emutls_implementations -eq 1) 'Native runtime gate failed'
    $tool = Join-Path $base 'native-candidates/tool/ps5-native-tool.exe'
    Require ((Digest $tool) -eq '78fdfec756c6e7fa132bd2fbf6eeaea753d9b01349d09a8993bba76eed77fd84') 'Converter digest mismatch'
    $inspection = @(& $tool self --inspect --file $eboot)
    Require ($LASTEXITCODE -eq 0 -and @($inspection | Where-Object { $_ -match '^integrity:\s*valid\s*$' }).Count -eq 1) 'Current SELF inspection failed'
    $schemaFile = (Resolve-Path -LiteralPath $ParamSchemaReference).Path
    Require ((Digest $schemaFile) -eq 'd4081e3ffc47200ae4ad582e65b74e008f9d1630ee144b45c3e168f85d8f52ec') 'Public metadata reference schema changed; review before use'
    $reference = JsonFile $schemaFile
    $schemaKeys = @('ageLevel','applicationCategoryType','applicationDrmType','attribute','attribute2','attribute3','conceptId','contentBadgeType','contentId','contentVersion','downloadDataSize','gameIntent','localizedParameters','masterVersion','requiredSystemSoftwareVersion','sdkVersion','titleId','versionFileUri')
    Require (@(Compare-Object ($schemaKeys | Sort-Object) ($reference.PSObject.Properties.Name | Sort-Object)).Count -eq 0) 'Reference metadata field set is unsupported'
    $iconSource = Join-Path $root 'runtime/assets/ps5/icon0.png'
    Require ((Digest $iconSource) -eq '6bf20bf297ebaad8c7c4361bab1cced973eaaaa9d0d2d5b295134068cc004824') 'Original icon differs from reviewed512x512 asset'
    $iconBytes = [IO.File]::ReadAllBytes($iconSource)
    Require ($iconBytes.Length -ge 33 -and [Convert]::ToHexString($iconBytes[0..7]) -eq '89504E470D0A1A0A') 'Required icon is not PNG'
    Require ([Convert]::ToHexString($iconBytes[16..23]) -eq '0000020000000200' -and $iconBytes[24] -eq 8 -and $iconBytes[25] -eq 2 -and $iconBytes[28] -eq 0) 'Required icon must be opaque512x512 RGB8 noninterlaced PNG'
    $out = [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
    Require ($out -eq $ownedRoot -or $out.StartsWith($ownedRoot + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Output must stay in ignored build/private-delivery-r3'
    $app = Join-Path $out 'PPSA77025'
    $manifestFile = Join-Path $out 'PACKAGE-MANIFEST.json'
    if (!$Finalize) {
        Require (!(Test-Path -LiteralPath $out)) 'Fresh staging directory required; this script never overwrites an existing candidate'
        New-Item -ItemType Directory -Path "$app/sce_sys" -Force | Out-Null
        Copy-Item -LiteralPath $eboot -Destination "$app/eboot.bin"
        # Fresh metadata with the verified field schema; no PE1 identity/art copied.
        $param = [ordered]@{
            ageLevel=@{default=0};applicationCategoryType=0;applicationDrmType='free'
            attribute=0;attribute2=0;attribute3=524352;conceptId='00025';contentBadgeType=1
            contentId='UP9000-PPSA77025_00-THIRDBIRTHDAY000';contentVersion='01.000.001'
            downloadDataSize=256;gameIntent=@{permittedIntents=@(@{intentType='launchActivity'})}
            localizedParameters=@{defaultLanguage='en-US';'en-US'=@{titleName='Parasite Eve 3'}}
            masterVersion='01.00';requiredSystemSoftwareVersion='0x0000000000000000'
            sdkVersion='0x0000000000000000';titleId='PPSA77025';versionFileUri=''
        }
        $param | ConvertTo-Json -Depth 8 | Set-Content "$app/sce_sys/param.json"
        $copies = @()
        $copies += @{source=$iconSource;target='sce_sys/icon0.png'}
        foreach ($name in @('LICENSE.txt','PROVENANCE.md')) { $copies += @{source=(Join-Path $root "runtime/assets/ps5/$name");target="notices/original-icon/$name"} }
        foreach ($name in @('vfpu_exp2_lut.dat','vfpu_exp2_lut65536.dat','vfpu_rcp_lut.dat','vfpu_sin_lut_delta.dat','vfpu_sin_lut_exceptions.dat','vfpu_sin_lut_interval_delta.dat','vfpu_sin_lut8192.dat')) {
            $copies += @{source=(Join-Path $root "runtime/assets/vfpu/$name");target="assets/vfpu/$name"}
        }
        foreach ($name in @('jpn0.pgf','ltn0.pgf')) { $copies += @{source=(Join-Path $root "runtime/font/$name");target="font/$name"} }
        foreach ($name in @('LICENSE','CREDITS.md')) { $copies += @{source=(Join-Path $root "runtime/$name");target="notices/upstream/$name"} }
        $copies += @{source="$base/upstream/PS5_PayloadSDK-b83202be73e930050e00de0f1b4d9ec46c0391bf/LICENSE";target='notices/ps5platform/GPL-3.0.txt'}
        $copies += @{source="$base/attempt-resolution-mods-20261031/sdl3-src/LICENSE.txt";target='notices/SDL/LICENSE.txt'}
        $copies += @{source="$base/deps/radv-release/PROVENANCE.txt";target='notices/Mesa/PROVENANCE.txt'}
        $copies += @{source="$base/upstream/PS5_Mesa-7b59ef27c1b09b9671bc4153c41940c3155c3af2/docs/license.rst";target='notices/Mesa/license.rst'}
        foreach ($file in Get-ChildItem "$base/upstream/PS5_Mesa-7b59ef27c1b09b9671bc4153c41940c3155c3af2/licenses" -File) { $copies += @{source=$file.FullName;target="notices/Mesa/licenses/$($file.Name)"} }
        foreach ($name in @('COPYING.LGPLv2.1','LICENSE.md')) { $copies += @{source=(Join-Path $root "build/ffmpeg-source/$name");target="notices/FFmpeg/$name"} }
        foreach ($name in @('stb_image.h','stb_image_write.h','PROVENANCE.json')) { $copies += @{source=(Join-Path $root "runtime/third_party/stb/$name");target="notices/stb/$name"} }
        foreach ($name in @('LICENSE','PROVENANCE.md')) { $copies += @{source=(Join-Path $root "runtime/third_party/xxhash/$name");target="notices/xxhash/$name"} }
        $copies += @{source=(Join-Path $root 'runtime/third_party/sdl_debug_font/SDL_render_debug_font.h');target='notices/SDL/SDL_render_debug_font.h'}
        $copies += @{source="$base/../public-build21-source-20261106/notices/LLVM-LICENSE.txt";target='notices/compiler-runtime/LLVM-LICENSE.txt'}
        $copyReceipt = @()
        foreach ($item in $copies) {
            Require (Test-Path -LiteralPath $item.source -PathType Leaf) "Required auxiliary/notice missing: $($item.source)"
            $target = Join-Path $app $item.target
            New-Item -ItemType Directory -Force (Split-Path $target -Parent) | Out-Null
            Copy-Item -LiteralPath $item.source -Destination $target
            $sourceHash = Digest $item.source
            Require ((Digest $target) -eq $sourceHash) "Copy differs: $($item.target)"
            $copyReceipt += @{path=$item.target;source=$item.source;sha256=$sourceHash;bytes=(Get-Item $target).Length}
        }
        @'
Parasite Eve 3 — private native PS5 development candidate
Original game: The 3rd Birthday (USA, ULUS10567).

Provisional title ID PPSA77025; no catalog/global reservation is claimed.
Planned new app: /mnt/usb0/homebrew/PPSA77025
Planned tester-owned ISO: /mnt/usb0/third-birthday-data/ULUS10567.iso
This package does not include the ISO or a save/settings template.

Native save/write services are explicitly disabled pending qualified providers.
Boot, rendering, audio, movies and gameplay have not been console-tested.
The mandatory512x512 icon is an original rectangle/text placeholder authored for
this project, MIT licensed. No unrelated PE1 or tester-owned ISO art was copied.

Do not overwrite an existing app. Installation requires the manager's current
title/path collision check and fresh closed-game confirmation. This staging task
has not accessed the console, installed, launched or changed any console files.

The EBOOT contains game-derived translated code prepared from the tester's own
disc. This is a PRIVATE candidate: do not publish this directory or its manifest.
Public source: https://github.com/GUMMIBRI/the-3rd-birthday-recomp/tree/ps5-port
Component notices are retained under notices/. Original rights remain unchanged.
'@ | Set-Content "$app/README-PRIVATE.txt"
        $files = @(Get-ChildItem -LiteralPath $app -File -Recurse | ForEach-Object {
            @{path=[IO.Path]::GetRelativePath($app,$_.FullName).Replace('\','/');bytes=$_.Length;sha256=(Digest $_.FullName)}
        })
        @{private=$true;applicationId='PPSA77025';displayName='Parasite Eve 3';appRoot=$app
          reviewedEbootSHA256=(Digest $eboot);reviewedSceSHA256=(Digest $sce);sourceResultSHA256=(Digest $resultFile)
          schemaReferenceSHA256=(Digest $schemaFile);files=$files;auxiliaryCopies=$copyReceipt
          nativeWritesEnabled=$false;iconIncluded=$true;isoIncluded=$false;saveTemplatesIncluded=$false
          consoleExecuted=$false;installed=$false;playableVerified=$false
        } | ConvertTo-Json -Depth 10 | Set-Content $manifestFile
    }
    Require (Test-Path -LiteralPath $manifestFile -PathType Leaf) 'Staged candidate manifest missing'
    $manifest = JsonFile $manifestFile
    Require ($manifest.iconIncluded -eq $true) 'Mandatory native application icon was not staged'
    Require ($manifest.reviewedEbootSHA256 -eq (Digest $eboot) -and $manifest.reviewedSceSHA256 -eq (Digest $sce)) 'Staged candidate is from another reviewed EBOOT'
    Require ($manifest.sourceResultSHA256 -eq (Digest $resultFile)) 'Reviewed conversion result changed after staging'
    $actual = @(Get-ChildItem -LiteralPath $app -File -Recurse)
    Require ($actual.Count -eq $manifest.files.Count) 'Candidate file inventory differs from staging manifest'
    foreach ($item in $manifest.files) {
        $file = [IO.Path]::GetFullPath((Join-Path $app $item.path))
        Require ($file.StartsWith($app + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Invalid path in private package manifest'
        Require ((Test-Path -LiteralPath $file -PathType Leaf) -and (Get-Item $file).Length -eq $item.bytes -and (Digest $file) -eq $item.sha256) "Candidate file changed: $($item.path)"
    }
    $ready = $false
    $reason = 'Current hash-bound post-SCE audit is missing or not passed'
    if (Test-Path -LiteralPath $SceAuditPath -PathType Leaf) {
        $gate = JsonFile $SceAuditPath
        $ready = ($gate.passed -is [bool]) -and $gate.passed -eq $true -and $gate.sce_sha256 -eq (Digest $sce) -and
            $gate.native_sha256 -eq $result.sourceNativeElfSHA256 -and $gate.imports -eq $pre.imports
        if ($ready) { $reason = 'Static reviewed EBOOT/SCE/SELF and staged-file gates passed; no hardware result' }
    }
    $state = if ($ready) { 'READY_FOR_MANAGER_REVIEW' } else { 'PENDING_POST_SCE_GATE' }
    @{state=$state;reason=$reason;private=$true;ebootSHA256=(Digest "$app/eboot.bin")
      ebootBytes=(Get-Item "$app/eboot.bin").Length;paramSHA256=(Digest "$app/sce_sys/param.json")
      fileCount=$actual.Count;plannedApp='/mnt/usb0/homebrew/PPSA77025'
      plannedISO='/mnt/usb0/third-birthday-data/ULUS10567.iso';titleCollisionCheckPending=$true
      nativeWritesEnabled=$false;iconIncluded=$true;artworkRequirementPending=$false
      installed=$false;consoleExecuted=$false;bootVerified=$false;playableVerified=$false
    } | ConvertTo-Json -Depth 5 | Set-Content "$out/STAGING-RESULT.json"
    Write-Output "$state — local private candidate only. Console collision/installation/hardware gates remain separate."
} finally { Pop-Location }
