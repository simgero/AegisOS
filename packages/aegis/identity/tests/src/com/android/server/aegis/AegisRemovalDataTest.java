package com.android.server.aegis;

import static org.junit.Assert.*;

import android.system.Os;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** App-owned, quiescent guest files only. No AOSP user or key deletion. */
@RunWith(AndroidJUnit4.class)
public final class AegisRemovalDataTest {
    private Path fixture;
    private static final byte[] SENTINEL = {7, 19, 29, 31};

    @Before public void setUp() throws Exception {
        fixture = Files.createTempDirectory(InstrumentationRegistry.getInstrumentation()
                .getTargetContext().getCacheDir().toPath(), "aegis-system-data-");
    }

    @After public void tearDown() throws Exception {
        if (fixture != null) AegisRemovalData.deleteSystemDirectory(fixture.toFile(), true);
    }

    private File populated(String name) throws Exception {
        Path root = Files.createDirectory(fixture.resolve(name));
        Files.write(Files.createDirectory(root.resolve("child")).resolve("data"), SENTINEL);
        return root.toFile();
    }

    @Test public void contentsRemovalRetainsEmptyRootForVold() throws Exception {
        File root = populated("system_ce");
        AegisRemovalData.deleteSystemDirectory(root, false);
        assertTrue(root.isDirectory());
        assertEquals(0, root.list().length);
    }

    @Test public void fullDirectoryRemovalIsIdempotentWhenAlreadyAbsent() throws Exception {
        File root = populated("system_user");
        AegisRemovalData.deleteSystemDirectory(root, true);
        assertFalse(root.exists());
        AegisRemovalData.deleteSystemDirectory(root, true);
        AegisRemovalData.deleteSystemDirectory(root, false);
    }

    @Test public void linksInsideRemovedTreeDoNotDeleteOtherFilesOrDirectories() throws Exception {
        File retained = populated("retained");
        File root = populated("removed");
        Os.symlink(retained.getPath(), new File(root, "directory-link").getPath());
        Os.symlink(new File(retained, "child/data").getPath(), new File(root, "file-link").getPath());
        AegisRemovalData.deleteSystemDirectory(root, true);
        assertFalse(root.exists());
        assertArrayEquals(SENTINEL, Files.readAllBytes(retained.toPath().resolve("child/data")));
    }

    @Test public void symbolicRootIsRejectedWithoutTouchingItsTarget() throws Exception {
        File retained = populated("retained");
        Path link = fixture.resolve("not-system-root");
        Os.symlink(retained.getPath(), link.toString());
        try {
            AegisRemovalData.deleteSystemDirectory(link.toFile(), true);
            fail("Symbolic AOSP root accepted");
        } catch (IOException expected) { }
        assertTrue(Files.isSymbolicLink(link));
        assertArrayEquals(SENTINEL, Files.readAllBytes(retained.toPath().resolve("child/data")));
    }

    @Test public void unreadableDirectoryCannotBeReportedAsEmpty() throws Exception {
        File root = populated("unreadable");
        Os.chmod(root.getPath(), 0300);
        try {
            try {
                AegisRemovalData.deleteSystemDirectory(root, false);
                fail("Unreadable directory was accepted as empty");
            } catch (IOException expected) { }
        } finally { Os.chmod(root.getPath(), 0700); }
        assertArrayEquals(SENTINEL, Files.readAllBytes(root.toPath().resolve("child/data")));
    }
}
