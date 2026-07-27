# Third-party notices

No third-party source or binary is committed to this initial repository.

## Build-time test dependency

### Catch2

- Purpose: host-runnable unit testing.
- Acquisition: CMake first searches for an installed Catch2 3 package, then
  downloads the pinned `v3.8.1` source during a test-enabled configure.
- License: Boost Software License 1.0.
- Source: <https://github.com/catchorg/Catch2>
- Redistribution: Catch2 is linked only into test executables, not the ASI target.

## External runtime tool

### Ultimate ASI Loader

- Purpose: load `.asi` libraries into a Windows game process.
- Relationship: separately installed runtime prerequisite; not linked or bundled.
- License: MIT at the time of this review.
- Source: <https://github.com/ThirteenAG/Ultimate-ASI-Loader>

Users must download a release suitable for their game and review its current
license. Its binaries are intentionally excluded from this repository.

## Optional audio backend

### libVLC / VLC modules

- Purpose: optional HTTP(S) media playback, decoding, and audio output.
- Integration: libVLC 3 C API, loaded dynamically at runtime.
- Tested SDK/runtime: VLC 3.0.23 Win32 official 7z archive.
- Official source:
  <https://downloads.videolan.org/pub/videolan/vlc/3.0.23/win32/>
- Archive SHA-256:
  `f148ff49cdac6c0b6b7018ad7c4e6cd24c99bc6c2dea8258d82684261a639017`.
- libVLC license: GNU Lesser General Public License 2.1 or later.
- Redistribution: no VLC header, import library, DLL, plugin, or archive is
  committed, uploaded, or distributed by this project.

VLC is modular. Individual modules and their third-party libraries can have
different license and source-offer obligations. The MIT license for SAORS for GTA
III Classic does not replace those terms. A module-by-module legal and packaging
review is required before any future release bundles external VLC binaries.

Developers and users must obtain their own compatible Win32 SDK/runtime and retain
the license and notice files supplied by VideoLAN.

## Evaluated future integration

The following are candidates, not dependencies of version 0.1.0:

| Project | Relevant capabilities | License/packaging consideration | Decision |
| --- | --- | --- | --- |
| plugin-sdk | GTA III C++ integration helpers | MIT; external project | Defer until verified hooks require it |
| FFmpeg libraries | HTTP/TLS protocols and MP3/AAC decoding | LGPL-2.1+ only with a compliant configuration; optional GPL/nonfree parts change obligations | Viable low-level candidate; higher integration and compliance cost |
| miniaudio | Small Windows audio-device layer and MP3 decoding | Public domain or MIT; no built-in networking and no stock AAC decoder | Useful output layer, insufficient alone |
| BASS/BASS_AAC | Win32 internet streams and common radio codecs | Proprietary/custom licensing and additional AAC considerations | Do not download, commit, or redistribute without a separate license review |

Primary references reviewed:

- <https://www.videolan.org/vlc/libvlc.html>
- <https://ffmpeg.org/legal.html>
- <https://ffmpeg.org/ffmpeg-protocols.html>
- <https://miniaud.io/docs/manual/>
- <https://www.un4seen.com/bass.html>
- <https://github.com/DK22Pac/plugin-sdk>

Before distributing any backend, confirm the exact modules, HTTPS behavior,
MP3/AAC support, Wine/Proton behavior, transitive licenses, source-offer
requirements, and whether binary redistribution is permitted.

## Tooling

CMake, Ninja, Visual Studio, MinGW-w64, Clang, Git, GitHub Actions, Wine, and
Proton are development or runtime tools and are not redistributed by this project.
