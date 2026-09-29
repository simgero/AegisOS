package org.aegisos.identity;

import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;

/** One-use terminal preparation, not password proof or permission to read CE. */
public final class LoginPreparation {
    public final AospIdentityBackend.UserKey user;
    private final long interactiveEpoch;
    private final long userEpoch;
    private final AtomicBoolean consumed = new AtomicBoolean();

    public LoginPreparation(AospIdentityBackend.UserKey user,
            long interactiveEpoch, long userEpoch) {
        this.user = Objects.requireNonNull(user);
        this.interactiveEpoch = interactiveEpoch;
        this.userEpoch = userEpoch;
    }

    /** A failed claim also consumes the preparation; the CLI must prepare again. */
    public void claim(AospIdentityBackend.UserKey current, long interactive, long revision) {
        if (!consumed.compareAndSet(false, true) || !user.equals(current)
                || interactive != interactiveEpoch || revision != userEpoch) {
            throw new SecurityException("Login target changed; prepare again before credentials");
        }
    }

    public void revoke() { consumed.set(true); }
}
