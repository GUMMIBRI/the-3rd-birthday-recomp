param([Parameter(Mandatory=$true)][string]$Zig)
# Rebuild the upstream, openly licensed host preparation utility. No game input.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $compiler = (Resolve-Path -LiteralPath $Zig).Path
    $ed = 'runtime/third_party/eboot_decrypt'
    $out = 'build/eboot-tool'
    New-Item -ItemType Directory -Force $out | Out-Null
    $sources = @{}
    Get-ChildItem $ed -Recurse -File | Where-Object {$_.Extension -in '.c','.cpp','.h'} | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($root, $_.FullName).Replace('\','/')
        $sources[$relative] = (Get-FileHash $_.FullName).Hash.ToLowerInvariant()
    }
    foreach ($name in @('AES','SHA1','bn','ec','kirk_engine','amctrl')) {
        & $compiler cc -O2 -w -c "$ed/libkirk/$name.c" -o "$out/$name.o"
        if ($LASTEXITCODE) { throw "Host preparation tool compile failed: $name" }
    }
    $objects = @('AES','SHA1','bn','ec','kirk_engine','amctrl') | ForEach-Object {"$out/$_.o"}
    & $compiler c++ -std=c++17 -O2 -w "-I$ed/shim" "-I$ed" "$ed/eboot_decrypt.cpp" "$ed/PrxDecrypter.cpp" @objects -o "$out/eboot_decrypt.exe"
    if ($LASTEXITCODE) { throw 'Host preparation tool link failed' }
    foreach ($entry in $sources.GetEnumerator()) {
        if ((Get-FileHash $entry.Key).Hash.ToLowerInvariant() -ne $entry.Value) { throw 'Preparation sources changed during build' }
    }
    @{
        method='locally-built-upstream-eboot-decrypt'; recipe='scripts/build_eboot_tool.ps1'
        recipe_sha256=(Get-FileHash $PSCommandPath).Hash.ToLowerInvariant()
        compiler_sha256=(Get-FileHash $compiler).Hash.ToLowerInvariant()
        compiler_version=(& $compiler version); source_hashes=$sources
        executable='eboot_decrypt.exe'; executable_sha256=(Get-FileHash "$out/eboot_decrypt.exe").Hash.ToLowerInvariant()
        license='GPL-3.0-or-later; upstream component notices retained'; game_input_used=$false
    } | ConvertTo-Json -Depth 5 | Set-Content "$out/BUILD-RECEIPT.json"
    Write-Output 'Upstream host preparation tool rebuilt with source/binary receipt.'
} finally { Pop-Location }
