package org.aegisos.identity;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.internal.widget.ILockSettings;
import com.android.internal.widget.LockscreenCredential;

import java.lang.reflect.Proxy;
import java.nio.CharBuffer;

import org.junit.Test;
import org.junit.runner.RunWith;

/** Device-side Binder/Parcel tests; never contacts real LockSettings or changes a real user. */
@RunWith(AndroidJUnit4.class)
public final class CredentialTransportTest {
    // Public test fixture only, never an actual enrolled credential.
    private static final char[] FIXTURE = {'t', 'e', 's', 't'};
    private static final byte[] EXPECTED = {'t', 'e', 's', 't'};

    private static final class LocalLockSettings extends Binder implements AutoCloseable {
        final ILockSettings local;
        LockscreenCredential received;
        LockscreenCredential saved;
        int transactionFlags;
        int userId;
        boolean failTransaction;

        LocalLockSettings() {
            local = (ILockSettings) Proxy.newProxyInstance(ILockSettings.class.getClassLoader(),
                    new Class<?>[]{ILockSettings.class}, (proxy, method, args) -> {
                        if ("asBinder".equals(method.getName())) return this;
                        if (!"setLockCredential".equals(method.getName())) {
                            throw new AssertionError("Unexpected test operation");
                        }
                        received = (LockscreenCredential) args[0];
                        saved = (LockscreenCredential) args[1];
                        userId = (int) args[2];
                        return true;
                    });
            attachInterface(local, ILockSettings.Stub.DESCRIPTOR);
        }

        @Override protected boolean onTransact(int code, Parcel data, Parcel reply, int flags)
                throws RemoteException {
            if (failTransaction) throw new RemoteException("Simulated transport failure");
            transactionFlags = flags;
            data.enforceInterface(ILockSettings.Stub.DESCRIPTOR);
            LockscreenCredential incoming = data.readTypedObject(LockscreenCredential.CREATOR);
            LockscreenCredential old = data.readTypedObject(LockscreenCredential.CREATOR);
            int target = data.readInt();
            data.enforceNoDataAvail();
            boolean result = local.setLockCredential(incoming, old, target);
            reply.writeNoException();
            reply.writeBoolean(result);
            return true;
        }

        @Override public void close() {
            if (received != null) received.close();
            if (saved != null) saved.close();
        }
    }

    @Test public void callerWipeDoesNotInvalidateAsynchronousServiceCopy() throws Exception {
        try (LocalLockSettings service = new LocalLockSettings()) {
            // Without the transport wrapper, asInterface really would return the local object.
            assertSame(service.local, ILockSettings.Stub.asInterface(service));
            try (LockscreenCredential input = LockscreenCredential.createPassword(CharBuffer.wrap(FIXTURE));
                    LockscreenCredential old = LockscreenCredential.createNone()) {
                assertTrue(CredentialTransport.connect(service).setLockCredential(input, old, 10));
                assertNotSame(input, service.received);
                assertNotSame(old, service.saved);
                assertEquals(10, service.userId);
            }
            // Simulates AOSP's delayed password-metrics work after the caller has returned.
            assertTrue(service.received.isPassword());
            assertArrayEquals(EXPECTED, service.received.getCredential());
            assertTrue((service.transactionFlags & IBinder.FLAG_CLEAR_BUF) != 0);
        }
    }

    @Test public void receiverWipeDoesNotInvalidateCallerCopy() throws Exception {
        try (LocalLockSettings service = new LocalLockSettings();
                LockscreenCredential input = LockscreenCredential.createPassword(CharBuffer.wrap(FIXTURE));
                LockscreenCredential old = LockscreenCredential.createNone()) {
            CredentialTransport.connect(service).setLockCredential(input, old, 10);
            byte[] receivedBytes = service.received.getCredential();
            service.received.close();
            assertArrayEquals(new byte[EXPECTED.length], receivedBytes);
            assertArrayEquals(EXPECTED, input.getCredential());
        }
    }

    @Test public void failedTransactionDoesNotBecomeAnAuthenticationResult() throws Exception {
        try (LocalLockSettings service = new LocalLockSettings();
                LockscreenCredential input = LockscreenCredential.createPassword(CharBuffer.wrap(FIXTURE));
                LockscreenCredential old = LockscreenCredential.createNone()) {
            service.failTransaction = true;
            try {
                CredentialTransport.connect(service).setLockCredential(input, old, 10);
                fail("Expected transport failure");
            } catch (RemoteException expected) {
                assertArrayEquals(EXPECTED, input.getCredential());
            }
        }
    }
}
