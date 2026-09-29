package org.aegisos.identity;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** One unbuffered stdin owner across command, password and raw-shell modes. */
final class TerminalConsole {
    static TerminalConsole current() { return TerminalNative.isConsole() ? new TerminalConsole() : null; }

    String readLine(String prompt) {
        System.out.print(prompt);
        System.out.flush();
        byte[] bytes = line(4096);
        if (bytes == null) return null;
        try { return new String(bytes, StandardCharsets.UTF_8); }
        finally { Arrays.fill(bytes, (byte) 0); }
    }

    char[] readPassword(String format, String prompt) {
        // The format is fixed by the CLI. Never format or log credential bytes.
        if (!"%s".equals(format)) throw new IllegalArgumentException("Invalid password prompt");
        TerminalNative.beginMode(0);
        byte[] bytes = null;
        char[] result = null;
        boolean filled = false;
        try {
            System.out.print(prompt);
            System.out.flush();
            bytes = line(128);
            if (bytes == null) return null;
            result = new char[bytes.length];
            for (int i = 0; i < bytes.length; ++i) result[i] = (char) (bytes[i] & 0xff);
            filled = true;
            return result;
        } finally {
            if (bytes != null) Arrays.fill(bytes, (byte) 0);
            if (!filled && result != null) Arrays.fill(result, '\0');
            try { TerminalNative.endMode(); }
            catch (RuntimeException | Error failure) {
                if (result != null) Arrays.fill(result, '\0');
                throw failure;
            }
            System.out.println();
        }
    }

    private static byte[] line(int maximum) {
        byte[] bytes = new byte[maximum];
        int length = 0;
        boolean overflow = false;
        try {
            for (;;) {
                byte[] next = TerminalNative.readInput(1, 1000);
                if (next == null) {
                    if (overflow) throw new IllegalArgumentException("Input exceeds bounds");
                    return length == 0 ? null : Arrays.copyOf(bytes, length);
                }
                if (next.length == 0) continue;
                int value = next[0] & 0xff;
                Arrays.fill(next, (byte) 0);
                if (value == '\n') {
                    if (overflow) throw new IllegalArgumentException("Input exceeds bounds");
                    return Arrays.copyOf(bytes, length);
                }
                if (length == maximum) overflow = true;
                else bytes[length++] = (byte) value;
            }
        } finally { Arrays.fill(bytes, (byte) 0); }
    }
}
