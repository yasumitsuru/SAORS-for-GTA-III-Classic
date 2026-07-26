# Stream and playlist formats

## Implemented parser behavior

### Direct URL

Absolute `http://` and `https://` URLs with a non-empty authority are accepted.
This is syntax validation, not a network availability or TLS test.

### M3U

The parser:

- reads one entry per line;
- ignores blank lines and lines beginning with `#`;
- accepts more than one absolute HTTP(S) URL;
- preserves URL order;
- reports non-URL entries as warnings;
- fails if no supported URL remains.

Both files with `#EXTM3U` and simple headerless lists are supported.

### M3U8

M3U8 text is handled with the same line parser when its content contains absolute
HTTP(S) entries. The current code does not implement HLS manifests, variant
selection, media segments, encryption keys, live-window refresh, or relative URL
resolution. A future backend may hand HLS URLs to a backend with verified HLS
support.

### PLS

The parser detects `[playlist]`, a `.pls` source hint, or `FileN=` entries. Valid
`FileN` HTTP(S) URLs are returned in numeric order. Title, length, version, and
entry-count metadata are ignored.

## Not implemented

- fetching playlist files;
- redirects and MIME/content-type handling;
- relative URL resolution against a playlist URL;
- nested playlist recursion;
- authentication or credentials;
- Icecast/Shoutcast metadata;
- decoding MP3 or AAC;
- HTTP buffering and reconnect policy.

## Future network safety requirements

A network implementation must set connection/read timeouts, cap response and
metadata sizes, verify TLS certificates and hostnames, limit redirects and nesting,
reject unexpected schemes, redact URL credentials from logs, and remain
interruptible during plugin shutdown.
