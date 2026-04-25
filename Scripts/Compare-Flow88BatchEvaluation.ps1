[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BaselineCsv,

    [Parameter(Mandatory = $true)]
    [string]$CandidateCsv
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Fail-Comparison {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    throw $Message
}

function Resolve-ComparisonPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Fail-Comparison "CSV was not found: '$Path'."
    }

    return (Resolve-Path -LiteralPath $Path).Path
}

function Convert-ToNumber {
    param(
        [Parameter(Mandatory = $false)]
        [AllowNull()]
        [object]$Value
    )

    if ($null -eq $Value -or [string]::IsNullOrWhiteSpace([string]$Value)) {
        return 0.0
    }

    return [double]$Value
}

function Get-DetailRows {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Rows
    )

    $detailRows = @()
    foreach ($row in @($Rows)) {
        if ([string]::IsNullOrWhiteSpace([string]$row.Track)) {
            break
        }

        $detailRows += $row
    }

    return @($detailRows)
}

function Get-AverageRows {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Rows
    )

    return @(@($Rows) | Where-Object { $_.Track -eq "AVERAGE" })
}

function Get-FamilyKey {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Row
    )

    return "{0}|{1}|{2}" -f $Row.Genre, $Row.Preset, $Row.Style
}

function Get-FamilyLabel {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Row
    )

    return [string]$Row.Preset
}

function Format-SignedNumber {
    param(
        [Parameter(Mandatory = $true)]
        [double]$Value,

        [int]$Decimals = 2
    )

    $format = "F{0}" -f $Decimals
    $text = $Value.ToString($format, [System.Globalization.CultureInfo]::InvariantCulture)
    if ($Value -gt 0.0) {
        return "+" + $text
    }

    return $text
}

$hasRegression = $false

