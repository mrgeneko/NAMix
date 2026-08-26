# Windows counterpart to scripts/makedist-linux.sh / makedist-macos.sh -- same staged-
# directory-then-archive shape, PowerShell instead of bash since that's what's native (and
# has Compress-Archive built in) on the windows-latest GitHub Actions runner, rather than
# depending on git-bash having `zip` available.
#
# UNVERIFIED LOCALLY: written and reviewed against JUCE's own CMake source for the exact
# Windows VST3 artifact layout (see CMakeLists.txt / build.yml's own comments), but there is
# no Windows machine in the environment this was authored in -- first real run is CI itself.

$ErrorActionPreference = "Stop"

$Repo = Split-Path -Parent $PSScriptRoot

cmake -B "$Repo/build" -A x64 -S "$Repo"
cmake --build "$Repo/build" --config Release --parallel

$CMakeListsContent = Get-Content "$Repo/CMakeLists.txt" -Raw
if ($CMakeListsContent -match 'project\(namix VERSION (\S+)') {
    $Version = $Matches[1]
} else {
    throw "Could not find 'project(namix VERSION ...)' in CMakeLists.txt"
}

$Arch = "x86_64"
$StageDir = Join-Path $env:TEMP ([System.Guid]::NewGuid().ToString())
$PkgDir = Join-Path $StageDir "NAMix-$Version"
New-Item -ItemType Directory -Path $PkgDir -Force | Out-Null

$Artefacts = "$Repo/build/NAMix_artefacts/Release"

# VST3 bundle (a folder even on Windows -- see CMakeLists.txt's comment on
# _juce_create_windows_package) and the Standalone .exe. No AU on Windows (macOS-only).
Copy-Item -Recurse "$Artefacts/VST3/Anti-Static NAM.vst3" "$PkgDir/"
Copy-Item "$Artefacts/Standalone/Anti-Static NAM.exe" "$PkgDir/"

Copy-Item "$Repo/NOTICE" "$PkgDir/"
if (Test-Path "$Repo/LICENSE") { Copy-Item "$Repo/LICENSE" "$PkgDir/" }
elseif (Test-Path "$Repo/LICENCE") { Copy-Item "$Repo/LICENCE" "$PkgDir/" }

New-Item -ItemType Directory -Path "$Repo/dist" -Force | Out-Null
$Zip = "$Repo/dist/NAMix-$Version-windows-$Arch.zip"
if (Test-Path $Zip) { Remove-Item $Zip }
Compress-Archive -Path $PkgDir -DestinationPath $Zip
Remove-Item -Recurse -Force $StageDir

Write-Host "Packaged: $Zip"
Write-Host ""
Write-Host "Contents:"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead($Zip)
$archive.Entries | ForEach-Object { Write-Host "  $($_.FullName)" }
$archive.Dispose()
