/* Copyright 2026 The AegisOS Authors. Licensed under Apache-2.0. */
package com.android.server.aegis;

import android.os.storage.StorageManager;
import android.os.storage.VolumeInfo;
import android.os.storage.VolumeRecord;
import android.system.ErrnoException;
import android.system.Os;

import java.io.File;
import java.io.IOException;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.NoSuchFileException;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.Objects;
import java.util.ArrayList;
import java.util.List;

/** Internal AOSP removal primitives. No identity, key or authorization authority. */
public final class AegisRemovalData {
    private AegisRemovalData() {}

    /** Checked inventory of a quiescent AOSP credential-state directory. No file contents. */
    public static List<String> listSystemFiles(File directory) throws IOException {
        Path root = directory.toPath();
        final BasicFileAttributes initial;
        try {
            initial = Files.readAttributes(root, BasicFileAttributes.class, LinkOption.NOFOLLOW_LINKS);
        } catch (NoSuchFileException absent) {
            return new ArrayList<>();
        }
        if (!initial.isDirectory()) throw new IOException("Expected AOSP credential directory");
        List<String> names = new ArrayList<>();
        try (java.nio.file.DirectoryStream<Path> children = Files.newDirectoryStream(root)) {
            for (Path child : children) {
                BasicFileAttributes info = Files.readAttributes(child, BasicFileAttributes.class,
                        LinkOption.NOFOLLOW_LINKS);
                if (!info.isRegularFile()) throw new IOException("Unexpected credential-state entry");
                names.add(child.getFileName().toString());
            }
        }
        BasicFileAttributes after = Files.readAttributes(root, BasicFileAttributes.class,
                LinkOption.NOFOLLOW_LINKS);
        if (!after.isDirectory() || !Objects.equals(initial.fileKey(), after.fileKey())) {
            throw new IOException("AOSP credential directory changed");
        }
        return names;
    }

    /** Phase 1 cannot acknowledge deletion while an adopted disk may retain data. */
    public static StorageManager requireInternalStorageOnly(StorageManager storage) {
        Objects.requireNonNull(storage, "AOSP storage unavailable");
        for (VolumeInfo volume : Objects.requireNonNull(storage.getVolumes())) {
            Objects.requireNonNull(volume, "Incomplete AOSP volume inventory");
            if (volume.getType() == VolumeInfo.TYPE_PRIVATE
                    && !VolumeInfo.ID_PRIVATE_INTERNAL.equals(volume.getId())) {
                throw new IllegalStateException("Adopted private storage removal is unsupported");
            }
        }
        for (VolumeRecord record : Objects.requireNonNull(storage.getVolumeRecords())) {
            Objects.requireNonNull(record, "Incomplete AOSP volume records");
            if (record.getType() == VolumeInfo.TYPE_PRIVATE) {
                throw new IllegalStateException("Disconnected private storage removal is unsupported");
            }
        }
        return storage;
    }

    /**
     * Delete a trusted, quiescent AOSP system directory, without following links.
     * Only UserDataPreparer supplies these fixed paths after Android and GNU
     * processes are stopped. The original UserData/serial remains reserved.
     * Not a general-purpose deletion API for attacker-controlled live trees.
     * Listing failures, changed roots, mount crossings and failed unlink all
     * propagate. vold still owns deletion of system_de/system_ce roots.
     */
    public static void deleteSystemDirectory(File directory, boolean deleteRoot) throws IOException {
        final Path root = directory.toPath();
        final BasicFileAttributes initial;
        try {
            initial = Files.readAttributes(root, BasicFileAttributes.class, LinkOption.NOFOLLOW_LINKS);
        } catch (NoSuchFileException absent) {
            return;
        }
        if (!initial.isDirectory()) throw new IOException("Expected an AOSP system directory");
        final long device = device(root);
        Files.walkFileTree(root, new SimpleFileVisitor<Path>() {
            @Override public FileVisitResult preVisitDirectory(Path path, BasicFileAttributes attrs)
                    throws IOException {
                if (device(path) != device) throw new IOException("Unexpected mount in user data");
                if (path.equals(root) && !Objects.equals(initial.fileKey(), attrs.fileKey())) {
                    throw new IOException("AOSP system directory changed");
                }
                return FileVisitResult.CONTINUE;
            }
            @Override public FileVisitResult visitFile(Path path, BasicFileAttributes attrs)
                    throws IOException {
                if (path.equals(root)) throw new IOException("AOSP system directory changed");
                if (device(path) != device) throw new IOException("Unexpected mount in user data");
                // walkFileTree does not follow symbolic links; delete only the link itself.
                Files.delete(path);
                return FileVisitResult.CONTINUE;
            }
            @Override public FileVisitResult postVisitDirectory(Path path, IOException failure)
                    throws IOException {
                if (failure != null) throw failure;
                if (deleteRoot || !path.equals(root)) Files.delete(path);
                return FileVisitResult.CONTINUE;
            }
        });
        if (!deleteRoot) {
            BasicFileAttributes after = Files.readAttributes(root, BasicFileAttributes.class,
                    LinkOption.NOFOLLOW_LINKS);
            if (!after.isDirectory() || !Objects.equals(initial.fileKey(), after.fileKey())) {
                throw new IOException("AOSP system directory changed");
            }
            try (java.nio.file.DirectoryStream<Path> children = Files.newDirectoryStream(root)) {
                if (children.iterator().hasNext()) throw new IOException("AOSP data remains");
            }
        }
    }

    private static long device(Path path) throws IOException {
        try { return Os.lstat(path.toString()).st_dev; }
        catch (ErrnoException failure) { throw new IOException("AOSP data lookup failed", failure); }
    }
}
