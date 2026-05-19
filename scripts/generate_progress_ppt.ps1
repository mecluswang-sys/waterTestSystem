$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$outPath = Join-Path $root 'docs\project_progress_2026-05-19.pptx'

$pp = $null
$pres = $null

function Add-TitleSlide {
    param($presentation, [string]$title, [string]$subtitle)
    $slide = $presentation.Slides.Add($presentation.Slides.Count + 1, 1)
    $slide.Shapes.Title.TextFrame.TextRange.Text = $title
    $slide.Shapes.Item(2).TextFrame.TextRange.Text = $subtitle
}

function Add-BulletsSlide {
    param($presentation, [string]$title, [string[]]$bullets)
    $slide = $presentation.Slides.Add($presentation.Slides.Count + 1, 2)
    $slide.Shapes.Title.TextFrame.TextRange.Text = $title
    $slide.Shapes.Item(2).TextFrame.TextRange.Text = ($bullets -join "`r")
}

function Add-ImageSlide {
    param($presentation, [string]$title, [string]$imagePath, [string]$caption)
    $slide = $presentation.Slides.Add($presentation.Slides.Count + 1, 11)
    $slide.Shapes.Title.TextFrame.TextRange.Text = $title

    if (Test-Path $imagePath) {
        $left = 40
        $top = 90
        $width = 880
        $height = 430
        $null = $slide.Shapes.AddPicture($imagePath, $false, $true, $left, $top, $width, $height)
    }

    $cap = $slide.Shapes.AddTextbox(1, 40, 535, 880, 40)
    $cap.TextFrame.TextRange.Text = $caption
}

try {
    $pp = New-Object -ComObject PowerPoint.Application
    $pp.Visible = $true
    $pres = $pp.Presentations.Add()

    Add-TitleSlide $pres 'Water Medium Test System - Progress Report' 'Date: 2026-05-19'

    Add-BulletsSlide $pres 'Project Goal and Status' @(
        'Goal: deliver Terminal-Station distributed architecture for multi-station operation',
        'Status: main framework completed and integration fixes are ongoing',
        'Progress: critical control and monitoring chains are available'
    )

    Add-BulletsSlide $pres 'Recent Completed Fixes' @(
        'Unified relay labels with displayed glyph names on Station 1',
        'Corrected Valve 3/4 PLC mapping and relay-glyph binding',
        'Added missing Valve 4 static check and 3-valve linkage test',
        'Fixed pressure change wording and absolute-delta judgment logic'
    )

    Add-ImageSlide $pres 'UI and Architecture Reference' (Join-Path $root 'docs\UI-design\industrial_control_system.png') 'Figure: industrial control UI reference'
    Add-ImageSlide $pres 'Wiring and Deployment' (Join-Path $root 'docs\接线图.png') 'Figure: wiring diagram for deployment and commissioning'
    Add-ImageSlide $pres 'Field Device Verification - Flowmeter 1' (Join-Path $root 'docs\流量计1.jpg') 'Figure: on-site device image'
    Add-ImageSlide $pres 'Field Device Verification - Flowmeter 2' (Join-Path $root 'docs\流量计2.jpg') 'Figure: on-site device image'

    Add-BulletsSlide $pres 'Next Stage Plan (2-6 weeks)' @(
        'Complete usable versions of Dashboard, PLC Connection, and Station Manager pages',
        'Finish real-time data chain integration and action logging traceability',
        'Run stability tests including reconnect and long-running verification',
        'Prepare trial-run and acceptance deliverables'
    )

    Add-BulletsSlide $pres 'Conclusion' @(
        'Project is moving forward with the planned architecture direction',
        'Key integration issues are being closed in a stable feedback loop',
        'Next focus: feature completeness and operational stability'
    )

    $pres.SaveAs($outPath)
    Write-Output ('PPTX_CREATED:' + $outPath)
}
catch {
    Write-Output ('PPTX_CREATE_FAILED:' + $_.Exception.Message)
    exit 1
}
finally {
    if ($pres -ne $null) {
        try { $pres.Close() } catch {}
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($pres)
    }
    if ($pp -ne $null) {
        try { $pp.Quit() } catch {}
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($pp)
    }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}
