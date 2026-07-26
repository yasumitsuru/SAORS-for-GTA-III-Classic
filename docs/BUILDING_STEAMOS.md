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
```

Inspect it with `file`; it should report a PE32 Windows DLL for Intel 80386.

## Native tests

Do not execute a cross-compiled Windows test binary directly on Linux. Configure a
separate native tree with the ASI disabled:

```bash
cmake -S . -B build/host-tests -G Ninja \
  -DSAORS_BUILD_ASI=OFF \
  -DSAORS_BUILD_TESTS=ON
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

CI follows this separation.

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
