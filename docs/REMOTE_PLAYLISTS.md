# Remote playlists

Phase 2C resolves remote M3U, simple M3U8, and PLS resources before an
`AudioBackend` sees a media location. The configured station URL remains in the
INI; the selected media URL exists only in memory for the current attempt.

```text
configured HTTP(S) URL
        |
        v
PlaylistResolver ----> HttpClient ----> WinHTTP
        |
        +---- size, status, MIME, UTF-8, HLS, and policy checks
        |
        v
PlaylistParser ----> relative-reference resolution
        |
        v
StreamManager ----> final HTTP(S) media URL ----> AudioBackend
```

libVLC is not given the playlist URL directly because controlled and real tests
showed that this did not reliably open the target station. Keeping retrieval in a
separate service also makes limits, redirect policy, relative URLs, nesting, and
reconnection deterministic and unit-testable.

## Detection and parsing

A resource is treated as a playlist when its final path ends in `.m3u`, `.m3u8`,
or `.pls`, or when the response uses one of these media types:

- `audio/x-mpegurl`, `audio/mpegurl`, `application/x-mpegurl`;
- `application/vnd.apple.mpegurl`;
- `audio/x-scpls`, `application/pls+xml`.

`text/plain` is accepted only when its body passes playlist syntax checks.
Extensionless resources are first requested only through their response headers;
the bounded body is downloaded only after a playlist signal is found.

M3U and PLS entries may be absolute or relative. Relative references are resolved
against the final playlist URL after redirects, using URI path normalization
rather than string concatenation. Safe nested chains such as `M3U -> PLS ->
media` are supported.

Simple M3U8 radio lists work. Tags such as `#EXT-X-TARGETDURATION`,
`#EXT-X-MEDIA-SEQUENCE`, `#EXT-X-STREAM-INF`, or `#EXT-X-ENDLIST` identify HLS
and produce:

```text
HLS playlist detected but HLS resolution is not implemented
```

Segments, variants, keys, and live-window refresh are never selected in this
phase.

## Security limits

Defaults are conservative and configurable:

| Limit | Default |
| --- | ---: |
| Connect timeout | 5,000 ms |
| Receive timeout | 10,000 ms |
| Playlist body | 262,144 bytes |
| Entries per playlist | 128 |
| Redirects per request | 5 |
| Playlist depth | 3 |

WinHTTP retains normal Windows TLS certificate and hostname validation. The
resolver rejects invalid UTF-8, unsafe control characters, malformed escapes,
empty URLs, credentials in authorities, local paths, UNC paths, and schemes such
as `file:`, `ftp:`, and `data:`. A UTF-8 BOM and either CRLF or LF are accepted.
Normalized playlist URLs are tracked per attempt to detect cycles. Bodies and
selected media URLs are never cached on disk.

## HTTPS and HTTP policy

Transport redirects and playlist entries are deliberately different:

- HTTP to HTTP, HTTP to HTTPS, and HTTPS to HTTPS redirects are allowed within
  the redirect limit;
- an HTTPS response redirecting to HTTP is always blocked;
- an HTTP media entry inside an HTTPS playlist is blocked unless that station
  has `AllowHttp=true`.

Allowing the entry does not make its audio encrypted. It means that playlist
integrity is protected by HTTPS while the selected media can still be observed or
modified in transit.

## Configuration

The public Central DJ configuration used for Phase 2C is:

```ini
[General]
ResolveRemotePlaylists=true
PlaylistConnectTimeoutMilliseconds=5000
PlaylistReceiveTimeoutMilliseconds=10000
PlaylistMaximumBytes=262144
PlaylistMaximumEntries=128
PlaylistMaximumRedirects=5
PlaylistMaximumDepth=3

[Station.HeadRadio]
Enabled=true
Name=Rádio Central DJ
URL=https://www.centraldj.com.br/radios/centraldj/stream.m3u
AllowHttp=true
```

`AllowHttp` is per station so another HTTPS station does not silently inherit a
weaker media policy.

## Selection, reconnect, and logging

Entries retain source order. Invalid, unsupported, or policy-blocked entries are
discarded and the first usable entry is selected. `StreamManager` serializes
start and reconnect operations. Reconnect stops the current media, downloads the
configured playlist again, repeats selection, and opens the current media URL.
It does not reuse an old resolved URL indefinitely.

Diagnostics may record entry count, selected index, scheme, playlist type, and
content type. They must not record the selected hostname, path, query, credentials,
playlist body, or complete media URL. The public configured URL may be documented;
the resolved internal URL must not be published.

The ASI constructs the resolver on its initialization worker but performs no
network request, playback, or hook installation. GTA III hooks remain
unimplemented. Wine and Proton behavior remains pending.
