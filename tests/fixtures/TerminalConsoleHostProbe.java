package org.aegisos.identity;

import java.util.Arrays;

/** Public fixtures only. No Binder, AOSP account or actual credential is used. */
public final class TerminalConsoleHostProbe {
    public static void main(String[] args) {
        TerminalConsole console = TerminalConsole.current();
        if ("pipe".equals(args[0])) {
            if (console != null) throw new AssertionError("Pipe accepted as console");
            System.out.println("PIPE_REFUSED");
            return;
        }
        if (console == null) throw new AssertionError("Missing test PTY");
        char[] value = null;
        try {
            value = console.readPassword("%s", "PUBLIC_FIXTURE_PASSWORD> ");
            switch (args[0]) {
                case "normal":
                    if (!Arrays.equals(value, "OnlyPublicFixture42".toCharArray())) {
                        throw new AssertionError("Fixture mismatch");
                    }
                    break;
                case "maximum":
                    if (value == null || value.length != 128) {
                        throw new AssertionError("Maximum length mismatch");
                    }
                    for (char c : value) if (c != 'x') throw new AssertionError("Fixture mismatch");
                    break;
                case "eof":
                    if (value != null) throw new AssertionError("EOF was not preserved");
                    break;
                case "overflow":
                    throw new AssertionError("Oversized line was accepted");
                default:
                    throw new AssertionError("Unexpected scenario completion");
            }
            System.out.println("PASSWORD_RETURNED");
        } catch (IllegalArgumentException expected) {
            if (!"overflow".equals(args[0])
                    || !"Input exceeds bounds".equals(expected.getMessage())) throw expected;
            System.out.println("OVERFLOW_REFUSED");
        } finally {
            if (value != null) Arrays.fill(value, '\0');
        }
        // A following command proves that overlong input was fully drained and
        // that stdin ownership returned to the ordinary command reader.
        if (!"AFTER".equals(console.readLine("COMMAND> "))) {
            throw new AssertionError("Next command was lost or contaminated");
        }
        System.out.println("DONE");
    }
}
