param(
    [Parameter(Mandatory=$true)][string]$PlatformRoot,
    [string]$OutputDirectory = 'build/ps5-app-host'
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
Push-Location $repoRoot
try {
    $base = (Resolve-Path -LiteralPath $PlatformRoot).Path
    $out = (Resolve-Path -LiteralPath $OutputDirectory).Path
    $llvm = "$base/deps/llvm/bin"
    $elf = "$out/third-birthday-host.elf"
    & "$base/native-candidates/recipe/audit-application-imports.ps1" -Elf $elf `
        -StubDirectory "$base/deps/ps5-payload-sdk/target/lib" `
        -ReadElf "$llvm/llvm-readelf.exe" -Output "$out/imports.json"
    if ($LASTEXITCODE) { throw 'Import audit failed' }
    $imports = Get-Content "$out/imports.json" -Raw | ConvertFrom-Json
    $symbols = @(& "$llvm/llvm-nm.exe" $elf)
    if ($LASTEXITCODE) { throw 'Symbol inspection failed' }
    $sections = @(& "$llvm/llvm-readelf.exe" -SW $elf)
    if ($LASTEXITCODE) { throw 'Section inspection failed' }
    $segments = @(& "$llvm/llvm-readelf.exe" -lW $elf)
    if ($LASTEXITCODE) { throw 'Program header inspection failed' }
    $relocations = @(& "$llvm/llvm-readelf.exe" -rW $elf)
    if ($LASTEXITCODE) { throw 'Relocation inspection failed' }
    $decoded = @(& "$llvm/llvm-objdump.exe" -d --no-show-raw-insn $elf)
    if ($LASTEXITCODE) { throw 'Instruction decode failed' }
    $issues = [Collections.Generic.List[string]]::new()
    $syscalls = @($decoded | Where-Object { $_ -match '^\s*[0-9a-f]+:\s+syscall\s*$' })
    if ($syscalls.Count) { $issues.Add('Decoded inline syscall instructions remain') }
    $nativeTLS = @($segments | Where-Object { $_ -match '^\s*TLS\s' }).Count -gt 0 -or
        @($sections | Where-Object { $_ -match '\.(tdata|tbss)\s' }).Count -gt 0
    if ($nativeTLS) { $issues.Add('Native TLS is not supported by this application route') }
    foreach ($line in $sections) {
        if ($line -match '^\s*\[\s*\d+\]\s+\S+\s+\S+\s+[0-9a-f]+\s+[0-9a-f]+\s+[0-9a-f]+\s+[0-9a-f]+\s+(\S+)\s+\d+\s+\d+\s+\d+\s*$' -and
            $Matches[1].Contains('T')) { $issues.Add('SHF_TLS section is unsupported') }
    }
    foreach ($line in $relocations) {
        if ($line -match '\b(R_X86_64_\w+)\b' -and
            $Matches[1] -notin @('R_X86_64_64', 'R_X86_64_GLOB_DAT', 'R_X86_64_JUMP_SLOT', 'R_X86_64_RELATIVE')) {
            $issues.Add("Unsupported relocation: $($Matches[1])")
        }
    }
    $tlsReferences = @($symbols | Where-Object { $_ -match '\bU\s+.*(__emutls|__tls_get_addr)' })
    $tlsDefinitions = @($symbols | Where-Object { $_ -match '\b[tT]\s+__emutls_get_address$' })
    # This initial host application contract is strictly TLS-free. Check every
    # symbol, including weak/defined controls, rather than only unresolved calls.
    if (@($symbols | Where-Object { $_ -match '(__emutls|emutls_|__tls_get_addr|__tls_lookup)' }).Count) {
        $issues.Add('TLS symbols require a separate runtime audit and are unsupported in this host target')
    }
    foreach ($item in $imports.Imports) {
        if ($item.Binding -eq 'WEAK' -or $item.Symbol -match '^(__dl|__crt|payload_|kernel_)') {
            $issues.Add("Unsupported runtime import: $($item.Symbol)")
        }
        $expected = if ($item.Symbol -match '^(pthread_|sceKernel|__error$|_exit$)') {
            'libkernel.sprx'
        } elseif ($item.Symbol -match '^(_init_env|atexit|exit|calloc|free|abort|puts)$') {
            'libSceLibcInternal.sprx'
        } else { $null }
        if ($expected -and $item.Module -ne $expected) { $issues.Add("Wrong provider: $($item.Symbol)") }
    }
    if (@($symbols | Where-Object { $_ -match '\b[tT]\s+(__crt|payload_|kernel_)' }).Count) {
        $issues.Add('Payload startup/runtime definitions found')
    }
    $result = @{
        Passed = $issues.Count -eq 0; Issues = @($issues); ImportCount = $imports.ImportCount
        DecodedSyscalls = $syscalls.Count; NativeTLS = $nativeTLS
        EmutlsDefinitions = $tlsDefinitions.Count; SHA256 = (Get-FileHash $elf).Hash
        ConsoleExecuted = $false; Limit = 'Static link/provider audit only; not a game or hardware result'
    }
    $result | ConvertTo-Json -Depth 6 | Set-Content "$out/HOST-AUDIT.json"
    if ($issues.Count) { throw ($issues -join '; ') }
    Write-Output 'Native host static audit passed; console behavior remains unverified.'
} finally { Pop-Location }
