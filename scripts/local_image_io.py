"""Verified sequential image writes, optionally using an independent APFS clone.

The seed is a storage optimization, never trusted output. Every logical byte,
including zero padding, must be supplied again and the completed file is hashed
back. Existing images and persistent profile disks are never opened writable.
"""
import ctypes
import hashlib
import os
from pathlib import Path
import stat
import sys

BLOCK = 64 * 1024
ZERO = bytes(BLOCK)


def regular_source(path):
    path = Path(path).absolute()
    if path.resolve() != path or not stat.S_ISREG(path.lstat().st_mode):
        raise ValueError("Seed must be a regular file without symlink components")
    return path


def clone_seed(seed, target):
    if sys.platform != "darwin":
        raise ValueError("Explicit copy-on-write reuse requires macOS/APFS")
    seed = regular_source(seed)
    source = os.open(seed, os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC)
    try:
        before = os.fstat(source)
        if not stat.S_ISREG(before.st_mode):
            raise ValueError("Seed is no longer a regular file")
        parent = os.open(target.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC)
        try:
            # sys/clonefile.h: fclonefileat(srcfd, dst_dirfd, name, flags).
            # CLONE_NOOWNERCOPY=2; do not request CLONE_ACL or follow links.
            function = ctypes.CDLL("/usr/lib/libSystem.B.dylib", use_errno=True).fclonefileat
            function.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint32]
            function.restype = ctypes.c_int
            if function(source, parent, os.fsencode(target.name), 2):
                error = ctypes.get_errno()
                raise OSError(error, os.strerror(error), str(target))
        finally:
            os.close(parent)
        return before
    finally:
        os.close(source)


class ImageWriter:
    def __init__(self, target, size, seed=None):
        self.path = Path(target).absolute()
        if self.path.resolve() != self.path or self.path.exists():
            raise ValueError("Image target must be new and have no symlink components")
        if not isinstance(size, int) or size < 0:
            raise ValueError("Invalid image length")
        self.size = size
        self.position = self.written = self.reused = 0
        self.digest = hashlib.sha256()
        self.finished = False
        self.cloned = seed is not None
        self.fd = -1
        try:
            if self.cloned:
                original = clone_seed(seed, self.path)
                self.fd = os.open(self.path, os.O_RDWR | os.O_NOFOLLOW | os.O_CLOEXEC)
                actual = os.fstat(self.fd)
                if not stat.S_ISREG(actual.st_mode) or (actual.st_dev, actual.st_ino) == (original.st_dev, original.st_ino):
                    raise ValueError("Clone did not create an independent regular file")
            else:
                self.fd = os.open(self.path, os.O_RDWR | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW | os.O_CLOEXEC, 0o600)
            os.fchmod(self.fd, 0o600)
            os.ftruncate(self.fd, size)
        except BaseException:
            self.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def close(self):
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1

    def write(self, data):
        if self.fd < 0 or self.finished or len(data) > self.size - self.position:
            raise ValueError("Closed, completed or oversized image stream")
        self.digest.update(data)
        for offset in range(0, len(data), BLOCK):
            chunk = data[offset:offset + BLOCK]
            if self.cloned:
                identical = os.pread(self.fd, len(chunk), self.position) == chunk
            else:
                identical = chunk == ZERO[:len(chunk)]  # New truncated bytes are zero.
            if identical:
                self.reused += len(chunk)
            else:
                written = 0
                while written < len(chunk):
                    count = os.pwrite(self.fd, chunk[written:], self.position + written)
                    if count <= 0:
                        raise OSError("Short image write")
                    written += count
                self.written += written
            self.position += len(chunk)

    def zeros(self, length):
        if length < 0 or length > self.size - self.position:
            raise ValueError("Invalid zero padding")
        while length:
            count = min(length, BLOCK)
            self.write(ZERO[:count])
            length -= count

    def finish(self):
        if self.fd < 0 or self.finished or self.position != self.size:
            raise ValueError("Incomplete or already completed image stream")
        os.fsync(self.fd)
        actual = hashlib.sha256()
        for offset in range(0, self.size, 1024 * 1024):
            chunk = os.pread(self.fd, min(1024 * 1024, self.size - offset), offset)
            if not chunk:
                raise ValueError("Truncated completed image")
            actual.update(chunk)
        if os.fstat(self.fd).st_size != self.size or actual.digest() != self.digest.digest():
            raise ValueError("Image readback differs from the complete supplied stream")
        self.finished = True
        return {"size": self.size, "sha256": actual.hexdigest()}
