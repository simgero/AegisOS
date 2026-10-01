package org.aegisos.identity;

import android.net.LocalSocket;
import android.net.Credentials;
import android.os.Process;
import android.os.ParcelFileDescriptor;
import android.os.SELinux;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructPollfd;
import android.system.StructStat;
import android.system.UnixSocketAddress;

import java.io.FileDescriptor;
import java.io.IOException;
import java.net.SocketAddress;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.ReentrantLock;

/**
 * One private system-server connection to the native resource owner.
 * No CLI endpoint or authority cache. Caller MUST hold RuntimeAdmission plus
 * current AOSP user/serial/CE authorization across start and publication.
 * All package requests share this connection and its disconnect cleanup.
 * Fresh AOSP package approval and session ownership remain service duties.
 */
final class RuntimeBrokerConnection {
    private static final String SOCKET = "/dev/socket/aegis_runtime";
    private static final String PEER = "u:r:aegis_runtime_broker:s0";
    // Pinned Bionic libc/include/sys/socket.h: MSG_NOSIGNAL=0x4000.
    // OsConstants does not expose it. O_NONBLOCK is already set on the fd.
    private static final int SEND_NO_SIGNAL = 0x4000;
    private final ReentrantLock operations = new ReentrantLock(true);
    private FileDescriptor descriptor;
    private LocalSocket socket;
    private long sequence;
    private boolean failed;

    RuntimeBrokerConnection() {
        if (Process.myUid() != Process.SYSTEM_UID || !SELinux.isSELinuxEnforced()
                || !"u:r:system_server:s0".equals(SELinux.getContext())) {
            throw new SecurityException("Runtime control is internal to enforcing system_server");
        }
    }

    RuntimeBrokerProtocol.StartReply start(int user, int serial, long job, long deadlineNanos) {
        if (job < 0) throw new IllegalArgumentException("Invalid start job");
        Exchange result = call(job == 0 ? RuntimeBrokerProtocol.START : RuntimeBrokerProtocol.CONTINUE_START,
                user, serial, deadlineNanos, null, job);
        // EAGAIN here means precisely the returned registered selection. Never
        // retry any other native error, malformed response or broken channel.
        if (result.reply.error != RuntimeBrokerProtocol.START_PENDING) success(result.reply);
        return result.start;
    }

    /** All serials; success means native cleanup proof, NOT AOSP CE-key eviction. */
    void stopAndReleaseAll(int user, long deadlineNanos) {
        success(call(RuntimeBrokerProtocol.STOP_USER, user, 0, deadlineNanos, null, 0).reply);
    }

    int state(int user, int serial, long deadlineNanos) {
        RuntimeBrokerProtocol.Reply reply = call(RuntimeBrokerProtocol.STATUS, user, serial, deadlineNanos, null, 0).reply;
        success(reply);
        return reply.state;
    }

    RuntimeBrokerProtocol.Reply activation(int user, int serial, long deadlineNanos) {
        RuntimeBrokerProtocol.Reply reply = call(RuntimeBrokerProtocol.ACTIVATION, user, serial, deadlineNanos, null, 0).reply;
        success(reply);
        return reply;
    }

    /** Caller must bind this owned PTY and command to the admitted personal CLI session. */
    static final class Terminal implements AutoCloseable {
        final long command;
        final ParcelFileDescriptor master;
        Terminal(long command, ParcelFileDescriptor master) { this.command = command; this.master = master; }
        @Override public void close() throws IOException { master.close(); }
    }

    Terminal execute(int user, int serial, String[] arguments, long deadlineNanos) {
        // Snapshot and validate BEFORE touching the connection. A malformed CLI
        // argument must not tear down other admitted contexts on this channel.
        byte[] tail = RuntimeBrokerProtocol.arguments(arguments == null ? null : arguments.clone());
        Exchange result = call(RuntimeBrokerProtocol.EXEC, user, serial, deadlineNanos, tail, 0);
        success(result.reply);
        return new Terminal(result.reply.command, result.master);
    }

