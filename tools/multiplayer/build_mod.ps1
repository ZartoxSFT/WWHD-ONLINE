# Builds the multiplayer-connection guest mod (examples/guest-mods/multiplayer-connection) and
# installs it into the run_dev.ps1 instances. Works on any PC of the project: the tools are found,
# not hard-coded.
#
#   build_mod.ps1                   build, check, install into instance 1
#   build_mod.ps1 -Instance 1,2     install into instances 1 and 2
#   build_mod.ps1 -Instance 0       build and check only
#   build_mod.ps1 -Clang C:\msys64\clang64\bin\clang.exe   a given compiler
#
# Tools (docs/mod-sdk-v2.md "Build"):
#   clang with the PowerPC backend: MSYS2 CLANG64 (C:\msys64\clang64) or MINGW64 clang. The clang of
#     the install (data\toolchain\llvm-mingw-*) has no PowerPC target, so it is not used;
#   ld.lld: the one of the install (any lld links ELF PowerPC), else the one next to clang, else
#     devkitPro's;
#   the check translates mod.elf like the game does at start (tools/guestmod/guestmod.py, with the
#     install's Python): an error shows here instead of in the game's Mods tab.
# The game translates and compiles the installed mod.elf itself at its next start (GuestBuild).
param(
    [int[]]$Instance = @(1),
    [string]$Clang = "",
    [string]$Lld = "",
    [switch]$NoCheck,
    [string]$Distribution = (Join-Path $PSScriptRoot "..\..\..\WindWakerHD-v0.3.0-windows-x86_64"),
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\..\build\windows-msys2-d")
)
$ErrorActionPreference = "Stop"

$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$mod = Join-Path $repo "examples\guest-mods\multiplayer-connection"
$include = Join-Path $repo "runtime\guest\include"
$dist = Resolve-Path $Distribution

function Test-PowerPcClang([string]$Path) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) { return $false }
    $targets = & $Path --print-targets 2>$null | Out-String
    return $targets -match "ppc32"
}

if (-not $Clang) {
    $candidates = @(
        "C:\msys64\clang64\bin\clang.exe", "D:\msys64\clang64\bin\clang.exe",
        "C:\msys64\mingw64\bin\clang.exe", "D:\msys64\mingw64\bin\clang.exe"
    )
    $cache = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path -LiteralPath $cache) { # the compiler of this PC's runtime build
        $line = Select-String -LiteralPath $cache -Pattern "^CMAKE_C_COMPILER:\w+=(.+)$" | Select-Object -First 1
        if ($line) { $candidates = @($line.Matches[0].Groups[1].Value) + $candidates }
    }
    $where = Get-Command clang.exe -ErrorAction SilentlyContinue
    if ($where) { $candidates += $where.Source }
    $Clang = $candidates | Where-Object { Test-PowerPcClang $_ } | Select-Object -First 1
    if (-not $Clang) {
        throw "No clang with the PowerPC backend. Install MSYS2 CLANG64 (pacman -S mingw-w64-clang-x86_64-clang mingw-w64-clang-x86_64-lld) or pass -Clang"
    }
} elseif (-not (Test-PowerPcClang $Clang)) {
    throw "$Clang has no PowerPC target"
}

if (-not $Lld) {
    $bundled = Get-ChildItem -LiteralPath (Join-Path $dist "data\toolchain") -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName "bin\ld.lld.exe" } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    $candidates = @($bundled, (Join-Path (Split-Path $Clang) "ld.lld.exe"), "C:\devkitPro\msys2\mingw64\bin\ld.lld.exe")
    $Lld = $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
    if (-not $Lld) { throw "No ld.lld found; pass -Lld" }
}

Write-Host "clang  : $Clang"
Write-Host "ld.lld : $Lld"

# clang needs its own DLLs (MSYS2 bin) on PATH
$env:PATH = (Split-Path $Clang) + ";" + $env:PATH

$object = Join-Path $mod "mod.o"
$elf = Join-Path $mod "mod.elf"
$flags = @("--target=powerpc-unknown-eabi", "-mcpu=750", "-O2", "-ffreestanding", "-fno-builtin", "-nostdlib",
           "-fno-jump-tables", "-ffunction-sections", "-fdata-sections", "-I$include",
           "-Wall", "-Wextra", "-Wshadow", "-Wconversion", "-Werror")
