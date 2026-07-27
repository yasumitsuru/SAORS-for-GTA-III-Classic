# Building on SteamOS or Arch Linux

This process cross-compiles a Windows x86 DLL. It does not create a native Linux
plugin.

## Toolchain

Check the environment:

```bash
chmod +x tools/setup_steamos.sh
./tools/setup_steamos.sh
```

On Arch-based systems, the expected packages are approximately:

```bash
sudo pacman -S --needed git cmake ninja clang mingw-w64-gcc
```

Review package names for the active distribution. SteamOS system partitions may be
read-only; a development container is often preferable to changing the base OS.
The setup script never invokes `sudo`.

The required cross compiler command is:

```text
i686-w64-mingw32-g++
```

## Cross-build

```bash
cmake --preset linux-mingw-x86-debug
cmake --build --preset linux-mingw-x86-debug

cmake --preset linux-mingw-x86-release
cmake --build --preset linux-mingw-x86-release
```

The release artifact is normally:

```text
build/linux-mingw-x86-release/bin/SAORSForGTA3.asi
build/linux-mingw-x86-release/bin/saors_stream_probe.exe
build/linux-mingw-x86-release/bin/saors_exe_probe.exe
```

Inspect it with `file`; it should report a PE32 Windows DLL for Intel 80386.

## Native tests

Do not execute a cross-compiled Windows test binary directly on Linux. Configure a
separate native tree with the ASI disabled:

```bash
cmake -S . -B build/host-tests -G Ninja \
  -DSAORS_BUILD_ASI=OFF \
  -DSAORS_BUILD_TESTS=ON \
  -DSAORS_BUILD_EXE_PROBE=OFF \
  -DSAORS_ENABLE_WINHTTP=OFF
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

CI follows this separation.

The MinGW i686 target enables `SAORS_ENABLE_WINHTTP` by default and links the
Windows `winhttp` system library. Native Linux host tests compile
`PlaylistResolver` against fake clients and executable fingerprinting against a
fake `FileHasher`; they never compile `WinHttpClient` or the Windows BCrypt
implementation.

## Cross-building the optional libVLC backend

Supply an extracted official VLC Win32 **7z** archive containing `sdk/`, the x86
DLLs, and `plugins/`. Do not use the Win64 archive. Configure a separate tree:

```bash
cmake -S . -B build/linux-mingw-x86-libvlc -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-mingw32.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DSAORS_BUILD_ASI=ON \
  -DSAORS_BUILD_TESTS=OFF \
  -DSAORS_BUILD_STREAM_PROBE=ON \
  -DSAORS_ENABLE_LIBVLC=ON \
  -DSAORS_LIBVLC_ROOT="$PWD/build/deps/vlc-3.0.23"
cmake --build build/linux-mingw-x86-libvlc
file build/linux-mingw-x86-libvlc/bin/saors_stream_probe.exe
```

The backend dynamically resolves libVLC, so the resulting executable is not
loader-dependent on an absolute developer path. The complete runtime is still
required when the executable runs.

## Wine stream-probe procedure

Use a dedicated prefix and keep the VLC runtime adjacent to the probe:

```bash
mkdir -p build/wine-probe/bin/vlc
cp build/linux-mingw-x86-libvlc/bin/saors_stream_probe.exe build/wine-probe/bin/
cp -a build/deps/vlc-3.0.23/. build/wine-probe/bin/vlc/

export WINEPREFIX="$PWD/build/wine-probe/prefix"
export WINEARCH=win32
wineboot -u
WINEDEBUG=+loaddll,+seh wine \
  build/wine-probe/bin/saors_stream_probe.exe \
  --url "https://authorized.example/stream" \
  --duration 30 --pause-after 10 --reconnect-after 20 \
  2>build/wine-probe/wine.log
```

Confirm audible output and record the stream format, transport, Wine version,
probe exit code, state sequence, and sanitized log. Test HTTP MP3, HTTPS MP3, and
AAC separately; one result does not establish the others.

For Proton, create a separate test prefix rather than reusing the game prefix.
Invoke the same Windows executable with the Proton version selected for the game,
capture `PROTON_LOG=1`, and keep `libvlc.dll`, `libvlccore.dll`, and `plugins/`
together under the adjacent `vlc/` directory. Proton launch details differ between
Steam installations, so record the exact command and version used.

No Wine or Proton stream-probe result is claimed by this repository yet.

For a station whose configured URL is a playlist, add
`--allow-http-streams` only when its final HTTP media is intentionally accepted.
WinHTTP behavior under Wine and Proton, including system certificate stores and
proxy configuration, is still unverified.

## Proton/Wine development installation

1. Locate the Steam library containing the classic Windows GTA III installation.
2. Locate its Proton prefix under a path shaped like
   `steamapps/compatdata/<APP_ID>/pfx`. The actual library root and AppID vary; this
   project intentionally does not guess them.
3. Download a compatible 32-bit release of Ultimate ASI Loader from its official
   project. Do not commit its proxy DLL to this repository.
4. Follow the loader documentation to choose the correct proxy name, commonly
   `dinput8.dll`, and place it beside `gta3.exe`.
5. Place `SAORSForGTA3.asi` and `SAORSForGTA3.ini` beside `gta3.exe`.
6. If Proton does not prefer the native proxy, a launch option may be required:

   ```text
   WINEDLLOVERRIDES="dinput8=n,b" %command%
   ```

   Replace `dinput8` if a different documented proxy DLL is used.
7. For diagnosis, temporarily enable Proton logging:

   ```text
   PROTON_LOG=1 WINEDLLOVERRIDES="dinput8=n,b" %command%
   ```

Prefixes, AppIDs, proxy names, and Steam library paths vary. Back up the prefix and
game directory before modifying them. No Proton/Wine runtime validation has been
completed for this milestone.
