/* Copyright 2026 The AegisOS Authors. SPDX-License-Identifier: Apache-2.0 */
package com.android.server.aegis;
import static org.junit.Assert.*;
import android.os.ServiceSpecificException;
import android.system.OsConstants;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import org.junit.Test;
import org.junit.runner.RunWith;

/** The production wait policy with controlled replies; not a real key-eviction test. */
@RunWith(AndroidJUnit4.class)
public final class AegisCeLockTest {
    @Test public void failedLockBlocksPublicationUntilConfirmedRetry() throws Exception {
        final int user = 701;
        AtomicInteger published = new AtomicInteger();
        try {
            AegisCeLock.completeUserLock(user, () -> { throw new IOException("lost vold reply"); },
                    published::incrementAndGet);
            fail("Failed eviction succeeded");
        } catch (IOException expected) { }
        assertTrue(AegisCeLock.isPending(user));
        assertEquals(0, published.get());
        assertFalse(AegisCeLock.isPending(user + 1));
        AegisCeLock.completeUserLock(user, () -> {
            assertTrue(AegisCeLock.isPending(user));
        }, () -> {
            assertTrue(AegisCeLock.isPending(user));
            published.incrementAndGet();
        });
        assertFalse(AegisCeLock.isPending(user));
        assertEquals(1, published.get());
    }
    @Test public void failedCachePublicationKeepsUnlockBlocked() throws Exception {
        final int user = 702;
        try {
            AegisCeLock.completeUserLock(user, () -> {}, () -> { throw new IOException("cache"); });
            fail("Failed cache update succeeded");
        } catch (IOException expected) { }
        assertTrue(AegisCeLock.isPending(user));
        AegisCeLock.completeUserLock(user, () -> {}, () -> {});
        assertFalse(AegisCeLock.isPending(user));
    }
    @Test public void successHasNoDelay() throws Exception {
        AtomicInteger calls=new AtomicInteger();
        AegisCeLock.complete(calls::incrementAndGet,()->10L,m->{throw new AssertionError();},500);
        assertEquals(1,calls.get());
    }
    @Test public void busyRetriesUntilExplicitSuccess() throws Exception {
        AtomicInteger calls=new AtomicInteger();AtomicLong clock=new AtomicLong();
        AegisCeLock.complete(()->{if(calls.incrementAndGet()<4)throw new ServiceSpecificException(OsConstants.EBUSY);},
                clock::get,clock::addAndGet,500);
        assertEquals(4,calls.get());assertEquals(300,clock.get());
    }
    @Test public void busyDeadlineNeverBecomesSuccess() throws Exception {
        AtomicInteger calls=new AtomicInteger();AtomicLong clock=new AtomicLong();
        try {
            AegisCeLock.complete(()->{calls.incrementAndGet();throw new ServiceSpecificException(OsConstants.EBUSY);},clock::get,clock::addAndGet,250);
            fail("Busy timeout returned success");
        } catch(IllegalStateException expected) { assertTrue(expected.getMessage().contains("pending")); }
        assertEquals(3,calls.get());assertEquals(250,clock.get());
    }
    @Test public void hardVoldFailureIsNotRetried() throws Exception {
        AtomicInteger calls=new AtomicInteger();
        try {
            AegisCeLock.complete(()->{calls.incrementAndGet();throw new ServiceSpecificException(OsConstants.EIO);},()->0L,m->{throw new AssertionError();},500);
            fail("Hard error ignored");
        } catch(ServiceSpecificException expected) {assertEquals(OsConstants.EIO,expected.errorCode);}
        assertEquals(1,calls.get());
    }
    @Test public void lostBinderOrOtherExceptionIsNotReinterpretedAsBusy() throws Exception {
        IOException original=new IOException("uncertain reply");
        try {
            AegisCeLock.complete(()->{throw original;},()->0L,m->{throw new AssertionError();},500);
            fail("Uncertain reply accepted");
        } catch(IOException expected) {assertSame(original,expected);}
    }
    @Test public void interruptedWaitPreservesCancellation() throws Exception {
        try {
            AegisCeLock.complete(()->{throw new ServiceSpecificException(OsConstants.EBUSY);},()->0L,m->{throw new InterruptedException();},500);
            fail("Interrupted wait accepted");
        } catch(InterruptedException expected) {assertTrue(Thread.currentThread().isInterrupted());}
        finally {Thread.interrupted();}
    }
}
