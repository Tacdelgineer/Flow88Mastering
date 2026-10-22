$ErrorActionPreference = 'Stop'
$bundle = 'build/Flow88Master_artefacts/Release/VST3/Flow88 Master.vst3'
$binary = Join-Path $bundle 'Contents/x86_64-win/Flow88 Master.vst3'
if (!(Test-Path $binary -PathType Leaf)) { throw 'Missing Windows x64 VST3 binary' }
$bytes = [IO.File]::ReadAllBytes((Resolve-Path $binary))
if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) { throw 'Not a PE binary' }
$pe = [BitConverter]::ToInt32($bytes, 0x3c)
if ([BitConverter]::ToUInt32($bytes, $pe) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x8664) { throw 'Not an AMD64 PE binary' }
if (Test-Path release) { Remove-Item release -Recurse -Force }
New-Item -ItemType Directory release | Out-Null
Copy-Item $bundle release -Recurse
Copy-Item install.bat release
Copy-Item docs/INSTALL-Windows.txt release
Copy-Item docs/beta-release-notes.md release
Copy-Item evidence release -Recurse
$sha = git rev-parse HEAD
"Source commit: $sha`nTarget: Windows x64 Release VST3`nJUCE: 8.0.12`nDAWs: unverified (automated VST3 host testing only)" | Set-Content release/BUILD.txt
Compress-Archive -Path release/* -DestinationPath Flow88Mastering-Windows-VST3.zip -Force
Get-FileHash Flow88Mastering-Windows-VST3.zip -Algorithm SHA256 | Format-List | Out-File evidence/zip-sha256.txt
