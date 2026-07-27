# Stream and playlist formats

## Direct media

Absolute HTTP and HTTPS media URLs are supported. URL validation requires a
non-empty authority and rejects credentials, control characters, malformed
escapes, local paths, UNC paths, and non-HTTP schemes.

The optional libVLC backend currently has real AAC-over-HTTP evidence. MP3 over
HTTP/HTTPS and AAC over HTTPS remain pending separate authorized tests.

## M3U and simple M3U8

The parser reads one entry per line, ignores blank lines and `#` metadata,
preserves order, and accepts safe absolute or relative HTTP references. Both
`#EXTM3U` and headerless lists are supported. CRLF, LF, and a UTF-8 BOM are
handled.

Simple M3U8 radio lists use the same behavior. HLS tags are detected and rejected
explicitly; SAORS does not select variants, segments, keys, or live windows.

## PLS

PLS detection uses a `.pls` path, a known PLS Content-Type, `[playlist]`, or
`FileN=` fields. Safe `FileN` references are returned in numeric order. Title,
length, version, and declared entry count are informational and do not override
the configured entry limit.

## Remote resolution

`PlaylistResolver` downloads bounded playlist text through the testable
`HttpClient` interface. Production Windows builds use WinHTTP. It handles known
playlist Content-Types, syntactically valid `text/plain`, redirects, relative
references based on the final URL, nested playlists, cycles, UTF-8 validation,
and deterministic first-usable selection.

The resolver passes only the selected media URL to the audio backend. Reconnect
downloads the configured playlist again. See
[Remote playlists](REMOTE_PLAYLISTS.md) for limits and HTTP policy.

## Not implemented

- HLS interpretation or segment downloads;
- authentication or credential storage;
- Icecast/Shoutcast metadata display;
- persistent playlist caching;
- adaptive entry selection;
- GTA III radio hooks;
- verified Wine or Proton runtime support.
