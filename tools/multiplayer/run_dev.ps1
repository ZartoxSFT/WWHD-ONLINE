# Starts the locally built runtime (with the wwhd_net_* host services) as an isolated instance.
#
# Each instance has its own user folder (mods, settings, launcher.cfg) and save folder under
# <BuildDir>\instances\<n>, copied once from the portable v0.3.0 install. The install's own
# folders are only read, never written, so a test can never damage the real saves.
#
#   run_dev.ps1                      instance 1
#   run_dev.ps1 -Instance 2          a second player on the same PC
#   run_dev.ps1 -Instance 2 -CopyModsFrom 1   refresh instance 2's installed mods from instance 1
#   run_dev.ps1 -Instance 1 -Reset   start instance 1 again from the install (deletes it)
#   run_dev.ps1 -BuildMod            rebuild the multiplayer mod and install it first (after a pull)
#
# The runtime DLL folder (-MsysBin) is found from the build: MINGW64 on one PC, CLANG64 on another.
#
# The executable is started from the build directory on purpose: started from the portable
# install, code-mod startup would switch to that install's cached code-mod build, which has no
# network services. WWHD_INSTALL_DIR is therefore left unset.
param(
    [ValidateRange(1, 8)][int]$Instance = 1,
    [ValidateRange(0, 8)][int]$CopyModsFrom = 0,
    [switch]$Reset,
    [string]$Distribution = (Join-Path $PSScriptRoot "..\..\..\WindWakerHD-v0.3.0-windows-x86_64"),
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\..\build\windows-msys2-d"),
    # the folder of the C++ runtime DLLs wwhd.exe links (libstdc++ of MINGW64, libc++ of CLANG64);
    # empty: the bin folder of the compiler that built it (BuildDir\CMakeCache.txt)
    [string]$MsysBin = "",
    [string]$GameDir = "",
    # rebuild and install the multiplayer mod first (build_mod.ps1), e.g. after a git pull
    [switch]$BuildMod
)
$ErrorActionPreference = "Stop"

if (-not $MsysBin) {
    $cache = Join-Path $BuildDir "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cache)) { throw "No ${cache}: build the runtime first, or pass -MsysBin" }
    $line = Select-String -LiteralPath $cache -Pattern "^CMAKE_CXX_COMPILER:\w+=(.+)$" | Select-Object -First 1
    if (-not $line) { throw "No CMAKE_CXX_COMPILER in ${cache}: pass -MsysBin" }
    $MsysBin = Split-Path ($line.Matches[0].Groups[1].Value -replace "/", "\")
}
if (-not (Test-Path -LiteralPath $MsysBin)) { throw "-MsysBin $MsysBin does not exist" }

if ($BuildMod) {
    & (Join-Path $PSScriptRoot "build_mod.ps1") -Instance $Instance -BuildDir $BuildDir -Distribution $Distribution
}

$data = Join-Path (Resolve-Path $Distribution) "data"
$build = Resolve-Path $BuildDir
$exe = Join-Path $build "wwhd.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw "Runtime not built: $exe" }
if (-not $GameDir) { $GameDir = (Get-Content -LiteralPath (Join-Path $data "install.json") -Raw | ConvertFrom-Json).game_dir }
# -LiteralPath: dump folder names contain [brackets], which Test-Path would read as wildcards
if (-not (Test-Path -LiteralPath (Join-Path $GameDir "code\cking.rpx"))) { throw "Game folder without code\cking.rpx: $GameDir" }

# robocopy: Copy-Item fails on the long GuestBuild\<64 hex> paths. Exit codes 0-7 are success.
function Copy-Tree([string]$From, [string]$To) {
    # launcher.cfg holds the instance identity: never copied between instances
    & robocopy $From $To /E /R:1 /W:1 /XF launcher.cfg /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "copy failed ($LASTEXITCODE): $From -> $To" }
    $global:LASTEXITCODE = 0
}

$root = Join-Path $build "instances\$Instance"
$user = Join-Path $root "user"
$save = Join-Path $root "save"
$cfgDir = Join-Path $user "ModManager\Data\multiplayer-connection"