& $Clang @flags -c (Join-Path $mod "mod.c") -o $object
if ($LASTEXITCODE -ne 0) { throw "mod.c does not compile" }
& $Lld -m elf32ppc -r $object -o $elf
if ($LASTEXITCODE -ne 0) { throw "mod.o does not link" }
Write-Host "built  : $elf"

if (-not $NoCheck) {
    $python = Join-Path $dist "tools\python\python.exe"
    if (-not (Test-Path -LiteralPath $python)) {
        $py = Get-Command python.exe -ErrorAction SilentlyContinue
        $python = if ($py) { $py.Source } else { $null }
    }
    if ($python) {
        $out = Join-Path $env:TEMP "multiplayer-connection-check.c"
        $report = Join-Path $env:TEMP "multiplayer-connection-check.json"
        & $python (Join-Path $repo "tools\guestmod\guestmod.py") $elf $out --base 0x7F000000 --id multiplayer-connection --report $report | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "the game would refuse mod.elf (translation failed, see above)" }
        $r = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
        Write-Host ("check  : translated, {0} functions, {1} hooks" -f $r.functions, $r.hooks.Count)
        # every import must be a host service of the runtime (guest_mods.cpp): the module's init
        # returns null otherwise and the game says "module ABI does not match" (0.2.33: __fixsfdi,
        # the compiler's helper for a float to 64-bit integer conversion)
        $hostSource = Get-Content -LiteralPath (Join-Path $repo "runtime\src\mods\guest_mods.cpp") -Raw
        $hostServices = [regex]::Matches($hostSource, '\{"([A-Za-z_0-9]+)",\s*svc_') | ForEach-Object { $_.Groups[1].Value }
        $imports = [regex]::Match((Get-Content -LiteralPath $out -Raw), 'k_services\[\] = \{([^}]*)\}')
        if ($imports.Success) {
            $missing = [regex]::Matches($imports.Groups[1].Value, '"([^"]+)"') | ForEach-Object { $_.Groups[1].Value } |
                Where-Object { $hostServices -notcontains $_ }
            if ($missing) { throw ("mod.elf imports what the game does not provide: " + ($missing -join ", ")) }
            Write-Host "check  : every import is a host service"
        }
        # the runtime refuses a mod that hooks or calls a function whose code differs in another
        # regional build ("function differs between game builds", 0.2.55: dMeter_Execute 02593B10)
        $used = @($r.hooks | ForEach-Object { $_.target }) + @($r.game_calls)
        foreach ($map in Get-ChildItem -LiteralPath (Join-Path $repo "tools\recomp\builds") -Filter *.json) {
            $build = Get-Content -LiteralPath $map.FullName -Raw | ConvertFrom-Json
            foreach ($d in $build.differing_functions) {
                $start = [Convert]::ToUInt32($d[0], 16); $end = $start + [uint32]$d[1]
                $bad = $used | Where-Object { $a = [Convert]::ToUInt32($_, 16); $a -ge $start -and $a -lt $end }
                if ($bad) { throw ("mod.elf uses " + ($bad -join ", ") + ", which differs in the " + $build.name + " build: the game refuses the mod") }
            }
        }
        Write-Host "check  : no function that differs between builds"
    } else {
        Write-Host "check  : skipped (no Python)"
    }
}

$version = (Get-Content -LiteralPath (Join-Path $mod "manifest.json") -Raw | ConvertFrom-Json).version
foreach ($n in $Instance) {
    if ($n -le 0) { continue }
    $user = Join-Path $BuildDir "instances\$n\user"
    if (-not (Test-Path -LiteralPath $user)) {
        Write-Host "instance $n does not exist yet: start it once with run_dev.ps1 -Instance $n, then build again"
        continue
    }
    $target = Join-Path $user "ModManager\Mods\multiplayer-connection"
    # robocopy: exit codes 0-7 are success; the build's object file is not part of the package
    & robocopy $mod $target /E /R:1 /W:1 /XF mod.o /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "install failed ($LASTEXITCODE): $target" }
    $global:LASTEXITCODE = 0
    Write-Host "install: instance $n, version $version"
}