    RuntimeBrokerProtocol.Reply result(int user, int serial, long command, long deadlineNanos) {
        if (command <= 0) throw new IllegalArgumentException("Invalid runtime command identity");
        Exchange result = call(RuntimeBrokerProtocol.RESULT, user, serial, deadlineNanos, null, command);
        success(result.reply);
        return result.reply;
    }

    /** Errors retain the owned job; the service must register it before leaving admission. */
    PackageBrokerProtocol.Reply packageCall(int operation, int user, int serial, long job,
            PackageBrokerProtocol.Intent intent, String digest, long deadlineNanos) {
        byte[] payload = PackageBrokerProtocol.payload(operation, job, intent, digest);
        PackageBrokerProtocol.Reply reply = call(operation, user, serial, deadlineNanos, payload, job).packageReply;
        return reply; // A native error never implicitly abandons a registered job.
    }

    private static final class Exchange {
        final RuntimeBrokerProtocol.Reply reply;
        final ParcelFileDescriptor master;
        final RuntimeBrokerProtocol.StartReply start;
        final PackageBrokerProtocol.Reply packageReply;
        Exchange(RuntimeBrokerProtocol.Reply reply, ParcelFileDescriptor master) {
            this(reply, master, null);
        }
        Exchange(RuntimeBrokerProtocol.Reply reply, ParcelFileDescriptor master, RuntimeBrokerProtocol.StartReply start) {
            this(reply, master, start, null);
        }
        Exchange(RuntimeBrokerProtocol.Reply reply, ParcelFileDescriptor master, RuntimeBrokerProtocol.StartReply start,
                PackageBrokerProtocol.Reply packageReply) {
            this.reply = reply; this.master = master; this.start = start; this.packageReply = packageReply;
        }
    }

    private static void success(RuntimeBrokerProtocol.Reply reply) {
        if (reply.error != 0) {
            // Do not translate incomplete native cleanup into admission or logout success.
            // Keep this healthy channel: a later bounded STOP must be able to retry cleanup.
            throw new IllegalStateException("Native runtime operation was not confirmed: " + reply.error);
        }
    }

