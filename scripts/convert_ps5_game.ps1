param(
    [Parameter(Mandatory=$true)][string]$PlatformRoot,
    [Parameter(Mandatory=$true)][string]$PreparedDirectory,
    [string]$ConversionName = 'converted-r1'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if ($ConversionName -notmatch '^[A-Za-z0-9_-]+$') { throw 'Invalid conversion folder name' }
    $base = (Resolve-Path -LiteralPath $PlatformRoot).Path
    $prepared = (Resolve-Path -LiteralPath $PreparedDirectory).Path
    $link = "$prepared/native-link"
    $elf = "$link/third-birthday-native.elf"
    $audit = Get-Content "$link/GAME-AUDIT.json" -Raw | ConvertFrom-Json
    if (!$audit.passed -or $audit.sha256 -ne (Get-FileHash $elf).Hash.ToLowerInvariant()) {
        throw 'Current native game must pass static audit before conversion'
    }
    $tool = "$base/native-candidates/tool/ps5-native-tool.exe"
    if ((Get-FileHash $tool).Hash.ToLowerInvariant() -ne '78fdfec756c6e7fa132bd2fbf6eeaea753d9b01349d09a8993bba76eed77fd84') {
        throw 'Canonical native converter digest mismatch'
    }
    $out = "$link/$ConversionName"
    if (Test-Path $out) { throw 'Use a fresh conversion name; previous artifacts are preserved' }
    New-Item -ItemType Directory $out | Out-Null
    & $tool link --in $elf --out "$out/eboot.elf" --stub-dir "$base/deps/ps5-payload-sdk/target/lib" --file-name eboot.elf *> "$out/CONVERT.log"
    if ($LASTEXITCODE) { Get-Content "$out/CONVERT.log" -Tail 20; throw 'Native SCE conversion failed' }
    & node scripts/verify_ps5_conversion.cjs $elf "$out/eboot.elf" | Set-Content "$out/RETAINED.json"
    if ($LASTEXITCODE) { throw 'Converted native application changed retained bytes or memory layout' }
    & "$base/native-candidates/recipe/inspect-executable.ps1" -Path "$out/eboot.elf" -RequireApplicationElf |
        Set-Content "$out/APPLICATION-SHAPE.json"
    & $tool self --sign --in "$out/eboot.elf" --out "$out/eboot.bin" *> "$out/SELF.log"
    if ($LASTEXITCODE) { throw 'Native SELF creation failed' }
    $inspection = & $tool self --inspect --file "$out/eboot.bin"
    $inspection | Set-Content "$out/SELF-INSPECTION.txt"
    if ($LASTEXITCODE -or !(($inspection -join "`n") -match '(?m)^integrity:\s*valid\s*$')) {
        throw 'Native SELF integrity check failed'
    }
    if ($audit.sha256 -ne (Get-FileHash $elf).Hash.ToLowerInvariant()) { throw 'Native input changed during conversion' }
    @{
        state='private-native-development-eboot'; displayName='Parasite Eve 3'; originalGame='The 3rd Birthday'
        sourceNativeElfSHA256=$audit.sha256; convertedElfSHA256=(Get-FileHash "$out/eboot.elf").Hash
        ebootSHA256=(Get-FileHash "$out/eboot.bin").Hash; ebootBytes=(Get-Item "$out/eboot.bin").Length
        importCount=$audit.imports; retainedSectionsVerified=$true; selfIntegrityValid=$true
        postConversionImportAudit='separate gate'; nativeWritesEnabled=$false
        installed=$false; consoleExecuted=$false; playableVerified=$false
    } | ConvertTo-Json | Set-Content "$out/RESULT.json"
    Write-Output "Private native eboot prepared: $out/eboot.bin; not installed or gameplay verified."
} finally { Pop-Location }
