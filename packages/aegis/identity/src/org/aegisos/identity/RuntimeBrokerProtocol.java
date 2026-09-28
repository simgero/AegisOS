package org.aegisos.identity;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/** Internal system-server/native-owner protocol. Never carries credentials or client paths. */
final class RuntimeBrokerProtocol {
    static final int MAGIC = 0x42524741;
    static final short VERSION = 1;
    static final int SIZE = 32;
    static final int HELLO = 1, START = 2, STOP_USER = 3, STATUS = 4;
    static final int ABSENT = 0, READY = 1, SEALED = 2;
    static final long MAX_WAIT_NANOS = 10_000_000_000L;

    private RuntimeBrokerProtocol() {}

    static void identity(int operation, int user, int serial) {
        if (operation < HELLO || operation > STATUS || serial < 0
                || (operation == HELLO ? user != 0 || serial != 0 : user < 10 || user >= 21473)
                || (operation == STOP_USER && serial != 0)) {
            throw new IllegalArgumentException("Invalid internal runtime request");
        }
    }

    static byte[] request(int operation, long sequence, long deadline, int user, int serial, long now) {
        identity(operation, user, serial);
        if (sequence <= 0 || deadline <= now || now < 0 || deadline - now > MAX_WAIT_NANOS) {
            throw new IllegalArgumentException("Invalid internal runtime sequence/deadline");
        }
        return ByteBuffer.allocate(SIZE).order(ByteOrder.LITTLE_ENDIAN)
                .putInt(MAGIC).putShort(VERSION).putShort((short) operation)
                .putLong(sequence).putLong(deadline).putInt(user).putInt(serial).array();
    }

    static final class Reply {
        final int error, state;
        Reply(int error, int state) { this.error = error; this.state = state; }
    }

    static Reply reply(byte[] bytes, int length, int operation, long sequence, int user, int serial) {
        identity(operation, user, serial);
        if (bytes == null || length != SIZE || bytes.length < SIZE || sequence <= 0) {
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
                || (error == 0 && operation == START && state != READY)
                || (error == 0 && (operation == HELLO || operation == STOP_USER) && state != ABSENT)) {
            throw new IllegalArgumentException("Invalid internal runtime completion state");
        }
        return new Reply(error, state);
    }
}
