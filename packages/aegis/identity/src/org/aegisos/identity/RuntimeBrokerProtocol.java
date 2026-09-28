package org.aegisos.identity;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;

/** Internal system-server/native-owner protocol. No credentials, host paths or input descriptors. */
final class RuntimeBrokerProtocol {
    static final int MAGIC = 0x42524741;
    static final short VERSION = 2;
    static final int SIZE = 32;
    static final int TERMINAL_REPLY_SIZE = 48, MAX_PACKET = 8192, MAX_ARGS = 32;
    static final int HELLO = 1, START = 2, STOP_USER = 3, STATUS = 4, EXEC = 5, RESULT = 6;
    static final int ABSENT = 0, READY = 1, SEALED = 2;
    static final long MAX_WAIT_NANOS = 10_000_000_000L;

    private RuntimeBrokerProtocol() {}

    static void identity(int operation, int user, int serial) {
        if (operation < HELLO || operation > RESULT || serial < 0
                || (operation == HELLO ? user != 0 || serial != 0 : user < 10 || user >= 21473)
                || (operation == STOP_USER && serial != 0)) {
            throw new IllegalArgumentException("Invalid internal runtime request");
        }
    }

    static byte[] request(int operation, long sequence, long deadline, int user, int serial, long now) {
        if (operation > STATUS) throw new IllegalArgumentException("Terminal request needs a payload");
        return header(operation, sequence, deadline, user, serial, now, SIZE).array();
    }

    private static ByteBuffer header(int operation, long sequence, long deadline, int user, int serial,
            long now, int size) {
        identity(operation, user, serial);
        if (sequence <= 0 || deadline <= now || now < 0 || deadline - now > MAX_WAIT_NANOS) {
            throw new IllegalArgumentException("Invalid internal runtime sequence/deadline");
        }
        return ByteBuffer.allocate(size).order(ByteOrder.LITTLE_ENDIAN)
                .putInt(MAGIC).putShort(VERSION).putShort((short) operation)
                .putLong(sequence).putLong(deadline).putInt(user).putInt(serial);
    }

    static byte[] arguments(String[] arguments) {
        if (arguments == null || arguments.length == 0 || arguments.length > MAX_ARGS) {
            throw new IllegalArgumentException("Invalid runtime argument count");
        }
        ByteBuffer payload = ByteBuffer.allocate(MAX_PACKET - SIZE).order(ByteOrder.LITTLE_ENDIAN);
        payload.putInt(arguments.length).putInt(0);
        for (int index = 0; index < arguments.length; index++) {
            String value = arguments[index];
            if (value == null || value.length() >= MAX_PACKET || (index == 0 && !value.startsWith("/"))) {
                throw new IllegalArgumentException("Invalid runtime program argument");
            }
            for (int pos = 0; pos < value.length(); pos++) {
                char c = value.charAt(pos);
                if (c == 0 || Character.isLowSurrogate(c)) {
                    throw new IllegalArgumentException("Invalid runtime argument encoding");
                }
                if (Character.isHighSurrogate(c)) {
                    if (++pos == value.length() || !Character.isLowSurrogate(value.charAt(pos))) {
                        throw new IllegalArgumentException("Invalid runtime argument encoding");
                    }
                }
            }
            byte[] bytes = value.getBytes(StandardCharsets.UTF_8);
            if (bytes.length >= payload.remaining()) {
                throw new IllegalArgumentException("Runtime arguments exceed packet limit");
            }
            payload.put(bytes).put((byte) 0);
        }
        payload.putInt(4, payload.position() - 8);
        byte[] bytes = new byte[payload.position()];
        payload.flip(); payload.get(bytes);
        return bytes;
    }

    /** Tail is produced by arguments(); connection keeps it private and immutable. */
    static byte[] execRequest(long sequence, long deadline, int user, int serial, long now, byte[] tail) {
        if (tail == null || tail.length < 10 || tail.length > MAX_PACKET - SIZE) {
            throw new IllegalArgumentException("Invalid runtime argument packet");
        }
        return header(EXEC, sequence, deadline, user, serial, now, SIZE + tail.length).put(tail).array();
    }

    static byte[] resultRequest(long sequence, long deadline, int user, int serial, long now, long command) {
        if (command <= 0) throw new IllegalArgumentException("Invalid runtime command identity");
        return header(RESULT, sequence, deadline, user, serial, now, 40).putLong(command).array();
    }

    static final class Reply {
        final int error, state, waitStatus;
        final long command;
        final boolean exited;
        Reply(int error, int state) { this(error, state, 0, 0, false); }
        Reply(int error, int state, long command, int waitStatus, boolean exited) {
            this.error = error; this.state = state; this.command = command;
            this.waitStatus = waitStatus; this.exited = exited;
        }
    }

    static Reply reply(byte[] bytes, int length, int operation, long sequence, int user, int serial) {
        if (operation > STATUS) throw new IllegalArgumentException("Terminal reply needs completion fields");
        return parse(bytes, length, operation, sequence, user, serial, SIZE);
    }

    private static Reply parse(byte[] bytes, int length, int operation, long sequence, int user, int serial,
            int size) {
        identity(operation, user, serial);
        if (bytes == null || length != size || bytes.length < size || sequence <= 0) {
            throw new IllegalArgumentException("Invalid internal runtime reply size");
        }
        ByteBuffer reply = ByteBuffer.wrap(bytes, 0, SIZE).order(ByteOrder.LITTLE_ENDIAN);
        if (reply.getInt() != MAGIC || reply.getShort() != VERSION || reply.getShort() != operation
                || reply.getLong() != sequence || reply.getInt() != user || reply.getInt() != serial) {
            throw new IllegalArgumentException("Internal runtime reply does not match this request");
        }
        int error = reply.getInt(), state = reply.getInt();
        if (error < 0 || error > 4095 || state < ABSENT || state > SEALED
                || (error != 0 && state != SEALED)
                || (error == 0 && (operation == START || operation == EXEC || operation == RESULT) && state != READY)
                || (error == 0 && (operation == HELLO || operation == STOP_USER) && state != ABSENT)) {
            throw new IllegalArgumentException("Invalid internal runtime completion state");
        }
        return new Reply(error, state);
    }

    static Reply terminalReply(byte[] bytes, int length, int operation, long sequence, int user, int serial,
            long expectedCommand, int descriptorCount) {
        if ((operation != EXEC && operation != RESULT)
                || (operation == EXEC ? expectedCommand != 0 : expectedCommand <= 0)) {
            throw new IllegalArgumentException("Invalid terminal operation");
        }
        Reply header = parse(bytes, length, operation, sequence, user, serial, TERMINAL_REPLY_SIZE);
        ByteBuffer data = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        long command = data.getLong(32);
        int status = data.getInt(40), exited = data.getInt(44);
        int signal = status & 0x7f;
        boolean finishedStatus = status >= 0 && status <= 65535 && signal != 0x7f;
        if (header.error != 0 ? (command != 0 || status != 0 || exited != 0 || descriptorCount != 0)
                : (command <= 0 || (exited != 0 && exited != 1)
                    || (operation == EXEC && (exited != 0 || status != 0 || descriptorCount != 1))
                    || (operation == RESULT && (command != expectedCommand || descriptorCount != 0))
                    || (exited == 0 && status != 0) || (exited == 1 && !finishedStatus))) {
            throw new IllegalArgumentException("Invalid runtime terminal completion");
        }
        return new Reply(header.error, header.state, command, status, exited == 1);
    }
}
