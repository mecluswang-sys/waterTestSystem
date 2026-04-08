param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$BuildType = "Release"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$workspaceRoot = Split-Path -Parent $scriptDir

$buildScript = Join-Path $workspaceRoot "scripts/rebuild.ps1"
$deployScript = Join-Path $workspaceRoot "scripts/deploy.ps1"
$startScript = Join-Path $workspaceRoot "deploy/start.bat"

Push-Location $workspaceRoot

function Invoke-ScriptBlock([string]$label, [scriptblock]$block) {
    Write-Host "" -ForegroundColor Cyan
    Write-Host "=== $label ===" -ForegroundColor Cyan
    & $block
    if ($LASTEXITCODE -ne 0) {
        Write-Host "*** $label failed (exit code $LASTEXITCODE)" -ForegroundColor Red
        Pop-Location
        exit $LASTEXITCODE
    }
}

try {
    Write-Host "Restarting Water Test System" -ForegroundColor Green
    Invoke-ScriptBlock "Full Build ($BuildType)" { & $buildScript -BuildType $BuildType }
    Invoke-ScriptBlock "Deploy ($BuildType)" { & $deployScript -BuildType $BuildType }

    if (-not (Test-Path $startScript)) {
        Write-Host "Warning: $startScript not found, skipping launcher start." -ForegroundColor Yellow
    } else {
        Write-Host "" -ForegroundColor Cyan
        Write-Host "Starting launcher" -ForegroundColor Cyan
        Start-Process -FilePath $startScript -WorkingDirectory (Split-Path -Parent $startScript)
    }
}
finally {
    Pop-Location
}
