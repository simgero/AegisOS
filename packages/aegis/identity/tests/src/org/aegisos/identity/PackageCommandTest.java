package org.aegisos.identity;

import static org.junit.Assert.*;
import org.junit.Test;

/** Checks CLI intent preservation; these fixtures do not grant package authority. */
public final class PackageCommandTest {
    private static String[] parse(String line) { return PackageCommand.fromLinux(line.split(" ")); }
    private static void reject(String line) {
        try { parse(line); fail("Accepted ambiguous or incomplete package request: " + line); }
        catch (IllegalArgumentException expected) { }
    }

    @Test public void installPreservesBothScopesAndAnExactDebianVersion() {
        for (String scope : new String[]{"user", "all"}) {
            assertArrayEquals(new String[]{"package", "install", "--" + scope, "hello"},
                    parse("linux package install hello --scope " + scope));
            assertArrayEquals(new String[]{"package", "install", "--" + scope, "hello", "2:2.10-5+deb13u1~test"},
                    parse("linux package install --scope " + scope + " hello=2:2.10-5+deb13u1~test"));
        }
    }

    @Test public void updateAndRemovePreserveTheirActionWithoutDefaults() {
        for (String scope : new String[]{"user", "all"}) {
            assertArrayEquals(new String[]{"package", "update", "--" + scope},
                    parse("linux package update --scope " + scope));
            assertArrayEquals(new String[]{"package", "remove", "--" + scope, "hello"},
                    parse("linux package remove hello --scope " + scope));
        }
        reject("linux package update hello --scope user");
        reject("linux package remove --scope all");
    }

    @Test public void missingOrUnknownScopeNeverFallsBackToUserOrAll() {
        reject("linux package install hello");
        reject("linux package update");
        reject("linux package remove hello");
        reject("linux package install hello --scope");
        reject("linux package install hello --scope everybody");
        reject("linux package install hello --user");
        reject("linux package install hello --all");
    }

    @Test public void repeatedAndConflictingScopesAreRejected() {
        reject("linux package install hello --scope user --scope user");
        reject("linux package install hello --scope user --scope all");
        reject("linux package install --scope all hello --user");
        reject("linux package install hello --scope=all --scope user");
    }

    @Test public void extraOwnerOptionsOrOperandsCannotChangeTheRequest() {
        reject("linux package install hello ed --scope user");
        reject("linux package install hello --scope user --owner 10");
        reject("linux package install hello --scope all --allow-unauthenticated");
        reject("linux package install --scope user --force");
        reject("linux package purge hello --scope user");
        reject("package install hello --scope user");
    }

    @Test public void malformedOrRemovalVersionsAreNotSilentlyDropped() {
        reject("linux package install hello= --scope user");
        reject("linux package install =2.10-5 --scope user");
        reject("linux package install hello=2.10=5 --scope user");
        reject("linux package remove hello=2.10-5 --scope user");
    }

    @Test public void jobControlsKeepTheExistingSessionJob() {
        for (String action : new String[]{"status", "approve", "cancel"}) {
            assertArrayEquals(new String[]{"package", action}, parse("linux package " + action));
            reject("linux package " + action + " --scope all");
            reject("linux package " + action + " 10");
        }
    }

    @Test public void normalizationDoesNotModifyTheInputTokens() {
        String[] input = {"linux", "package", "install", "hello=2.10-5", "--scope", "user"};
        String[] saved = input.clone();
        String[] output = PackageCommand.fromLinux(input);
        assertArrayEquals(saved, input);
        assertNotSame(input, output);
    }
}
