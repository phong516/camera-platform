# common/

Header-only types shared by the server and its clients.

`Protocol.hpp` is the single source of truth for the control-plane contract: API
paths, default ports, and RTSP mount names. The server and both clients read their
values from here so a port change cannot drift out of sync.

Rules for this directory:

- Header-only. No `.cpp` files, no library to link.
- No GStreamer, no Qt. The server must stay Qt-free; the client must stay
  GStreamer-free. Anything added here must be includable by both.
- Value types and small helpers only. If it needs a socket, a pipeline, or a
  widget, it belongs in `server/` or `client-qt/`.
