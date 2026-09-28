package org.aegisos.identity;

import android.os.Binder;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

/** Kernel process identity, captured before clearing Binder identity. Not a user identity. */
final class CallerProcess {
    private final int uid;
    private final int pid;
    private final long startTicks;

    CallerProcess() {
        uid = Binder.getCallingUid();
        pid = Binder.getCallingPid();
        if (pid <= 0) throw new SecurityException("A synchronous client process is required");
        startTicks = startTicks(pid);
    }

    void requireSameCaller() {
        if (Binder.getCallingUid() != uid || Binder.getCallingPid() != pid) {
            throw new SecurityException("Session belongs to a different process");
        }
        requireAlive();
    }

    /** For in-progress guards after Binder identity has been cleared by the AOSP adapter. */
    void requireAlive() {
        if (startTicks(pid) != startTicks) {
            throw new SecurityException("Original client process is no longer alive");
        }
    }

    private static long startTicks(int pid) {
        try {
            // The path contains only a kernel-supplied positive PID. comm can contain spaces
            // and parentheses, so fields start after its LAST closing parenthesis.
            String stat = Files.readString(Paths.get("/proc", Integer.toString(pid), "stat"),
                    StandardCharsets.US_ASCII);
            int end = stat.lastIndexOf(')');
            if (end < 0) throw new IOException("Invalid proc stat");
            String[] fields = stat.substring(end + 1).trim().split("\\s+");
            if (fields.length < 20) throw new IOException("Incomplete proc stat");
            long value = Long.parseLong(fields[19]); // field 22, with field 3 at index 0
            if (value <= 0) throw new IOException("Invalid process start time");
            return value;
        } catch (IOException | NumberFormatException e) {
            throw new SecurityException("Client process identity is unavailable");
        }
    }
}
