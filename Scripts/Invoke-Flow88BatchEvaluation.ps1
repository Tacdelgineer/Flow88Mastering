[CmdletBinding()]
param(
    [string]$InputFolder = "",

    [string]$OutputCsv = "",

    [string]$EvalExe = "",

    [switch]$TestAudioValidation,

    [switch]$IncludeWindowsCopyDuplicates
)

<#
.SYNOPSIS
Runs Flow88MasterEval across a folder of WAV files and writes one consolidated CSV.

.EXAMPLE
powershell -ExecutionPolicy Bypass -File .\Scripts\Invoke-Flow88BatchEvaluation.ps1 `
  -TestAudioValidation

.EXAMPLE
powershell -ExecutionPolicy Bypass -File .\Scripts\Invoke-Flow88BatchEvaluation.ps1 `
  -InputFolder ".\TestAudio" `
  -OutputCsv ".\build\Flow88BatchEvaluation_TestAudio.csv"
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptRoot = if ($PSScriptRoot) {
    $PSScriptRoot
} else {
    Split-Path -Parent $MyInvocation.MyCommand.Path
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptRoot ".."))
$defaultEvalExe = Join-Path $repoRoot "build\Flow88MasterEval_artefacts\Release\Flow88MasterEval.exe"
$defaultOutputCsv = Join-Path $repoRoot "build\Flow88BatchEvaluation.csv"
$testAudioInputFolder = Join-Path $repoRoot "TestAudio"
$testAudioOutputCsv = Join-Path $repoRoot "build\Flow88BatchEvaluation_TestAudio.csv"

function Fail-Batch {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    throw $Message
}

function Resolve-RepoRelativePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

function Get-TestAudioValidationMatrix {
    return @(
        [pscustomobject]@{ Genre = "House";       Style = "Clean"; Preset = "House / Clean";       SubgenreId = "house";       StyleId = "clean" }
        [pscustomobject]@{ Genre = "House";       Style = "Club";  Preset = "House / Club";        SubgenreId = "house";       StyleId = "club"  }
        [pscustomobject]@{ Genre = "Techno";      Style = "Clean"; Preset = "Techno / Clean";      SubgenreId = "techno";      StyleId = "clean" }
        [pscustomobject]@{ Genre = "Techno";      Style = "Club";  Preset = "Techno / Club";       SubgenreId = "techno";      StyleId = "club"  }
        [pscustomobject]@{ Genre = "Melodic";     Style = "Clean"; Preset = "Melodic / Clean";     SubgenreId = "melodic";     StyleId = "clean" }
        [pscustomobject]@{ Genre = "Melodic";     Style = "Warm";  Preset = "Melodic / Warm";      SubgenreId = "melodic";     StyleId = "warm"  }
        [pscustomobject]@{ Genre = "Progressive"; Style = "Clean"; Preset = "Progressive / Clean"; SubgenreId = "progressive"; StyleId = "clean" }
        [pscustomobject]@{ Genre = "Progressive"; Style = "Warm";  Preset = "Progressive / Warm";  SubgenreId = "progressive"; StyleId = "warm"  }
        [pscustomobject]@{ Genre = "Trance";      Style = "Club";  Preset = "Trance / Club";       SubgenreId = "trance";      StyleId = "club"  }
    )
}

function Get-TestAudioCorpusFileNames {
    return @(
        "Clinical Orbit.wav"
        "Driving Horizon.wav"
        "Layered Horizons.wav"
        "Midnight Airwave.wav"
        "Midnight Tectonics.wav"
        "Wide Horizon Break.wav"
    )
}

function Test-IsWindowsCopyDuplicate {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.FileInfo]$File,

        [Parameter(Mandatory = $true)]
        [hashtable]$KnownFilesByBaseAndExtension
    )

    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($File.Name)
    if ($baseName -notmatch '^(?<OriginalName>.+) \((?<CopyIndex>\d+)\)$') {
        return $false
    }

    $originalKey = "{0}|{1}" -f $Matches.OriginalName.ToLowerInvariant(), $File.Extension.ToLowerInvariant()
    return $KnownFilesByBaseAndExtension.ContainsKey($originalKey)
}

function New-ReportRow {
    param(
        [string]$Track = "",
        [string]$Genre = "",
        [string]$Preset = "",
        [string]$Style = "",
        [object]$LufsI = "",
        [object]$ShortTermLufs = "",
        [object]$TruePeak = "",
        [object]$Correlation = "",
        [object]$Width = "",
        [object]$TonalBalanceDeviation = "",
        [object]$LowEndStability = "",
        [object]$LoudnessSafetyScore = "",
        [object]$TonalBalanceScore = "",
        [object]$WidthRiskScore = "",
        [object]$LowEndStabilityScore = "",
        [object]$LimiterDamageScore = "",
        [object]$OverallScore = ""
    )

    [pscustomobject][ordered]@{
        "Track"                    = $Track
        "Genre"                    = $Genre
        "Preset"                   = $Preset
        "Style"                    = $Style
        "LUFS-I"                   = $LufsI
        "Short-term LUFS"          = $ShortTermLufs
        "True Peak"                = $TruePeak
        "Correlation"              = $Correlation
        "Width"                    = $Width
        "Tonal Balance Deviation"  = $TonalBalanceDeviation
        "Low-End Stability"        = $LowEndStability
        "Loudness Safety Score"    = $LoudnessSafetyScore
        "Tonal Balance Score"      = $TonalBalanceScore
        "Width Risk Score"         = $WidthRiskScore
        "Low-End Stability Score"  = $LowEndStabilityScore
        "Limiter Damage Score"     = $LimiterDamageScore
        "Overall Score"            = $OverallScore
    }
}

function Get-Average {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Rows,

        [Parameter(Mandatory = $true)]
        [string]$Property
    )

    if ($Rows.Count -eq 0) {
        return 0.0
    }

    $sum = 0.0
    foreach ($row in $Rows) {
        $sum += [double]$row.PSObject.Properties[$Property].Value
    }

    return [Math]::Round($sum / $Rows.Count, 2)
}

function Get-WorstRiskFlag {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Row
    )

    $risks = @(
        [pscustomobject]@{ Name = "TruePeak"; Severity = [Math]::Max(0.0, ([double]$Row."True Peak" + 1.0) * 100.0) }
        [pscustomobject]@{ Name = "WidthRisk"; Severity = [double]$Row."Width Risk Score" }
        [pscustomobject]@{ Name = "LowEnd"; Severity = 100.0 - [double]$Row."Low-End Stability Score" }
        [pscustomobject]@{ Name = "Limiter"; Severity = [double]$Row."Limiter Damage Score" }
        [pscustomobject]@{ Name = "Loudness"; Severity = 100.0 - [double]$Row."Loudness Safety Score" }
    )

    return $risks | Sort-Object Severity -Descending | Select-Object -First 1
}

$tempDirectory = $null

try {
    if ($TestAudioValidation) {
        if (-not [string]::IsNullOrWhiteSpace($InputFolder)) {
            $requestedInput = Resolve-RepoRelativePath -Path $InputFolder
            $expectedInput = [System.IO.Path]::GetFullPath($testAudioInputFolder)
            if (-not $requestedInput.Equals($expectedInput, [System.StringComparison]::OrdinalIgnoreCase)) {
                Fail-Batch "-TestAudioValidation uses the built-in TestAudio corpus only. Remove -InputFolder or set it to '$expectedInput'."
            }
        }

        if (-not [string]::IsNullOrWhiteSpace($OutputCsv)) {
            $requestedOutput = Resolve-RepoRelativePath -Path $OutputCsv
            $expectedOutput = [System.IO.Path]::GetFullPath($testAudioOutputCsv)
            if (-not $requestedOutput.Equals($expectedOutput, [System.StringComparison]::OrdinalIgnoreCase)) {
                Fail-Batch "-TestAudioValidation writes to the fixed CSV path '$expectedOutput'. Remove -OutputCsv or set it to that exact path."
            }
        }

        $InputFolder = $testAudioInputFolder
        $OutputCsv = $testAudioOutputCsv
    } elseif ([string]::IsNullOrWhiteSpace($InputFolder)) {
        Fail-Batch "InputFolder is required unless -TestAudioValidation is used."
    }

    if ([string]::IsNullOrWhiteSpace($EvalExe)) {
        $EvalExe = $defaultEvalExe
    } else {
        $EvalExe = Resolve-RepoRelativePath -Path $EvalExe
    }

    if ([string]::IsNullOrWhiteSpace($OutputCsv)) {
        $OutputCsv = $defaultOutputCsv
    } else {
        $OutputCsv = Resolve-RepoRelativePath -Path $OutputCsv
    }

    $resolvedInput = if (Test-Path -LiteralPath $InputFolder -PathType Container) {
        (Resolve-Path -LiteralPath $InputFolder).Path
    } else {
        Fail-Batch "Input folder was not found: '$InputFolder'."
    }

    $resolvedEval = [System.IO.Path]::GetFullPath($EvalExe)
    $resolvedOutput = [System.IO.Path]::GetFullPath($OutputCsv)

    if (-not (Test-Path -LiteralPath $resolvedEval -PathType Leaf)) {
        Fail-Batch "Flow88MasterEval was not found at '$resolvedEval'."
    }

    Write-Host ("Mode           : {0}" -f $(if ($TestAudioValidation) { "TestAudioValidation" } else { "Custom" }))
    Write-Host ("Evaluator path : {0}" -f $resolvedEval)
    Write-Host ("Input directory: {0}" -f $resolvedInput)
    Write-Host ("Output CSV     : {0}" -f $resolvedOutput)

    $outputDirectory = Split-Path -Parent $resolvedOutput
    if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }

    $allWavFiles = @(Get-ChildItem -LiteralPath $resolvedInput -Filter "*.wav" -File | Sort-Object Name)
    if ($allWavFiles.Count -eq 0) {
        Fail-Batch "No WAV files were found in '$resolvedInput'."
    }

    $knownFilesByBaseAndExtension = @{}
    foreach ($file in $allWavFiles) {
        $key = "{0}|{1}" -f $file.BaseName.ToLowerInvariant(), $file.Extension.ToLowerInvariant()
        $knownFilesByBaseAndExtension[$key] = $true
    }

    $wavFiles = New-Object System.Collections.Generic.List[System.IO.FileInfo]
    $excludedDuplicateFiles = New-Object System.Collections.Generic.List[System.IO.FileInfo]

    foreach ($file in $allWavFiles) {
        if (-not $IncludeWindowsCopyDuplicates -and (Test-IsWindowsCopyDuplicate -File $file -KnownFilesByBaseAndExtension $knownFilesByBaseAndExtension)) {
            $excludedDuplicateFiles.Add($file)
            continue
        }

        $wavFiles.Add($file)
    }

    if ($TestAudioValidation) {
        $corpusLookup = @{}
        foreach ($file in $wavFiles) {
            $corpusLookup[$file.Name] = $file
        }

        $selectedCorpusFiles = New-Object System.Collections.Generic.List[System.IO.FileInfo]
        $missingCorpusFiles = New-Object System.Collections.Generic.List[string]
        foreach ($corpusFileName in (Get-TestAudioCorpusFileNames)) {
            if ($corpusLookup.ContainsKey($corpusFileName)) {
                $selectedCorpusFiles.Add($corpusLookup[$corpusFileName])
            } else {
                $missingCorpusFiles.Add($corpusFileName)
            }
        }

        if ($missingCorpusFiles.Count -gt 0) {
            Fail-Batch ("TestAudioValidation is missing required corpus file(s): {0}" -f ($missingCorpusFiles -join ", "))
        }

        $ignoredNonCorpusFiles = @($wavFiles | Where-Object { $_.Name -notin (Get-TestAudioCorpusFileNames) })
        if ($ignoredNonCorpusFiles.Count -gt 0) {
            Write-Host ("Ignoring {0} non-canonical TestAudio file(s):" -f $ignoredNonCorpusFiles.Count)
            foreach ($ignoredFile in $ignoredNonCorpusFiles) {
                Write-Host ("  [I] {0}" -f $ignoredFile.FullName)
            }
        }

        $wavFiles = $selectedCorpusFiles
    }

    if ($wavFiles.Count -eq 0) {
        Fail-Batch "All WAV files were excluded. Use -IncludeWindowsCopyDuplicates to keep Windows copy files like '(1)'."
    }

    Write-Host ("Discovered {0} WAV file(s) before duplicate filtering." -f $allWavFiles.Count)

    if ($excludedDuplicateFiles.Count -gt 0 -and -not $IncludeWindowsCopyDuplicates) {
        Write-Host ("Excluding {0} Windows copy duplicate(s):" -f $excludedDuplicateFiles.Count)
        for ($duplicateIndex = 0; $duplicateIndex -lt $excludedDuplicateFiles.Count; $duplicateIndex++) {
            Write-Host ("  [X{0}] {1}" -f ($duplicateIndex + 1), $excludedDuplicateFiles[$duplicateIndex].FullName)
        }
    } elseif ($IncludeWindowsCopyDuplicates) {
        Write-Host "Including Windows copy duplicates when present."
    }

    Write-Host ("Evaluating {0} WAV file(s):" -f $wavFiles.Count)
    for ($wavIndex = 0; $wavIndex -lt $wavFiles.Count; $wavIndex++) {
        Write-Host ("  [{0}] {1}" -f ($wavIndex + 1), $wavFiles[$wavIndex].FullName)
    }

    $presetMatrix = @(Get-TestAudioValidationMatrix)
    Write-Host ("Preset/style matrix ({0} combinations):" -f $presetMatrix.Count)
    for ($presetIndex = 0; $presetIndex -lt $presetMatrix.Count; $presetIndex++) {
        $preset = $presetMatrix[$presetIndex]
        Write-Host ("  [{0}] {1} | subgenre={2} | style={3}" -f ($presetIndex + 1), $preset.Preset, $preset.SubgenreId, $preset.StyleId)
    }

    $tempDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("Flow88BatchEval-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $tempDirectory -Force | Out-Null

    $detailRows = New-Object System.Collections.Generic.List[object]
    $totalCombos = $wavFiles.Count * $presetMatrix.Count
    $comboIndex = 0

    for ($fileIndex = 0; $fileIndex -lt $wavFiles.Count; $fileIndex++) {
        $wavFile = $wavFiles[$fileIndex]
        Write-Host ("[{0}/{1}] Track: {2}" -f ($fileIndex + 1), $wavFiles.Count, $wavFile.Name)

        foreach ($preset in $presetMatrix) {
            $comboIndex++
            Write-Host ("  [{0}/{1}] Evaluating {2}" -f $comboIndex, $totalCombos, $preset.Preset)

            $tempReport = Join-Path $tempDirectory ("flow88-" + [guid]::NewGuid().ToString("N") + ".json")
            $arguments = @(
                "--input", $wavFile.FullName,
                "--output", $tempReport,
                "--format", "json",
                "--subgenre", $preset.SubgenreId,
                "--style", $preset.StyleId,
                "--mode", "preset"
            )

            $evalOutput = & $resolvedEval @arguments 2>&1
            $exitCode = $LASTEXITCODE
            if ($exitCode -ne 0) {
                $evalOutputText = if ($evalOutput) {
                    ($evalOutput | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
                } else {
                    "<no output>"
                }

                Fail-Batch ((
                    "Flow88MasterEval failed with exit code {0} for '{1}' using '{2}'.`n{3}" `
                        -f $exitCode, $wavFile.FullName, $preset.Preset, $evalOutputText
                ))
            }

            if (-not (Test-Path -LiteralPath $tempReport -PathType Leaf)) {
                Fail-Batch ((
                    "Flow88MasterEval did not create the expected report file '{0}' for '{1}' using '{2}'." `
                        -f $tempReport, $wavFile.FullName, $preset.Preset
                ))
            }

            $payload = Get-Content -LiteralPath $tempReport -Raw | ConvertFrom-Json
            Remove-Item -LiteralPath $tempReport -Force -ErrorAction SilentlyContinue

            $report = $payload.reports[0]
            $metrics = $report.outputMetrics
            $scores = $report.scores

            $overallScore = [Math]::Round(((
                        [double]$scores.loudnessSafety +
                        [double]$scores.tonalBalanceQuality +
                        (100.0 - [double]$scores.widthRisk) +
                        [double]$scores.lowEndStability +
                        (100.0 - [double]$scores.limiterDamageRisk)
                    ) / 5.0), 2)

            $detailRows.Add((New-ReportRow `
                    -Track $wavFile.BaseName `
                    -Genre $preset.Genre `
                    -Preset $preset.Preset `
                    -Style $preset.Style `
                    -LufsI ([Math]::Round([double]$metrics.integratedLufs, 2)) `
                    -ShortTermLufs ([Math]::Round([double]$metrics.shortTermLufs, 2)) `
                    -TruePeak ([Math]::Round([double]$metrics.truePeakDbTP, 2)) `
                    -Correlation ([Math]::Round([double]$metrics.stereoCorrelation, 3)) `
                    -Width ([Math]::Round([double]$metrics.stereoWidthPct, 2)) `
                    -TonalBalanceDeviation ([Math]::Round([double]$metrics.tonalBalanceDeviation, 3)) `
                    -LowEndStability ([Math]::Round([double]$metrics.lowEndMonoStability, 2)) `
                    -LoudnessSafetyScore ([Math]::Round([double]$scores.loudnessSafety, 2)) `
                    -TonalBalanceScore ([Math]::Round([double]$scores.tonalBalanceQuality, 2)) `
                    -WidthRiskScore ([Math]::Round([double]$scores.widthRisk, 2)) `
                    -LowEndStabilityScore ([Math]::Round([double]$scores.lowEndStability, 2)) `
                    -LimiterDamageScore ([Math]::Round([double]$scores.limiterDamageRisk, 2)) `
                    -OverallScore $overallScore))
        }
    }

    $outputRows = New-Object System.Collections.Generic.List[object]
    foreach ($row in $detailRows) {
        $outputRows.Add($row)
    }

    $outputRows.Add((New-ReportRow))
    $outputRows.Add((New-ReportRow -Track "SUMMARY - BEST PRESET PER TRACK"))

    $bestPerTrack = $detailRows | Group-Object Track | ForEach-Object {
        $_.Group | Sort-Object "Overall Score" -Descending | Select-Object -First 1
    }

    foreach ($row in $bestPerTrack | Sort-Object Track) {
        $outputRows.Add($row)
    }

    $outputRows.Add((New-ReportRow))
    $outputRows.Add((New-ReportRow -Track "SUMMARY - AVERAGE SCORE BY PRESET/STYLE"))

    $averages = $detailRows | Group-Object Genre, Preset, Style | ForEach-Object {
        $first = $_.Group[0]
        New-ReportRow `
            -Track "AVERAGE" `
            -Genre $first.Genre `
            -Preset $first.Preset `
            -Style $first.Style `
            -LufsI (Get-Average -Rows $_.Group -Property "LUFS-I") `
            -ShortTermLufs (Get-Average -Rows $_.Group -Property "Short-term LUFS") `
            -TruePeak (Get-Average -Rows $_.Group -Property "True Peak") `
            -Correlation (Get-Average -Rows $_.Group -Property "Correlation") `
            -Width (Get-Average -Rows $_.Group -Property "Width") `
            -TonalBalanceDeviation (Get-Average -Rows $_.Group -Property "Tonal Balance Deviation") `
            -LowEndStability (Get-Average -Rows $_.Group -Property "Low-End Stability") `
            -LoudnessSafetyScore (Get-Average -Rows $_.Group -Property "Loudness Safety Score") `
            -TonalBalanceScore (Get-Average -Rows $_.Group -Property "Tonal Balance Score") `
            -WidthRiskScore (Get-Average -Rows $_.Group -Property "Width Risk Score") `
            -LowEndStabilityScore (Get-Average -Rows $_.Group -Property "Low-End Stability Score") `
            -LimiterDamageScore (Get-Average -Rows $_.Group -Property "Limiter Damage Score") `
            -OverallScore (Get-Average -Rows $_.Group -Property "Overall Score")
    }

    foreach ($row in $averages | Sort-Object "Overall Score" -Descending) {
        $outputRows.Add($row)
    }

    $outputRows.Add((New-ReportRow))
    $outputRows.Add((New-ReportRow -Track "SUMMARY - WORST RISK FLAGS"))

    $flaggedRows = $detailRows | ForEach-Object {
        $flag = Get-WorstRiskFlag -Row $_
        [pscustomobject]@{
            SourceRow = $_
            FlagName = $flag.Name
            Severity = [Math]::Round([double]$flag.Severity, 2)
        }
    } | Where-Object { $_.Severity -ge 20.0 } | Sort-Object Severity -Descending | Select-Object -First 12

    foreach ($entry in $flaggedRows) {
        $row = $entry.SourceRow
        $outputRows.Add((New-ReportRow `
                -Track ("{0} [{1}]" -f $row.Track, $entry.FlagName) `
                -Genre $row.Genre `
                -Preset $row.Preset `
                -Style $row.Style `
                -LufsI $row."LUFS-I" `
                -ShortTermLufs $row."Short-term LUFS" `
                -TruePeak $row."True Peak" `
                -Correlation $row."Correlation" `
                -Width $row."Width" `
                -TonalBalanceDeviation $row."Tonal Balance Deviation" `
                -LowEndStability $row."Low-End Stability" `
                -LoudnessSafetyScore $row."Loudness Safety Score" `
                -TonalBalanceScore $row."Tonal Balance Score" `
                -WidthRiskScore $row."Width Risk Score" `
                -LowEndStabilityScore $row."Low-End Stability Score" `
                -LimiterDamageScore $row."Limiter Damage Score" `
                -OverallScore $row."Overall Score"))
    }

    Write-Host ("Writing consolidated CSV to: {0}" -f $resolvedOutput)
    $outputRows | Export-Csv -LiteralPath $resolvedOutput -NoTypeInformation -Encoding UTF8

    if (-not (Test-Path -LiteralPath $resolvedOutput -PathType Leaf)) {
        Fail-Batch "The CSV was not created at '$resolvedOutput'."
    }

    $outputFileInfo = Get-Item -LiteralPath $resolvedOutput
    if ($outputFileInfo.Length -le 0) {
        Fail-Batch "The CSV was created but is empty: '$resolvedOutput'."
    }

    Write-Host ("Confirmed CSV creation: {0} ({1} bytes)" -f $outputFileInfo.FullName, $outputFileInfo.Length)
    Write-Host ("Success. Wrote {0} CSV row(s)." -f $outputRows.Count)
    exit 0
} catch {
    $message = if ([string]::IsNullOrWhiteSpace($_.Exception.Message)) {
        $_ | Out-String
    } else {
        $_.Exception.Message
    }

    Write-Error $message
    exit 1
} finally {
    if ($tempDirectory -and (Test-Path -LiteralPath $tempDirectory -PathType Container)) {
        Remove-Item -LiteralPath $tempDirectory -Force -Recurse -ErrorAction SilentlyContinue
    }
}