try {
    $resolvedBaseline = Resolve-ComparisonPath -Path $BaselineCsv
    $resolvedCandidate = Resolve-ComparisonPath -Path $CandidateCsv

    Write-Host ("Baseline CSV : {0}" -f $resolvedBaseline)
    Write-Host ("Candidate CSV: {0}" -f $resolvedCandidate)

    $baselineRows = @(Import-Csv -LiteralPath $resolvedBaseline)
    $candidateRows = @(Import-Csv -LiteralPath $resolvedCandidate)

    if ($baselineRows.Count -eq 0) {
        Fail-Comparison "Baseline CSV is empty: '$resolvedBaseline'."
    }

    if ($candidateRows.Count -eq 0) {
        Fail-Comparison "Candidate CSV is empty: '$resolvedCandidate'."
    }

    $baselineDetailRows = @(Get-DetailRows -Rows $baselineRows)
    $candidateDetailRows = @(Get-DetailRows -Rows $candidateRows)
    $baselineAverageRows = @(Get-AverageRows -Rows $baselineRows)
    $candidateAverageRows = @(Get-AverageRows -Rows $candidateRows)

    if ($baselineDetailRows.Count -eq 0 -or $candidateDetailRows.Count -eq 0) {
        Fail-Comparison "Could not find detail rows in one or both CSV files."
    }

    if ($baselineAverageRows.Count -eq 0 -or $candidateAverageRows.Count -eq 0) {
        Fail-Comparison "Could not find AVERAGE rows in one or both CSV files."
    }

    $candidatePositiveRows = @(
        $candidateDetailRows |
            Where-Object { (Convert-ToNumber $_."True Peak") -gt 0.0 } |
            Sort-Object @{ Expression = { Convert-ToNumber $_."True Peak" }; Descending = $true }, Track, Preset
    )

    $baselineAveragesByFamily = @{}
    foreach ($row in $baselineAverageRows) {
        $baselineAveragesByFamily[(Get-FamilyKey -Row $row)] = $row
    }

    $candidateAveragesByFamily = @{}
    foreach ($row in $candidateAverageRows) {
        $candidateAveragesByFamily[(Get-FamilyKey -Row $row)] = $row
    }

    $missingFamilies = New-Object System.Collections.Generic.List[string]
    $scoreRegressions = New-Object System.Collections.Generic.List[object]
    $tonalRegressions = New-Object System.Collections.Generic.List[object]
    $averageScoreDeltas = New-Object System.Collections.Generic.List[object]

    foreach ($familyKey in $baselineAveragesByFamily.Keys | Sort-Object) {
        if (-not $candidateAveragesByFamily.ContainsKey($familyKey)) {
            $missingFamilies.Add($familyKey)
            continue
        }

        $baselineRow = $baselineAveragesByFamily[$familyKey]
        $candidateRow = $candidateAveragesByFamily[$familyKey]

        $baselineScore = Convert-ToNumber $baselineRow."Overall Score"
        $candidateScore = Convert-ToNumber $candidateRow."Overall Score"
        $baselineTonalDeviation = Convert-ToNumber $baselineRow."Tonal Balance Deviation"
        $candidateTonalDeviation = Convert-ToNumber $candidateRow."Tonal Balance Deviation"

        $averageScoreDeltas.Add([pscustomobject]@{
                Family                  = Get-FamilyLabel -Row $candidateRow
                BaselineScore           = [Math]::Round($baselineScore, 2)
                CandidateScore          = [Math]::Round($candidateScore, 2)
                ScoreDelta              = [Math]::Round($candidateScore - $baselineScore, 2)
                BaselineTonalDeviation  = [Math]::Round($baselineTonalDeviation, 3)
                CandidateTonalDeviation = [Math]::Round($candidateTonalDeviation, 3)
                TonalDelta              = [Math]::Round($candidateTonalDeviation - $baselineTonalDeviation, 3)
            })

        if ($candidateScore + 0.0001 -lt $baselineScore) {
            $scoreRegressions.Add([pscustomobject]@{
                    Family    = Get-FamilyLabel -Row $candidateRow
                    Baseline  = [Math]::Round($baselineScore, 2)
                    Candidate = [Math]::Round($candidateScore, 2)
                    Delta     = [Math]::Round($candidateScore - $baselineScore, 2)
                })
        }

        if ($candidateTonalDeviation -gt ($baselineTonalDeviation + 0.0001)) {
            $tonalRegressions.Add([pscustomobject]@{
                    Family    = Get-FamilyLabel -Row $candidateRow
                    Baseline  = [Math]::Round($baselineTonalDeviation, 3)
                    Candidate = [Math]::Round($candidateTonalDeviation, 3)
                    Delta     = [Math]::Round($candidateTonalDeviation - $baselineTonalDeviation, 3)
                })
        }
    }

    foreach ($familyKey in $candidateAveragesByFamily.Keys) {
        if (-not $baselineAveragesByFamily.ContainsKey($familyKey)) {
            $missingFamilies.Add($familyKey)
        }
    }

    Write-Host ""
    Write-Host "Positive dBTP rows in candidate:"
    if ($candidatePositiveRows.Count -eq 0) {
        Write-Host "  none"
    } else {
        $hasRegression = $true
        foreach ($row in $candidatePositiveRows) {
            Write-Host ("  {0} / {1}: {2} dBTP" -f $row.Track, $row.Preset, ([Math]::Round((Convert-ToNumber $row."True Peak"), 2)))
        }
    }

    Write-Host ""
    Write-Host "Average preset family scores:"
    foreach ($entry in $averageScoreDeltas | Sort-Object `
            @{ Expression = { $_.CandidateScore }; Descending = $true }, `
            @{ Expression = { $_.Family }; Descending = $false }) {
        Write-Host ("  {0}: {1} -> {2} ({3}) | tonal {4} -> {5} ({6})" -f `
                $entry.Family,
                $entry.BaselineScore,
                $entry.CandidateScore,
                (Format-SignedNumber -Value $entry.ScoreDelta -Decimals 2),
                $entry.BaselineTonalDeviation,
                $entry.CandidateTonalDeviation,
                (Format-SignedNumber -Value $entry.TonalDelta -Decimals 3))
    }

    Write-Host ""
    Write-Host "Score regressions:"
    if ($scoreRegressions.Count -eq 0) {
        Write-Host "  none"
    } else {
        $hasRegression = $true
        foreach ($row in $scoreRegressions | Sort-Object Delta, Family) {
            Write-Host ("  {0}: {1} -> {2} ({3})" -f `
                    $row.Family,
                    $row.Baseline,
                    $row.Candidate,
                    (Format-SignedNumber -Value $row.Delta -Decimals 2))
        }
    }

    Write-Host ""
    Write-Host "Tonal deviation regressions:"
    if ($tonalRegressions.Count -eq 0) {
        Write-Host "  none"
    } else {
        $hasRegression = $true
        foreach ($row in $tonalRegressions | Sort-Object `
                @{ Expression = { $_.Delta }; Descending = $true }, `
                @{ Expression = { $_.Family }; Descending = $false }) {
            Write-Host ("  {0}: {1} -> {2} ({3})" -f `
                    $row.Family,
                    $row.Baseline,
                    $row.Candidate,
                    (Format-SignedNumber -Value $row.Delta -Decimals 3))
        }
    }

    if ($missingFamilies.Count -gt 0) {
        $hasRegression = $true
        Write-Host ""
        Write-Host "Family coverage mismatch:"
        foreach ($familyKey in $missingFamilies | Sort-Object -Unique) {
            Write-Host ("  {0}" -f $familyKey)
        }
    }

    Write-Host ""
    if ($hasRegression) {
        Write-Host "Regression check: FAIL"
        exit 1
    }

    Write-Host "Regression check: PASS"
    exit 0
} catch {
    $message = if ([string]::IsNullOrWhiteSpace($_.Exception.Message)) {
        $_ | Out-String
    } else {
        $_.Exception.Message
    }

    Write-Error $message
    exit 1
}
