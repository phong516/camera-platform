# Main loop ownership: pointer aliases vs owned references

Phong asked what `lab8client` does and why `mloop_local` needs `g_main_loop_unref` when `mloop` already points at the same loop — evidence that "points to" and "owns" were being conflated. Lesson 0010 answers it with the refcount ledger, the GLib source comment showing `g_main_loop_run` holds its own temporary reference, and an ASan experiment (missing unref = 16-byte leak at `g_main_loop_new`; double unref = GLib critical), and future ownership topics (`GMainContext` vs `GMainLoop`, Qt event-loop integration) should reuse that ledger rather than re-derive it.

Evidence: the question came before any exercise; exercise 1 (delete the global by passing the loop through `user_data`) is the check for understanding. If it lands, app-level loop ownership is the next step; if not, re-teach with a two-owner example before Qt.
