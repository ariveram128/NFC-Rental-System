# generate_directory.ps1
param(
    [string]$Root = "RentScan_v3"
)

# Define directories to create
$dirs = @(
    "$Root",
    "$Root\docs",
    "$Root\firmware",
    "$Root\firmware\include",
    "$Root\firmware\src",
    "$Root\gateway",
    "$Root\backend_sim",
    "$Root\hardware"
)

# Create directories
foreach ($d in $dirs) {
    if (-not (Test-Path $d)) {
        New-Item -ItemType Directory -Path $d -Force | Out-Null
    }
}

# Define files to touch
$files = @(
    "$Root\.gitignore",
    "$Root\README.md",
    "$Root\sdk_config.h",
    "$Root\docs\system_diagram.png",
    "$Root\docs\nfc_data_flow.png",
    "$Root\docs\hardware_requirements.md",
    "$Root\firmware\Makefile",
    "$Root\firmware\include\nfc_handler.h",
    "$Root\firmware\include\rental_service.h",
    "$Root\firmware\include\rental_data.h",
    "$Root\firmware\include\storage_manager.h",
    "$Root\firmware\src\main.c",
    "$Root\firmware\src\nfc_handler.c",
    "$Root\firmware\src\rental_service.c",
    "$Root\firmware\src\rental_data.c",
    "$Root\firmware\src\storage_manager.c",
    "$Root\gateway\requirements.txt",
    "$Root\gateway\gateway_listener.py",
    "$Root\backend_sim\simulate_backend.py",
    "$Root\hardware\antenna_schem.pdf"
)

# Create files
foreach ($f in $files) {
    $dir = Split-Path $f
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    New-Item -ItemType File -Path $f -Force | Out-Null
}

Write-Host "Directory structure for '$Root' has been created."
