package org.aegisos.identity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

import org.junit.Test;
import org.junit.runner.RunWith;

/** Device-side preparation tests; no fixture is an AOSP password proof. */
@RunWith(AndroidJUnit4.class)
public final class LoginPreparationTest {
    private static final AospIdentityBackend.UserKey USER =
            new AospIdentityBackend.UserKey(10, 42);

    private static void denied(Runnable operation) {
        try {
            operation.run();
            fail("Revoked or consumed preparation was accepted");
        } catch (SecurityException expected) { }
    }

    @Test public void preparationCanBeClaimedOnlyOnce() {
        LoginPreparation prepared = new LoginPreparation(USER, 7, 9);
        prepared.claim(new AospIdentityBackend.UserKey(10, 42), 7, 9);
        denied(() -> prepared.claim(USER, 7, 9));
    }

    @Test public void otherUserOrReusedIdConsumesThePreparation() {
        for (AospIdentityBackend.UserKey wrong : new AospIdentityBackend.UserKey[] {
                new AospIdentityBackend.UserKey(11, 42),
                new AospIdentityBackend.UserKey(10, 43)}) {
            LoginPreparation prepared = new LoginPreparation(USER, 7, 9);
            denied(() -> prepared.claim(wrong, 7, 9));
            denied(() -> prepared.claim(USER, 7, 9));
        }
    }

    @Test public void keyguardRevocationCannotBeReplayedWithAnOlderEpoch() {
        LoginPreparation prepared = new LoginPreparation(USER, 7, 9);
        denied(() -> prepared.claim(USER, 8, 9));
        denied(() -> prepared.claim(USER, 7, 9));
    }

    @Test public void userStopOrLifecycleChangeInvalidatesPreparation() {
        LoginPreparation prepared = new LoginPreparation(USER, 7, 9);
        denied(() -> prepared.claim(USER, 7, 10));
        denied(() -> prepared.claim(USER, 7, 9));
    }

    @Test public void cancelledPreparationCannotBeClaimed() {
        LoginPreparation prepared = new LoginPreparation(USER, 7, 9);
        prepared.revoke();
        prepared.revoke();
        denied(() -> prepared.claim(USER, 7, 9));
    }

    @Test public void onlyOneConcurrentCredentialAttemptCanClaimPreparation() throws Exception {
        LoginPreparation prepared = new LoginPreparation(USER, 7, 9);
        CountDownLatch start = new CountDownLatch(1);
        ExecutorService workers = Executors.newFixedThreadPool(2);
        try {
            java.util.concurrent.Callable<Integer> attempt = () -> {
                if (!start.await(3, TimeUnit.SECONDS)) throw new AssertionError("No start signal");
                try {
                    prepared.claim(USER, 7, 9);
                    return 1;
                } catch (SecurityException expected) {
                    return 0;
                }
            };
            Future<Integer> first = workers.submit(attempt);
            Future<Integer> second = workers.submit(attempt);
            start.countDown();
            assertEquals(1, first.get(3, TimeUnit.SECONDS) + second.get(3, TimeUnit.SECONDS));
        } finally {
            workers.shutdownNow();
            if (!workers.awaitTermination(3, TimeUnit.SECONDS)) {
                throw new AssertionError("Credential fixture workers did not exit");
            }
        }
    }
}
