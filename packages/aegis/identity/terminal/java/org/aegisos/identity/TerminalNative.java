package org.aegisos.identity;

/** Fixed verified-image JNI, with no path or library selected by a Binder client. */
public final class TerminalNative {
    static { System.load("/system_ext/lib64/libaegis_terminal_jni.so"); }
    private TerminalNative() { }
    public static native boolean isConsole();
    /** 0: canonical password without echo; 1: raw interactive terminal. */
    public static synchronized native void beginMode(int mode);
    public static synchronized native void endMode();
    /** Bounded, sole-reader stdin operation: null EOF, empty timeout. */
    public static native byte[] readInput(int maximum, int timeoutMillis);
    /** rows, columns; defaults to 24,80 if the host reports zero dimensions. */
    public static native int[] consoleSize();
    /** Called only with the service-owned, validated private PTY master. */
    public static native void resize(int descriptor, int rows, int columns);
}
