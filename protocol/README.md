# File session protocol (version 1)

One TCP connection travels through the existing SSH reverse forward. The remote command
sends an authentication frame, waits for `ok`, then sends an open frame. The local daemon
receives the initial file on that same connection and replies with its local cache path.
Thereafter the local service sends saved contents and the remote command acknowledges
successful replacement or returns an error. The same codec is used on a private local Unix
socket by Neovim's asynchronous save helper.

A frame has six header bytes: `J`, `N`, version `1`, type, field count, reserved `0`.
Each field has a four-byte unsigned big-endian byte length followed by that many bytes.
Fields are binary-safe, including empty files. Limits: four fields, 16 MiB per field.
Unknown versions/types, invalid reserved bytes, oversized lengths and truncated messages
are rejected. `read_msg()` returns `std::nullopt` on malformed input, EOF or transport error.
The connection must be discarded after such a failure; frames are not resynchronized.

| Type | Value | Fields |
| --- | --- | --- |
| open | 1 | machine ID + UID, canonical workspace, canonical path, initial file contents |
| save | 2 | saved file contents |
| ok | 3 | cache path for open acknowledgement; otherwise no fields |
| error | 4 | human-readable failure |
| close | 5 | none |
| authenticate | 6 | shared secret from `JANISNVIM_TOKEN` |

The remote path is selected once by the remote invocation. Save messages cannot choose
another destination. Conflict baselines live in the remote session; the local cache keeps
its last acknowledged contents separately. SSH supplies encryption. The token authenticates
access to the local service, not an independent host identity proof. Trust the SSH endpoint
and keep machine IDs unique.
