/* Copyright 2026 The AegisOS Authors. Licensed under Apache-2.0. */
package com.android.server.aegis;

import android.os.FileUtils;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructStat;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Objects;

/**
 * Checked metadata I/O for AOSP's reserved-user removal path. No identity,
 * credential, key, Binder endpoint or caller authorization is implemented here.
 * Paths must come from AOSP, under its exclusively locked trusted directory.
 * The caller owns streams, reserves the original UserData throughout this work,
 * and must retain that reservation on ANY failure, including directory fsync.
 */
public final class AegisRemovalFiles {
    private AegisRemovalFiles() {}

    /**
     * Commit both newly opened AOSP copies before removing its old backup.
     * A failure can leave complete new bytes; it is never permission to release
     * a user ID. The caller must close streams without assuming failWrite's
     * original stream-state contract still applies.
     */
    public static void commit(File main, File backup, File reserve,
            FileOutputStream output, FileInputStream input,
            FileOutputStream reserveOutput, int mode) throws IOException {
        File parent = layout(main, backup, reserve);
        Objects.requireNonNull(output);
        Objects.requireNonNull(input);
        Objects.requireNonNull(reserveOutput);
        try {
            requireSameRegularFile(main, output.getFD());
            requireSameRegularFile(main, input.getFD());
            requireSameRegularFile(reserve, reserveOutput.getFD());
            optionalRegularFile(backup);
            syncFile(output, mode);
            input.getChannel().position(0);
            reserveOutput.getChannel().truncate(0);
            reserveOutput.getChannel().position(0);
            FileUtils.copy(input, reserveOutput);
            syncFile(reserveOutput, mode);
            requireSameRegularFile(main, output.getFD());
            requireSameRegularFile(reserve, reserveOutput.getFD());
            syncDirectory(parent);
            unlinkIfPresent(backup);
            syncDirectory(parent);
        } catch (ErrnoException e) {
            throw new IOException("AOSP removal metadata commit unconfirmed", e);
        }
    }

    /** All fallback copies must be gone durably before the caller releases an ID. */
    public static void delete(File main, File backup, File reserve) throws IOException {
        File parent = layout(main, backup, reserve);
        try {
            // Validate the entire inventory before the first unlink. A directory,
            // symlink or hardlink is an error, not an invitation to recursively delete.
            optionalRegularFile(main);
            optionalRegularFile(backup);
            optionalRegularFile(reserve);
            unlinkIfPresent(backup);
            unlinkIfPresent(reserve);
            unlinkIfPresent(main);
            syncDirectory(parent);
        } catch (ErrnoException e) {
            throw new IOException("AOSP removal metadata deletion unconfirmed", e);
        }
    }

    private static File layout(File main, File backup, File reserve) throws IOException {
        File parent = Objects.requireNonNull(main).getAbsoluteFile().getParentFile();
        if (parent == null || !parent.equals(Objects.requireNonNull(backup)
                .getAbsoluteFile().getParentFile()) || !parent.equals(Objects.requireNonNull(reserve)
                .getAbsoluteFile().getParentFile()) || main.getAbsoluteFile().equals(backup.getAbsoluteFile())
                || main.getAbsoluteFile().equals(reserve.getAbsoluteFile())
                || backup.getAbsoluteFile().equals(reserve.getAbsoluteFile())) {
            throw new IOException("Expected three distinct sibling AOSP metadata files");
        }
        return parent;
    }

    private static void regular(StructStat stat) throws IOException {
        if (!OsConstants.S_ISREG(stat.st_mode) || stat.st_nlink != 1) {
            throw new IOException("Expected a regular AOSP metadata copy with one link");
        }
    }

    private static void optionalRegularFile(File file) throws IOException, ErrnoException {
        try { regular(Os.lstat(file.getPath())); }
        catch (ErrnoException e) { if (e.errno != OsConstants.ENOENT) throw e; }
    }

    private static void requireSameRegularFile(File file, FileDescriptor fd)
            throws IOException, ErrnoException {
        StructStat named = Os.lstat(file.getPath());
        StructStat opened = Os.fstat(fd);
        regular(named);
        regular(opened);
        if (named.st_dev != opened.st_dev || named.st_ino != opened.st_ino) {
            throw new IOException("AOSP metadata name no longer refers to its opened stream");
        }
    }

    private static void syncFile(FileOutputStream stream, int mode)
            throws IOException, ErrnoException {
        stream.flush();
        Os.fchmod(stream.getFD(), mode);
        // FileUtils.sync() returns a boolean; FileDescriptor.sync() throws.
        stream.getFD().sync();
    }

    private static void unlinkIfPresent(File file) throws ErrnoException {
        try { Os.unlink(file.getPath()); }
        catch (ErrnoException e) { if (e.errno != OsConstants.ENOENT) throw e; }
    }

    private static void syncDirectory(File parent) throws ErrnoException {
        FileDescriptor fd = Os.open(parent.getPath(), OsConstants.O_RDONLY
                | OsConstants.O_CLOEXEC | OsConstants.O_NOFOLLOW, 0);
        try {
            // O_DIRECTORY is not exposed by the pinned Android OsConstants API.
            // Verify the opened descriptor, not a separate pathname lookup.
            if (!OsConstants.S_ISDIR(Os.fstat(fd).st_mode)) {
                throw new ErrnoException("AOSP metadata parent is not a directory",
                        OsConstants.ENOTDIR);
            }
            Os.fsync(fd);
        }
        finally { Os.close(fd); }
    }
}
