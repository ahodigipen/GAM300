#!/bin/bash
set -e

# Target include output folder
DEST="libraries/debug/include"

# Conan package cache directory
CONAN_CACHE="$HOME/.conan2/p"

# Create destination if needed
mkdir -p "$DEST"

echo "Copying Conan package headers into: $DEST"

# List of Conan packages you want to extract headers from
PACKAGES=("glm" "glfw" "glew" "imgui")

for pkg in "${PACKAGES[@]}"; do
    # Find the include folder inside Conan cache for each package
    path=$(find "$CONAN_CACHE" -type d -path "*/$pkg*/p/include" | head -n 1)

    if [[ -n "$path" ]]; then
        echo "Copying from: $path"
        cp -r "$path/"* "$DEST/"
    else
        echo "⚠️ Could not find headers for package: $pkg"
    fi
done

echo "✅ Header sync complete. Add '$DEST' to your Visual Studio include paths."
