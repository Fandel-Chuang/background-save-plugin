# Background Save Plugin - Test Runner Script
# PowerShell版本

Write-Host "Background Save Plugin - Test Runner" -ForegroundColor Green
Write-Host "====================================" -ForegroundColor Green
Write-Host ""

# 检查是否在正确目录
if (!(Test-Path "build")) {
    Write-Host "Error: build directory not found. Please run from project root." -ForegroundColor Red
    exit 1
}

Set-Location build

Write-Host "Available commands:" -ForegroundColor Yellow
Write-Host "  1. Run full test suite" -ForegroundColor White
Write-Host "  2. Run functional tests" -ForegroundColor White
Write-Host "  3. Run performance benchmarks" -ForegroundColor White
Write-Host "  4. Inspect RDB file" -ForegroundColor White
Write-Host "  5. Run all tests" -ForegroundColor White
Write-Host "  6. Exit" -ForegroundColor White
Write-Host ""

do {
    $choice = Read-Host "Select option (1-6)"

    switch ($choice) {
        "1" {
            Write-Host "`nRunning full test suite..." -ForegroundColor Cyan
            .\test_suite.exe --all --verbose
        }
        "2" {
            Write-Host "`nRunning functional tests..." -ForegroundColor Cyan
            .\test_functional.exe
        }
        "3" {
            Write-Host "`nRunning performance benchmarks..." -ForegroundColor Cyan
            .\test_benchmark.exe
        }
        "4" {
            Write-Host "`nInspecting RDB file..." -ForegroundColor Cyan
            .\rdb_inspector.exe ../tests/output/test_dump.rdb
        }
        "5" {
            Write-Host "`nRunning all tests..." -ForegroundColor Cyan
            Write-Host "=== Test Suite ===" -ForegroundColor Yellow
            .\test_suite.exe --all --verbose
            Write-Host "`n=== Functional Tests ===" -ForegroundColor Yellow
            .\test_functional.exe
            Write-Host "`n=== Performance Benchmarks ===" -ForegroundColor Yellow
            .\test_benchmark.exe
            Write-Host "`n=== RDB Inspector ===" -ForegroundColor Yellow
            .\rdb_inspector.exe ../tests/output/test_dump.rdb
        }
        "6" {
            Write-Host "Goodbye!" -ForegroundColor Green
            break
        }
        default {
            Write-Host "Invalid choice. Please select 1-6." -ForegroundColor Red
        }
    }

    if ($choice -ne "6") {
        Write-Host "`nPress any key to continue..." -ForegroundColor Gray
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        Clear-Host
        Write-Host "Background Save Plugin - Test Runner" -ForegroundColor Green
        Write-Host "====================================" -ForegroundColor Green
        Write-Host ""
        Write-Host "Available commands:" -ForegroundColor Yellow
        Write-Host "  1. Run full test suite" -ForegroundColor White
        Write-Host "  2. Run functional tests" -ForegroundColor White
        Write-Host "  3. Run performance benchmarks" -ForegroundColor White
        Write-Host "  4. Inspect RDB file" -ForegroundColor White
        Write-Host "  5. Run all tests" -ForegroundColor White
        Write-Host "  6. Exit" -ForegroundColor White
        Write-Host ""
    }

} while ($choice -ne "6")

Set-Location ..