    private Exchange call(int operation, int user, int serial, long deadline, byte[] arguments, long command) {
        RuntimeBrokerProtocol.identity(operation, user, serial);
        long wait = remaining(deadline);
        boolean acquired = false;
        try {
            acquired = operations.tryLock(wait, TimeUnit.NANOSECONDS);
            if (!acquired) throw new IllegalStateException("Runtime channel is busy; no completion confirmed");
            remaining(deadline);
            if (failed) throw new IllegalStateException("Runtime connection requires explicit recovery");
            try {
                if (socket == null) connect(deadline);
                return exchange(operation, user, serial, deadline, arguments, command);
            } catch (IOException | ErrnoException | RuntimeException failure) {
                poison();
                // No paths, received bytes or provider exception text cross the public service.
                throw new IllegalStateException("Runtime channel failed; native completion is unconfirmed");
            }
        } catch (InterruptedException interrupted) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Runtime channel acquisition interrupted");
        } finally {
            if (acquired) operations.unlock();
        }
    }

    private void connect(long deadline) throws ErrnoException, IOException {
        // Nonblocking from creation, including connect/backlog handling. Socket
        // timeouts alone would not bound waiting for a full Unix listen backlog.
        descriptor = Os.socket(OsConstants.AF_UNIX,
                OsConstants.SOCK_SEQPACKET | OsConstants.SOCK_CLOEXEC | OsConstants.SOCK_NONBLOCK, 0);
        try {
            Os.connect(descriptor, UnixSocketAddress.createFileSystem(SOCKET));
        } catch (ErrnoException error) {
            if (error.errno != OsConstants.EINPROGRESS) throw error;
            await(OsConstants.POLLOUT, deadline);
            if (Os.getsockoptInt(descriptor, OsConstants.SOL_SOCKET, OsConstants.SO_ERROR) != 0) {
                throw new IOException("Runtime connect failed");
            }
        }
        // This wrapper BORROWS the already-connected fd. poison() owns its close.
        // The framework reads SO_PEERCRED without requiring unstable libcore APIs.
        socket = new LocalSocket(descriptor);
        Credentials peer = socket.getPeerCredentials();
        if (peer == null || peer.getPid() <= 0 || peer.getUid() != Process.ROOT_UID
                || peer.getGid() != Process.ROOT_UID
                || !PEER.equals(SELinux.getPeerContext(descriptor))) {
            throw new SecurityException("Unexpected native runtime peer");
        }
        // LocalSocket's recvmsg wrapper lets us reject and close ancillary FDs.
        Exchange reply = exchange(RuntimeBrokerProtocol.HELLO, 0, 0, deadline, null, 0);
        success(reply.reply); // A new owner connection must first confirm predecessor cleanup.
    }

    private Exchange exchange(int operation, int user, int serial, long deadline, byte[] arguments, long command)
            throws ErrnoException, IOException {
        if (sequence == Long.MAX_VALUE) throw new IOException("Runtime sequence exhausted");
        long current = ++sequence;
        byte[] request;
        if (PackageBrokerProtocol.operation(operation)) {
            request = PackageBrokerProtocol.request(operation, current, deadline, user, serial, System.nanoTime(), arguments);
        } else if (operation == RuntimeBrokerProtocol.EXEC) {
            request = RuntimeBrokerProtocol.execRequest(current, deadline, user, serial, System.nanoTime(), arguments);
        } else if (operation == RuntimeBrokerProtocol.CONTINUE_START) {
            request = RuntimeBrokerProtocol.continueStartRequest(current, deadline, user, serial, System.nanoTime(), command);
        } else if (operation == RuntimeBrokerProtocol.RESULT) {
            request = RuntimeBrokerProtocol.resultRequest(current, deadline, user, serial, System.nanoTime(), command);
        } else {
            request = RuntimeBrokerProtocol.request(operation, current, deadline, user, serial, System.nanoTime());
        }
        for (;;) {
            await(OsConstants.POLLOUT, deadline);
            try {
                int sent = Os.sendto(descriptor, request, 0, request.length,
                        SEND_NO_SIGNAL, (SocketAddress) null);
                if (sent != request.length) throw new IOException("Partial runtime packet");
                break;
            } catch (ErrnoException error) {
                if (error.errno != OsConstants.EAGAIN && error.errno != OsConstants.EINTR) throw error;
            }
        }
        await(OsConstants.POLLIN, deadline);
        boolean terminal = operation == RuntimeBrokerProtocol.EXEC || operation == RuntimeBrokerProtocol.RESULT;
        boolean starting = operation == RuntimeBrokerProtocol.START || operation == RuntimeBrokerProtocol.CONTINUE_START;
        boolean packaging = PackageBrokerProtocol.operation(operation);
        boolean observing = operation == RuntimeBrokerProtocol.ACTIVATION;
        int expected = packaging ? PackageBrokerProtocol.MAX_REPLY : starting ? RuntimeBrokerProtocol.START_REPLY_SIZE
                : terminal ? RuntimeBrokerProtocol.TERMINAL_REPLY_SIZE
                : observing ? RuntimeBrokerProtocol.ACTIVATION_REPLY_SIZE : RuntimeBrokerProtocol.SIZE;
        byte[] bytes = new byte[expected + 1];
        FileDescriptor[] received = null;
        ParcelFileDescriptor master = null;
        try {
            // One SEQPACKET read. SIZE+1 detects oversized replies; never assemble
            // partial messages from separate packets or accept a valid prefix.
            int length = socket.getInputStream().read(bytes);
            received = socket.getAncillaryFileDescriptors();
            int descriptors = received == null ? 0 : received.length;
            remaining(deadline);
            RuntimeBrokerProtocol.Reply reply;
            RuntimeBrokerProtocol.StartReply start = null;
            PackageBrokerProtocol.Reply packageReply = null;
            if (packaging) {
                packageReply = PackageBrokerProtocol.reply(bytes, length, operation, current, user, serial, command, descriptors);
                reply = new RuntimeBrokerProtocol.Reply(packageReply.error,
                        packageReply.error == 0 ? RuntimeBrokerProtocol.READY : RuntimeBrokerProtocol.SEALED);
            } else if (starting) {
                start = RuntimeBrokerProtocol.startReply(bytes, length, operation, current, user, serial, command, descriptors);
                reply = new RuntimeBrokerProtocol.Reply(RuntimeBrokerProtocol.startError(bytes),
                        start.ready ? RuntimeBrokerProtocol.READY : RuntimeBrokerProtocol.SEALED);
            } else if (observing) {
                reply = RuntimeBrokerProtocol.activationReply(bytes, length, current, user, serial, descriptors);
            } else if (terminal) {
                reply = RuntimeBrokerProtocol.terminalReply(bytes, length, operation, current, user, serial,
                        command, descriptors);
            } else {
                if (descriptors != 0) throw new IOException("Unexpected runtime descriptors");
                reply = RuntimeBrokerProtocol.reply(bytes, length, operation, current, user, serial);
            }
            if (descriptors == 1) {
                FileDescriptor fd = received[0];
                if (fd == null) throw new IOException("Missing runtime terminal");
                StructStat stat = Os.fstat(fd);
                // Linux dev_t encoding for the /dev/pts/ptmx master (5,2).
                // Private devpts + mapped slave UID/GID were checked by the
                // authenticated native owner before this handoff.
                if (!OsConstants.S_ISCHR(stat.st_mode) || stat.st_rdev != 0x502L || !Os.isatty(fd)
                        || (Os.fcntlInt(fd, OsConstants.F_GETFL, 0) & OsConstants.O_ACCMODE) != OsConstants.O_RDWR) {
                    throw new IOException("Unexpected runtime terminal descriptor");
                }
                Os.fcntlInt(fd, OsConstants.F_SETFD, OsConstants.FD_CLOEXEC);
                master = ParcelFileDescriptor.dup(fd);
                Os.fcntlInt(master.getFileDescriptor(), OsConstants.F_SETFD, OsConstants.FD_CLOEXEC);
            }
            remaining(deadline);
            Exchange result = new Exchange(reply, master, start, packageReply);
            master = null; // The successful result now owns the duplicate.
            return result;
        } finally {
            if (master != null) try { master.close(); } catch (IOException ignored) { }
            if (received != null) for (FileDescriptor fd : received) close(fd);
            // A read failure can still leave ancillary descriptors in the wrapper.
            try {
                FileDescriptor[] pending = socket.getAncillaryFileDescriptors();
                if (pending != null) for (FileDescriptor fd : pending) close(fd);
            } catch (IOException ignored) { /* poison() retries after a failed read. */ }
        }
    }

    private void await(int event, long deadline) throws ErrnoException, IOException {
        StructPollfd poll = new StructPollfd();
        poll.fd = descriptor;
        poll.events = (short) event;
        for (;;) {
            long nanos = remaining(deadline);
            int millis = (int) ((nanos + 999_999L) / 1_000_000L);
            try {
                int ready = Os.poll(new StructPollfd[] {poll}, millis);
                remaining(deadline);
                if ((poll.revents & (OsConstants.POLLHUP | OsConstants.POLLERR | OsConstants.POLLNVAL)) != 0) {
                    throw new IOException("Runtime peer disconnected");
                }
                if (ready > 0 && (poll.revents & event) != 0) return;
            } catch (ErrnoException error) {
                if (error.errno != OsConstants.EINTR) throw error;
            }
        }
    }

    private static long remaining(long deadline) {
        long now = System.nanoTime();
        if (Thread.currentThread().isInterrupted() || now < 0 || deadline <= now
                || deadline - now > RuntimeBrokerProtocol.MAX_WAIT_NANOS) {
            throw new IllegalStateException("Runtime operation deadline expired or invalid");
        }
        return deadline - now;
    }

    private static void close(FileDescriptor fd) {
        if (fd == null) return;
        try { Os.close(fd); } catch (ErrnoException ignored) { }
    }

    private void poison() {
        failed = true;
        if (socket != null) {
            try {
                FileDescriptor[] received = socket.getAncillaryFileDescriptors();
                if (received != null) for (FileDescriptor fd : received) close(fd);
            } catch (IOException ignored) { }
        }
        socket = null;
        close(descriptor);
        descriptor = null;
        // Native owner must seal/kill/reap on peer loss. Socket close alone is
        // never proof of that cleanup and does not permit AOSP key eviction.
    }
}
