package org.aegisos.identity;

import static org.junit.Assert.*;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Inert internal frames only; no broker, passwords, users or native processes. */
@RunWith(AndroidJUnit4.class)
public final class RuntimeBrokerProtocolTest {
    private static void denied(Runnable action) {
        try { action.run(); fail("Malformed frame accepted"); }
        catch (IllegalArgumentException expected) { }
    }

    private static byte[] response(int operation, int error, int state) {
        return ByteBuffer.allocate(32).order(ByteOrder.LITTLE_ENDIAN)
                .putInt(RuntimeBrokerProtocol.MAGIC).putShort((short) 1).putShort((short) operation)
                .putLong(2).putInt(10).putInt(operation == RuntimeBrokerProtocol.STOP_USER ? 0 : 1234)
                .putInt(error).putInt(state).array();
    }

    private static RuntimeBrokerProtocol.Reply parse(byte[] data) {
        return RuntimeBrokerProtocol.reply(data, data.length, RuntimeBrokerProtocol.START, 2, 10, 1234);
    }

    @Test public void exactLittleEndianRequestMatchesNativeGoldenFrame() {
        byte[] golden = {0x41,0x47,0x52,0x42,1,0,2,0,2,0,0,0,0,0,0,0,
                0x64,(byte)0xca,(byte)0x9a,0x3b,0,0,0,0,10,0,0,0,(byte)0xd2,4,0,0};
        assertArrayEquals(golden, RuntimeBrokerProtocol.request(RuntimeBrokerProtocol.START,
                2, 1_000_000_100L, 10, 1234, 100));
    }

    @Test public void deadlinesAndSequencesCannotWrapOrBecomeUnbounded() {
        for (long deadline : new long[] {-1, 0, 100, 10_000_000_101L, Long.MAX_VALUE}) {
            denied(() -> RuntimeBrokerProtocol.request(RuntimeBrokerProtocol.START, 1, deadline, 10, 1234, 100));
        }
        for (long sequence : new long[] {0, -1, Long.MIN_VALUE}) {
            denied(() -> RuntimeBrokerProtocol.request(RuntimeBrokerProtocol.START, sequence, 101, 10, 1234, 100));
        }
    }

    @Test public void systemUserUnknownOperationsAndAmbiguousStopAreRejected() {
        denied(() -> RuntimeBrokerProtocol.request(2, 1, 101, 0, 0, 100));
        denied(() -> RuntimeBrokerProtocol.request(2, 1, 101, 21473, 0, 100));
        denied(() -> RuntimeBrokerProtocol.request(2, 1, 101, 10, -1, 100));
        denied(() -> RuntimeBrokerProtocol.request(1, 1, 101, 10, 0, 100));
        denied(() -> RuntimeBrokerProtocol.request(3, 1, 101, 10, 1234, 100));
        denied(() -> RuntimeBrokerProtocol.request(5, 1, 101, 10, 1234, 100));
        assertEquals(32, RuntimeBrokerProtocol.request(3, 1, 101, 10, 0, 100).length);
    }

    @Test public void responseMustEchoEveryRequestBinding() {
        byte[] valid = response(2, 0, 1);
        assertEquals(1, parse(valid).state);
        for (int offset : new int[] {0, 4, 6, 8, 16, 20}) {
            byte[] bad = valid.clone();
            bad[offset] ^= 1;
            denied(() -> parse(bad));
        }
    }

    @Test public void errorCannotClaimReadyOrCompletedTeardown() {
        denied(() -> parse(response(2, 5, 1)));
        denied(() -> parse(response(2, 5, 0)));
        denied(() -> parse(response(2, -1, 2)));
        denied(() -> parse(response(2, 4096, 2)));
        RuntimeBrokerProtocol.Reply failure = parse(response(2, 5, 2));
        assertEquals(5, failure.error);
        assertEquals(RuntimeBrokerProtocol.SEALED, failure.state);
    }

    @Test public void successfulStartAndStopHaveDifferentProofRequirements() {
        denied(() -> parse(response(2, 0, 0)));
        denied(() -> parse(response(2, 0, 2)));
        byte[] stopped = response(3, 0, 0);
        assertEquals(0, RuntimeBrokerProtocol.reply(stopped, 32, 3, 2, 10, 0).state);
        for (int state : new int[] {1, 2}) {
            byte[] bad = response(3, 0, state);
            denied(() -> RuntimeBrokerProtocol.reply(bad, 32, 3, 2, 10, 0));
        }
    }

    @Test public void partialAndOversizedPacketsAreNotAssembledOrTruncatedIntoSuccess() {
        byte[] valid = response(2, 0, 1);
        for (int length : new int[] {-1, 0, 31, 33}) {
            denied(() -> RuntimeBrokerProtocol.reply(valid, length, 2, 2, 10, 1234));
        }
        denied(() -> RuntimeBrokerProtocol.reply(new byte[31], 32, 2, 2, 10, 1234));
    }
}