if ($Reset -and (Test-Path -LiteralPath $root)) {
    $empty = Join-Path $env:TEMP "wwhd-empty-$Instance"
    New-Item -ItemType Directory -Force $empty | Out-Null
    & robocopy $empty $root /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
    Remove-Item -LiteralPath $empty, $root -Recurse -Force
    Write-Host "instance $Instance reset"
}

if (-not (Test-Path -LiteralPath $user)) {
    Write-Host "creating instance $Instance from $data"
    New-Item -ItemType Directory -Force $user, $save | Out-Null
    foreach ($dir in "ModManager", "story") {
        $source = Join-Path $data "user\$dir"
        if (Test-Path -LiteralPath $source) { Copy-Tree $source (Join-Path $user $dir) }
    }
    $settings = Join-Path $data "user\settings.ini"
    if (Test-Path -LiteralPath $settings) { Copy-Item -LiteralPath $settings -Destination $user }
    $saveSource = Join-Path $data "save"
    if (Test-Path -LiteralPath $saveSource) { Copy-Tree $saveSource $save }
}

if ($CopyModsFrom -gt 0 -and $CopyModsFrom -ne $Instance) {
    $from = Join-Path $build "instances\$CopyModsFrom\user\ModManager"
    if (-not (Test-Path -LiteralPath $from)) { throw "Instance $CopyModsFrom does not exist" }
    $cfgFile = Join-Path $cfgDir "launcher.cfg"
    $kept = $null
    if (Test-Path -LiteralPath $cfgFile) { $kept = [System.IO.File]::ReadAllBytes($cfgFile) }
    $target = Join-Path $user "ModManager"
    if (Test-Path -LiteralPath $target) {
        # robocopy /MIR into an empty folder also removes long paths Remove-Item cannot
        $empty = Join-Path $env:TEMP "wwhd-empty-$Instance"
        New-Item -ItemType Directory -Force $empty | Out-Null
        & robocopy $empty $target /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
        Remove-Item -LiteralPath $empty, $target -Recurse -Force
    }
    Copy-Tree $from $target
    if ($kept) {
        New-Item -ItemType Directory -Force $cfgDir | Out-Null
        [System.IO.File]::WriteAllBytes($cfgFile, $kept)
    }
    Write-Host "mods copied from instance $CopyModsFrom"
}

# One identity and tunic colour per instance, unless the launcher already wrote the file.
$cfgFile = Join-Path $cfgDir "launcher.cfg"
if (-not (Test-Path -LiteralPath $cfgFile)) {
    $colors = @("0DBD2F", "3060FF", "E63946", "FCA311", "9B5DE5", "00BBF9", "F15BB5", "FEE440")
    New-Item -ItemType Directory -Force $cfgDir | Out-Null
    $lines = @(
        "# written by run_dev.ps1; the launcher may overwrite it",
        ("uid=player{0:D2}" -f $Instance),
        "host=127.0.0.1",
        "port=10014",
        ("color=" + $colors[$Instance - 1]),
        "puppet=0"
    )
    [System.IO.File]::WriteAllText($cfgFile, ($lines -join "`n") + "`n", [System.Text.Encoding]::ASCII)
}

$env:PATH = "$MsysBin;$env:PATH"                                # the C++ runtime DLLs of this build
$env:WWHD_CODE_MODS = "1"                                     # build/gen was translated with --mod-hooks
$env:WWHD_GUEST_BUILD_CONFIG = Join-Path $data "guest-sdk.json"  # guest translator + compiler of the install
Remove-Item Env:WWHD_INSTALL_DIR -ErrorAction SilentlyContinue

# Runtime and guest-mod logs ([guestmod:<id>] ...) go to stderr; keep them in a file.
$log = Join-Path $root "wwhd.log"
Write-Host "instance: $Instance"
Write-Host "runtime : $exe"
Write-Host "game    : $GameDir"
Write-Host "user    : $user"
Write-Host "save    : $save"
Write-Host "settings: $cfgFile"
Write-Host "log     : $log   (Get-Content -Wait '$log' | Select-String multiplayer)"
$arguments = @("--user-dir", "`"$user`"", "--save", "`"$save`"", "--game", "`"$GameDir`"")
Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $root `
    -RedirectStandardError $log -RedirectStandardOutput "$log.out" -NoNewWindow -Wait