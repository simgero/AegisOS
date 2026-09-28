package org.aegisos.identity;

import android.net.LocalSocket;
import android.os.Process;
import android.os.SELinux;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructPollfd;
import android.system.StructUcred;
import android.system.UnixSocketAddress;

import java.io.FileDescriptor;
import java.io.IOException;
import java.net.SocketAddress;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.ReentrantLock;

/**
 * One private system-server connection to the forthcoming native resource owner.
 * No CLI endpoint or authority cache. Caller MUST hold RuntimeAdmission plus
 * current AOSP user/serial/CE authorization across start and publication.
 * Not registered by the current runtime-absent service. Package cancellation,
 * native owner recovery and SELinux/init integration are separate obligations.
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

    void start(int user, int serial, long deadlineNanos) {
        success(call(RuntimeBrokerProtocol.START, user, serial, deadlineNanos));
    }

    /** All serials; success means native cleanup proof, NOT AOSP CE-key eviction. */
    void stopAndReleaseAll(int user, long deadlineNanos) {
        success(call(RuntimeBrokerProtocol.STOP_USER, user, 0, deadlineNanos));
    }

    int state(int user, int serial, long deadlineNanos) {
        RuntimeBrokerProtocol.Reply reply = call(RuntimeBrokerProtocol.STATUS, user, serial, deadlineNanos);
        success(reply);
        return reply.state;
    }

    private static void success(RuntimeBrokerProtocol.Reply reply) {
        if (reply.error != 0) {
            // Do not translate incomplete native cleanup into admission or logout success.
            // Keep this healthy channel: a later bounded STOP must be able to retry cleanup.
            throw new IllegalStateException("Native runtime operation was not confirmed: " + reply.error);
        }
    }

    private RuntimeBrokerProtocol.Reply call(int operation, int user, int serial, long deadline) {
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
                return exchange(operation, user, serial, deadline);
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
        StructUcred peer = Os.getsockoptUcred(descriptor, OsConstants.SOL_SOCKET, OsConstants.SO_PEERCRED);
        if (peer.pid <= 0 || peer.uid != Process.ROOT_UID || peer.gid != Process.ROOT_UID
                || !PEER.equals(SELinux.getPeerContext(descriptor))) {
            throw new SecurityException("Unexpected native runtime peer");
        }
        // This wrapper BORROWS the already-connected fd. poison() owns its close.
        // LocalSocket's recvmsg wrapper lets us reject and close ancillary FDs.
        socket = new LocalSocket(descriptor);
        RuntimeBrokerProtocol.Reply reply = exchange(RuntimeBrokerProtocol.HELLO, 0, 0, deadline);
        success(reply); // A new owner connection must first confirm predecessor cleanup.
    }

    private RuntimeBrokerProtocol.Reply exchange(int operation, int user, int serial, long deadline)
            throws ErrnoException, IOException {
        if (sequence == Long.MAX_VALUE) throw new IOException("Runtime sequence exhausted");
        long current = ++sequence;
        byte[] request = RuntimeBrokerProtocol.request(operation, current, deadline, user, serial,
                System.nanoTime());
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
        byte[] bytes = new byte[RuntimeBrokerProtocol.SIZE + 1];
        int length;
        boolean hadDescriptors = false;
        try {
            // One SEQPACKET read. SIZE+1 detects oversized replies; never assemble
            // partial messages from separate packets or accept a valid prefix.
            length = socket.getInputStream().read(bytes);
        } finally {
            FileDescriptor[] received = socket.getAncillaryFileDescriptors();
            if (received != null) {
                hadDescriptors = received.length != 0;
                for (FileDescriptor fd : received) close(fd);
            }
        }
        remaining(deadline);
        if (hadDescriptors) throw new IOException("Unexpected runtime descriptors");
        return RuntimeBrokerProtocol.reply(bytes, length, operation, current, user, serial);
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
