#!/usr/bin/env bash

set -u

echo "SAORS for GTA III Classic - SteamOS/Arch development environment check"
echo "This script reports missing tools; it never runs sudo or installs packages."
echo

if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    source /etc/os-release
    case "${ID:-unknown}" in
        steamos|arch)
            echo "[ok] Detected ${PRETTY_NAME:-$ID}."
            ;;
        *)
            echo "[info] Detected ${PRETTY_NAME:-unknown Linux}; commands below target Arch/SteamOS."
            ;;
    esac
else
    echo "[warning] /etc/os-release is unavailable; distribution could not be detected."
fi

missing=()
for command in git cmake ninja clang i686-w64-mingw32-g++; do
    if command -v "$command" >/dev/null 2>&1; then
        echo "[ok] $command -> $(command -v "$command")"
    else
        echo "[missing] $command"
        missing+=("$command")
    fi
done

echo
if ((${#missing[@]} > 0)); then
    echo "Install the missing Arch packages after reviewing them:"
    echo "  sudo pacman -S --needed git cmake ninja clang mingw-w64-gcc"
    echo "SteamOS may require disabling read-only mode or using a development container."
    echo "This script intentionally does not change the system."
    exit 1
fi

echo "Debug cross-build commands:"
echo "  cmake --preset linux-mingw-x86-debug"
echo "  cmake --build --preset linux-mingw-x86-debug"
echo
echo "Release cross-build commands:"
echo "  cmake --preset linux-mingw-x86-release"
echo "  cmake --build --preset linux-mingw-x86-release"
echo
echo "The generated SAORSForGTA3.asi is a Windows x86 DLL for GTA III under Proton/Wine."
echo "Cross-compiled Windows tests are not executed directly. Run host tests separately as documented."
