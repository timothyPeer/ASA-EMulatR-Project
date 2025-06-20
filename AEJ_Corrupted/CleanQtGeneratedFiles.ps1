
# PowerShell script to clean Qt moc/ui/qrc generated files and build artifacts

Write-Host "Cleaning Qt generated files and build artifacts..."

# Set root path to current directory
$projectRoot = Get-Location

# Define file patterns to remove
$filePatterns = @("moc_*.cpp", "ui_*.h", "qrc_*.cpp", "*.moc", "*.qm")

foreach ($pattern in $filePatterns) {
    Get-ChildItem -Path $projectRoot -Recurse -Include $pattern -ErrorAction SilentlyContinue | Remove-Item -Force
}

# Remove known Qt and build folders if present
$foldersToDelete = @("build", "GeneratedFiles", ".qmake.stash", ".qtbuild", "x64\Debug", "x64\Release")

foreach ($folder in $foldersToDelete) {
    $fullPath = Join-Path $projectRoot $folder
    if (Test-Path $fullPath) {
        Remove-Item -Recurse -Force -Path $fullPath
        Write-Host "Removed folder: $fullPath"
    }
}

Write-Host "Cleanup complete. You can now rebuild your project in Visual Studio."
