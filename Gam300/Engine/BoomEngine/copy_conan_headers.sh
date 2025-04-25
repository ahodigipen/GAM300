#!/bin/bash
set -e

# Target output folders
INCLUDE_DEST="libraries/debug/include"
LIB_DEST="libraries/debug/lib"

# Conan package cache directory
CONAN_CACHE="$HOME/.conan2/p"

# Create destination folders if needed
mkdir -p "$INCLUDE_DEST"
mkdir -p "$LIB_DEST"

echo "📁 Copying Conan package headers and libs into:"
echo "   Includes ➜ $INCLUDE_DEST"
echo "   Libs     ➜ $LIB_DEST"

# List of Conan packages to extract headers and libs from
PACKAGES=("glm" "glfw" "glew" "imgui")

for pkg in "${PACKAGES[@]}"; do
    echo "🔍 Processing package: $pkg"

    # Copy headers
    inc_path=$(find "$CONAN_CACHE" -type d -path "*/$pkg*/p/include" | head -n 1)
    if [[ -n "$inc_path" ]]; then
        echo "📄 Copying headers from: $inc_path"
        cp -r "$inc_path/"* "$INCLUDE_DEST/"
    else
        echo "⚠️ No include folder found for $pkg"
    fi

    # Copy .lib files (Windows static or import libraries)
    lib_path=$(find "$CONAN_CACHE" -type d -path "*/$pkg*/p/lib" | head -n 1)
    if [[ -n "$lib_path" ]]; then
        echo "📦 Copying .lib files from: $lib_path"
        cp "$lib_path/"*.lib "$LIB_DEST/" 2>/dev/null || echo "⚠️ No .lib files found for $pkg"
    else
        echo "⚠️ No lib folder found for $pkg"
    fi
done

echo "✅ Header and library sync complete."
echo "👉 Add these to your Visual Studio project:"
echo "   Include path: $INCLUDE_DEST"
echo "   Lib path:     $LIB_DEST"
