package org.aegisos.identity;

/** Process-bound byte channel; never transfers the personal PTY descriptor. */
@SensitiveData
interface IAegisTerminal {
    // At most 4096 bytes. Empty means try later; null means terminal EOF.
    @nullable byte[] read();
    // A partial nonblocking write is possible; the caller keeps its remainder.
    int write(in byte[] bytes);
    void resize(int rows, int columns);
    // -1 means running; otherwise the shell's conventional exit status.
    int exitStatus();
    // Ends this terminal channel, not the AOSP user or the whole context.
    void close();
}
