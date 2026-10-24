# Client/server boundary: lab8client is a probe, appsrc lives in the server

Phong asked whether `lab8client` needs a server and where the `appsrc` comes from — the client-only view in Lesson 10 hid the pairing. Clarified with the two-sided diagram: the client is a consumer (`rtspsrc → appsink`, no `appsrc`), while `lab8-rtsp-recover.cpp` owns the producer pipeline (`… → appsink(psink)`) and the RTSP media's `appsrc(vsrc)`, which is created on the first viewer and wired on `media-configure`. Future architecture lessons should draw both sides of a bridge by default; the single-sided view invites exactly this question.

Evidence: the question itself; a server-only run shows no `media-configure` and logs "no viewers connected, nothing to forward".
