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
                .putInt(RuntimeBrokerProtocol.MAGIC).putShort(RuntimeBrokerProtocol.VERSION).putShort((short) operation)
                .putLong(2).putInt(10).putInt(operation == RuntimeBrokerProtocol.STOP_USER ? 0 : 1234)
                .putInt(error).putInt(state).array();
    }

    private static RuntimeBrokerProtocol.Reply parse(byte[] data) {
        return RuntimeBrokerProtocol.reply(data, data.length, RuntimeBrokerProtocol.START, 2, 10, 1234);
    }

    @Test public void exactLittleEndianRequestMatchesNativeGoldenFrame() {
        byte[] golden = {0x41,0x47,0x52,0x42,2,0,2,0,2,0,0,0,0,0,0,0,
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

    @Test public void terminalArgumentsMatchNativeGoldenAndPreserveAnEmptyArgument() {
        byte[] golden = {0x41,0x47,0x52,0x42,2,0,5,0,2,0,0,0,0,0,0,0,
                0x64,(byte)0xca,(byte)0x9a,0x3b,0,0,0,0,10,0,0,0,(byte)0xd2,4,0,0,
                3,0,0,0,16,0,0,0,'/','b','i','n','/','p','r','i','n','t','f',0,'%','s',0,0};
        byte[] args = RuntimeBrokerProtocol.arguments(new String[] {"/bin/printf", "%s", ""});
        assertArrayEquals(golden, RuntimeBrokerProtocol.execRequest(2, 1_000_000_100L, 10, 1234, 100, args));
    }

    @Test public void invalidUnicodeNulsRelativeProgramsAndHugeArgumentsFailBeforeTransport() {
        denied(() -> RuntimeBrokerProtocol.arguments(null));
        denied(() -> RuntimeBrokerProtocol.arguments(new String[0]));
        denied(() -> RuntimeBrokerProtocol.arguments(new String[33]));
        for (String value : new String[] {null, "", "bin/bash", "/bin/ba\u0000sh", "/\ud800", "/\udc00"}) {
            denied(() -> RuntimeBrokerProtocol.arguments(new String[] {value}));
        }
        char[] huge = new char[8192]; java.util.Arrays.fill(huge, 'a');
        denied(() -> RuntimeBrokerProtocol.arguments(new String[] {"/bin/echo", new String(huge)}));
        // Supplementary Unicode is preserved as UTF-8, not replaced silently.
        byte[] args = RuntimeBrokerProtocol.arguments(new String[] {"/bin/echo", "\ud83d\udd12"});
        assertEquals(8 + 10 + 5, args.length);
    }

    private static byte[] terminalResponse(int operation, int error, long command, int status, int exited) {
        return ByteBuffer.allocate(48).order(ByteOrder.LITTLE_ENDIAN)
                .put(response(operation, error, error == 0 ? 1 : 2))
                .putLong(command).putInt(status).putInt(exited).array();
    }

    private static RuntimeBrokerProtocol.Reply terminal(byte[] data, int operation, long command, int fds) {
        return RuntimeBrokerProtocol.terminalReply(data, data.length, operation, 2, 10, 1234, command, fds);
    }

    @Test public void commandResultRequestUsesPositiveSignedIdentityWithNoArguments() {
        byte[] packet = RuntimeBrokerProtocol.resultRequest(2, 101, 10, 1234, 100, 44);
        assertEquals(40, packet.length);
        ByteBuffer b = ByteBuffer.wrap(packet).order(ByteOrder.LITTLE_ENDIAN);
        assertEquals(RuntimeBrokerProtocol.RESULT, b.getShort(6)); assertEquals(44, b.getLong(32));
        for (long command : new long[] {0, -1, Long.MIN_VALUE}) {
            denied(() -> RuntimeBrokerProtocol.resultRequest(2, 101, 10, 1234, 100, command));
        }
    }

    @Test public void terminalStartRequiresExactlyOneDescriptorAndNoPrematureExit() {
        byte[] valid = terminalResponse(5, 0, 44, 0, 0);
        assertEquals(44, terminal(valid, 5, 0, 1).command);
        for (int count : new int[] {-1, 0, 2, 8}) denied(() -> terminal(valid, 5, 0, count));
        denied(() -> terminal(terminalResponse(5, 0, 0, 0, 0), 5, 0, 1));
        denied(() -> terminal(terminalResponse(5, 0, 44, 0, 1), 5, 0, 1));
        denied(() -> terminal(terminalResponse(5, 0, 44, 256, 0), 5, 0, 1));
        denied(() -> terminal(terminalResponse(5, 5, 0, 0, 0), 5, 0, 1));
        assertEquals(5, terminal(terminalResponse(5, 5, 0, 0, 0), 5, 0, 0).error);
    }

    @Test public void runningCannotLookLikeExitZeroOrAcceptAnotherCommandsResult() {
        byte[] running = terminalResponse(6, 0, 44, 0, 0);
        assertFalse(terminal(running, 6, 44, 0).exited);
        assertTrue(terminal(terminalResponse(6, 0, 44, 0, 1), 6, 44, 0).exited);
        assertEquals(256, terminal(terminalResponse(6, 0, 44, 256, 1), 6, 44, 0).waitStatus);
        assertEquals(9, terminal(terminalResponse(6, 0, 44, 9, 1), 6, 44, 0).waitStatus);
        denied(() -> terminal(running, 6, 45, 0));
        denied(() -> terminal(running, 6, 44, 1));
        denied(() -> terminal(terminalResponse(6, 0, 44, 256, 0), 6, 44, 0));
        denied(() -> terminal(terminalResponse(6, 0, 44, 0x7f, 1), 6, 44, 0));
        denied(() -> terminal(terminalResponse(6, 0, 44, 0, 2), 6, 44, 0));
        denied(() -> terminal(terminalResponse(6, 2, 44, 0, 0), 6, 44, 0));
    }

    @Test public void terminalReplyMustHaveExactSizeVersionAndAllSessionBindings() {
        byte[] valid = terminalResponse(5, 0, 44, 0, 0);
        for (int offset : new int[] {0, 4, 6, 8, 16, 20}) {
            byte[] bad = valid.clone(); bad[offset] ^= 1;
            denied(() -> terminal(bad, 5, 0, 1));
        }
        for (int size : new int[] {0, 32, 47, 49}) {
            denied(() -> RuntimeBrokerProtocol.terminalReply(valid, size, 5, 2, 10, 1234, 0, 1));
        }
    }
}
