param(
    [string]$ChromePath = 'C:\Program Files\Google\Chrome\Application\chrome.exe'
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$htmlPath = Join-Path $projectRoot 'papers\INFORME_FINAL.html'
$pdfPath = Join-Path $projectRoot 'papers\INFORME_FINAL.pdf'
$htmlUri = ([uri]$htmlPath).AbsoluteUri
$buildPath = Join-Path $projectRoot 'build'
$runId = [guid]::NewGuid().ToString('N')
$generatedPdfs = [System.Collections.Generic.List[string]]::new()

if (-not (Test-Path -LiteralPath $ChromePath -PathType Leaf)) {
    throw "No se encontró Chrome en $ChromePath. Indique -ChromePath con la ruta correcta."
}

function Wait-PdfReady([string]$Path) {
    $previousSize = -1
    $stableChecks = 0
    for ($check = 0; $check -lt 120; $check++) {
        if (Test-Path -LiteralPath $Path -PathType Leaf) {
            $size = (Get-Item -LiteralPath $Path).Length
            if ($size -gt 10000 -and $size -eq $previousSize) {
                $stableChecks++
                if ($stableChecks -ge 2) { return }
            }
            else {
                $stableChecks = 0
            }
            $previousSize = $size
        }
        Start-Sleep -Milliseconds 250
    }
    throw "Chrome no terminó de escribir $Path dentro de 30 segundos."
}

Push-Location $projectRoot
try {
    python scripts/export_report.py
    if ($LASTEXITCODE -ne 0) { throw 'Falló la generación del HTML preliminar.' }

    $currentPdf = Join-Path $buildPath "report_${runId}_0.pdf"
    & $ChromePath --headless --disable-gpu --disable-background-networking --no-first-run --no-pdf-header-footer "--print-to-pdf=$currentPdf" $htmlUri
    if ($LASTEXITCODE -ne 0) { throw 'Falló la impresión preliminar del PDF.' }
    Wait-PdfReady $currentPdf
    $generatedPdfs.Add($currentPdf)

    $indexReady = $false
    for ($pass = 0; $pass -lt 4; $pass++) {
        python scripts/export_report.py --toc-pdf $currentPdf
        if ($LASTEXITCODE -ne 0) { throw 'Falló el cálculo del índice.' }

        $nextPdf = Join-Path $buildPath "report_${runId}_$($pass + 1).pdf"
        & $ChromePath --headless --disable-gpu --disable-background-networking --no-first-run --no-pdf-header-footer "--print-to-pdf=$nextPdf" $htmlUri
        if ($LASTEXITCODE -ne 0) { throw 'Falló la impresión final del PDF.' }
        Wait-PdfReady $nextPdf
        $generatedPdfs.Add($nextPdf)
        $currentPdf = $nextPdf

        python scripts/export_report.py --check-toc-pdf $currentPdf
        if ($LASTEXITCODE -eq 0) {
            $indexReady = $true
            break
        }
    }
    if (-not $indexReady) { throw 'El índice no se estabilizó tras cuatro pasadas.' }

    python scripts/export_report.py --number-pdf $currentPdf
    if ($LASTEXITCODE -ne 0) { throw 'Falló la numeración de páginas.' }

    Copy-Item -LiteralPath $currentPdf -Destination $pdfPath -Force
    if ((Get-FileHash -LiteralPath $currentPdf -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $pdfPath -Algorithm SHA256).Hash) {
        throw 'La copia del PDF final no coincide con el archivo verificado.'
    }
    Write-Output "PDF final verificado: $pdfPath"
}
finally {
    foreach ($generatedPdf in $generatedPdfs) {
        if (Test-Path -LiteralPath $generatedPdf -PathType Leaf) {
            Remove-Item -LiteralPath $generatedPdf -Force
        }
    }
    Pop-Location
}
