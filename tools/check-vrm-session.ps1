param(
    [Parameter(Mandatory=$true)][string]$HostPath,
    [Parameter(Mandatory=$true)][string]$Avatar0,
    [Parameter(Mandatory=$true)][string]$Avatar1,
    [Parameter(Mandatory=$true)][string]$Clip,
    [Parameter(Mandatory=$true)][string]$EvidencePath,
    [double]$FixedStepSeconds = (1.0 / 60),
    [int]$Ticks = 120,
    [switch]$Face
)
$ErrorActionPreference = 'Stop'
# Caller supplies installed USD/plugin/DLL discovery. Assets are never copied.
$taskHost = (Resolve-Path -LiteralPath $HostPath).Path
$taskAssets = @($Avatar0, $Avatar1, $Clip) | ForEach-Object { (Resolve-Path -LiteralPath $_).Path }
$taskRuns = @()
foreach ($taskAvatar in $taskAssets[0..1]) {
    foreach ($taskSecondary in @($true, $false)) {
        $taskArguments = @($taskAvatar, $taskAssets[2], $FixedStepSeconds.ToString('R', [Globalization.CultureInfo]::InvariantCulture), "$Ticks")
        if (-not $taskSecondary) { $taskArguments += '--no-springs' }
        if ($Face) { $taskArguments += '--face' }
        $taskOutput = (& $taskHost @taskArguments 2>&1 | Out-String)
        $taskExit = $LASTEXITCODE
        Write-Output $taskOutput.Trim()
        $taskRuns += [ordered]@{ avatar=$taskAvatar; secondary=$taskSecondary; exit=$taskExit; output=$taskOutput }
    }
}
[ordered]@{
    fixed_step_seconds=$FixedStepSeconds; ticks=$Ticks; executable=$taskHost
    assets=@($taskAssets | ForEach-Object { [ordered]@{ path=$_; sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash } })
    runs=$taskRuns
    scope='Headless owner/runtime and USD pose transport; exact reset replay and retained unload. No resident mesh, Hydra/direct renderer or pixel parity.'
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $EvidencePath -Encoding utf8
if (@($taskRuns | Where-Object { $_.exit -ne 0 }).Count) { throw "VRM session validation failed; see $EvidencePath" }
