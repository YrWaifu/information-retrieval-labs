# PowerShell script to compile LaTeX document to PDF
# Usage: .\compile.ps1

$filename = "report"

Write-Host "Compiling $filename.tex to PDF..." -ForegroundColor Cyan

# Check if pdflatex is installed
$pdflatexCmd = Get-Command pdflatex -ErrorAction SilentlyContinue

if (-not $pdflatexCmd) {
    Write-Host ""
    Write-Host "ERROR: pdflatex not found in PATH." -ForegroundColor Red
    Write-Host ""
    Write-Host "To install LaTeX:" -ForegroundColor Yellow
    Write-Host "1. MiKTeX: https://miktex.org/download" -ForegroundColor Cyan
    Write-Host "2. TeX Live: https://www.tug.org/texlive/windows.html" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "After installation, restart terminal or add LaTeX path to PATH." -ForegroundColor Yellow
    exit 1
}

# Run compilation (non-interactive)
pdflatex -interaction=nonstopmode "$filename.tex"

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "Compilation successful! PDF file: $filename.pdf" -ForegroundColor Green
    Write-Host ""
    Write-Host "Note: You may need to run compilation twice for table of contents." -ForegroundColor Yellow
} else {
    Write-Host ""
    Write-Host "ERROR: Compilation failed. Exit code: $LASTEXITCODE" -ForegroundColor Red
    Write-Host "Check logs above for details." -ForegroundColor Yellow
    exit $LASTEXITCODE
}
