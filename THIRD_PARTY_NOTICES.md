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

## Evaluated future integration

The following are candidates, not dependencies of version 0.1.0:

| Project | Relevant capabilities | License/packaging consideration | Decision |
| --- | --- | --- | --- |
| plugin-sdk | GTA III C++ integration helpers | MIT; external project | Defer until verified hooks require it |
| libVLC | HTTP(S), broad codecs including MP3/AAC, Windows | libVLC is LGPL-2.1+; runtime modules may carry other terms and the distribution is large | Viable prototype candidate; audit exact module bundle |
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

Before adding any backend, confirm the exact version, Windows x86 artifacts, HTTPS
behavior, MP3/AAC support, Wine/Proton behavior, transitive licenses, source-offer
requirements, and whether binary redistribution is permitted.

## Tooling

CMake, Ninja, Visual Studio, MinGW-w64, Clang, Git, GitHub Actions, Wine, and
Proton are development or runtime tools and are not redistributed by this project.
