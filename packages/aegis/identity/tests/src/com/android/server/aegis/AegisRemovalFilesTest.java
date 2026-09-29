package com.android.server.aegis;

import static org.junit.Assert.*;

import android.system.Os;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Real app-owned guest files; no deletion of AOSP users, keys or product metadata. */
@RunWith(AndroidJUnit4.class)
public final class AegisRemovalFilesTest {
    private File dir, main, backup, reserve;
    private static final byte[] OLD = "previous user list".getBytes(StandardCharsets.UTF_8);
    private static final byte[] NEW = "new user list without removed user".getBytes(StandardCharsets.UTF_8);

    @Before public void createFixture() throws Exception {
        dir = Files.createTempDirectory(InstrumentationRegistry.getInstrumentation()
                .getTargetContext().getCacheDir().toPath(), "aegis-removal-").toFile();
        main = new File(dir, "users.xml");
        backup = new File(dir, "users.xml.backup");
        reserve = new File(dir, "users.xml.reservecopy");
        Files.write(backup.toPath(), OLD);
    }

    @After public void removeOnlyThisFixture() throws Exception {
        if (dir == null) return;
        Os.chmod(dir.getPath(), 0700);
        // Only these tests' own bounded directory and known children; no symlink following.
        for (File file : dir.listFiles()) {
            if (Files.isDirectory(file.toPath(), java.nio.file.LinkOption.NOFOLLOW_LINKS)) {
                for (File child : file.listFiles()) Files.delete(child.toPath());
            }
            Files.delete(file.toPath());
        }
        Files.delete(dir.toPath());
    }

    private interface Action { void run() throws Exception; }
    private static void denied(Action action) throws Exception {
        try { action.run(); }
        catch (IOException expected) { return; }
        throw new AssertionError("Unconfirmed metadata operation returned success");
    }

    private void commit() throws Exception {
        try (FileOutputStream output = new FileOutputStream(main);
             FileInputStream input = new FileInputStream(main);
             FileOutputStream reserveOutput = new FileOutputStream(reserve)) {
            output.write(NEW);
            AegisRemovalFiles.commit(main, backup, reserve, output, input, reserveOutput, 0600);
        }
    }

    @Test public void successfulCommitPreservesBothNewCopiesAndRemovesOldFallback() throws Exception {
        commit();
        assertArrayEquals(NEW, Files.readAllBytes(main.toPath()));
        assertArrayEquals(NEW, Files.readAllBytes(reserve.toPath()));
        assertFalse(backup.exists());
        assertEquals(0600, Os.stat(main.getPath()).st_mode & 07777);
        assertEquals(0600, Os.stat(reserve.getPath()).st_mode & 07777);
    }

    @Test public void commitWithoutOldBackupStillRequiresBothNewCopies() throws Exception {
        Files.delete(backup.toPath());
        commit();
        assertArrayEquals(NEW, Files.readAllBytes(main.toPath()));
        assertArrayEquals(NEW, Files.readAllBytes(reserve.toPath()));
    }

    @Test public void closedReserveStreamDoesNotEraseTheOldFallback() throws Exception {
        try (FileOutputStream output = new FileOutputStream(main);
             FileInputStream input = new FileInputStream(main);
             FileOutputStream reserveOutput = new FileOutputStream(reserve)) {
            output.write(NEW);
            reserveOutput.close();
            denied(() -> AegisRemovalFiles.commit(main, backup, reserve,
                    output, input, reserveOutput, 0600));
        }
        assertArrayEquals(OLD, Files.readAllBytes(backup.toPath()));
    }

    @Test public void substitutedFileNameCannotCommitAnOldOpenInode() throws Exception {
        try (FileOutputStream output = new FileOutputStream(main);
             FileInputStream input = new FileInputStream(main);
             FileOutputStream reserveOutput = new FileOutputStream(reserve)) {
            output.write(NEW);
            Files.move(main.toPath(), new File(dir, "opened-inode").toPath());
            Files.write(main.toPath(), OLD);
            denied(() -> AegisRemovalFiles.commit(main, backup, reserve,
                    output, input, reserveOutput, 0600));
        }
        assertArrayEquals(OLD, Files.readAllBytes(backup.toPath()));
        assertArrayEquals(OLD, Files.readAllBytes(main.toPath()));
    }

    @Test public void symlinkFallbackIsRejectedBeforeDeletionOfAnyCopy() throws Exception {
        Files.write(main.toPath(), NEW);
        Os.symlink(main.getPath(), reserve.getPath());
        denied(() -> AegisRemovalFiles.delete(main, backup, reserve));
        assertArrayEquals(NEW, Files.readAllBytes(main.toPath()));
        assertArrayEquals(OLD, Files.readAllBytes(backup.toPath()));
        assertTrue(Files.isSymbolicLink(reserve.toPath()));
    }

    @Test public void hardlinkedFallbackIsRejectedWithoutDeletingCopies() throws Exception {
        Files.write(main.toPath(), NEW);
        Os.link(main.getPath(), reserve.getPath());
        denied(() -> AegisRemovalFiles.delete(main, backup, reserve));
        assertTrue(main.exists());
        assertTrue(reserve.exists());
        assertArrayEquals(OLD, Files.readAllBytes(backup.toPath()));
    }

    @Test public void unexpectedDirectoryIsNotRecursivelyDeleted() throws Exception {
        Files.write(main.toPath(), NEW);
        assertTrue(reserve.mkdir());
        File retained = new File(reserve, "must-remain");
        Files.write(retained.toPath(), OLD);
        denied(() -> AegisRemovalFiles.delete(main, backup, reserve));
        assertArrayEquals(NEW, Files.readAllBytes(main.toPath()));
        assertArrayEquals(OLD, Files.readAllBytes(backup.toPath()));
        assertArrayEquals(OLD, Files.readAllBytes(retained.toPath()));
    }

    @Test public void deletionChecksRealUnlinkFailureRatherThanFileDeleteBoolean() throws Exception {
        Files.write(main.toPath(), NEW);
        Files.write(reserve.toPath(), NEW);
        Os.chmod(dir.getPath(), 0500);
        try {
            denied(() -> AegisRemovalFiles.delete(main, backup, reserve));
            assertArrayEquals(NEW, Files.readAllBytes(main.toPath()));
            assertArrayEquals(OLD, Files.readAllBytes(backup.toPath()));
        } finally { Os.chmod(dir.getPath(), 0700); }
    }

    @Test public void deletionRemovesEveryCopyAndAcceptsAlreadyAbsentCopies() throws Exception {
        Files.write(main.toPath(), NEW);
        Files.write(reserve.toPath(), NEW);
        AegisRemovalFiles.delete(main, backup, reserve);
        assertEquals(0, dir.list().length);
        AegisRemovalFiles.delete(main, backup, reserve);
        assertEquals(0, dir.list().length);
    }

    @Test public void aliasedOrDifferentParentCopiesAreRejectedBeforeWrites() throws Exception {
        denied(() -> AegisRemovalFiles.delete(main, main, reserve));
        File child = new File(dir, "different-parent");
        assertTrue(child.mkdir());
        denied(() -> AegisRemovalFiles.delete(main, backup, new File(child, "reserve")));
        assertArrayEquals(OLD, Files.readAllBytes(backup.toPath()));
    }
}
