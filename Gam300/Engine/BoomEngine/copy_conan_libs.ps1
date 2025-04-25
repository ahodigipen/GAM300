# Set source and destination folders
$sourceBase = "libraries/debug"
$includeDest = "$sourceBase/include"
$libDest = "$sourceBase/lib"
$binDest = "$sourceBase/dlls"

# Conan cache directory
$conanCache = "$env:USERPROFILE\.conan2\p"

# Create destination folders
New-Item -ItemType Directory -Force -Path $includeDest | Out-Null
New-Item -ItemType Directory -Force -Path $libDest | Out-Null
New-Item -ItemType Directory -Force -Path $binDest | Out-Null

Write-Host " Copying headers, .lib, and .dll files into libraries/debug folders..."

# Copy includes
$includeFolders = Get-ChildItem -Path "$sourceBase" -Recurse -Directory -Filter "include"
foreach ($folder in $includeFolders) {
    Write-Host " Copying includes from: $($folder.FullName)"
    Copy-Item "$($folder.FullName)\*" $includeDest -Recurse -Force
}

# Copy .lib files
$libFolders = Get-ChildItem -Path "$sourceBase" -Recurse -Directory -Filter "lib"
foreach ($folder in $libFolders) {
    $libs = Get-ChildItem "$($folder.FullName)\*.lib" -ErrorAction SilentlyContinue
    foreach ($lib in $libs) {
        Write-Host " Copying lib: $($lib.Name)"
        Copy-Item $lib.FullName $libDest -Force
    }
}

# Copy .dll files from Conan cache directly
$dlls = Get-ChildItem -Path "$conanCache" -Recurse -Filter *.dll -ErrorAction SilentlyContinue
foreach ($dll in $dlls) {
    Write-Host " Copying DLL: $($dll.Name)"
    Copy-Item $dll.FullName $binDest -Force
}

Write-Host " Done!"
Write-Host "Include path to $includeDest"
Write-Host "Library path to $libDest"
Write-Host "DLL path     to $binDest"
