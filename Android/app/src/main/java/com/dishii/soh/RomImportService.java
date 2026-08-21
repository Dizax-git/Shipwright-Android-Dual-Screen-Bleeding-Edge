package com.dishii.soh;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Locale;
import java.util.Set;

final class RomImportService {
    static final long MAX_ROM_BYTES = 64L * 1024 * 1024;
    private static final int BUFFER_SIZE = 64 * 1024;

    enum Error {
        NONE,
        UNSUPPORTED_INPUT,
        NO_SUPPORTED_ROM,
        MULTIPLE_SUPPORTED_ROMS,
        ENCRYPTED_ARCHIVE,
        UNSAFE_ARCHIVE,
        IO_ERROR
    }

    static final class Result {
        private final File candidate;
        private final Error error;

        private Result(File candidate, Error error) {
            this.candidate = candidate;
            this.error = error;
        }

        static Result success(File candidate) {
            return new Result(candidate, Error.NONE);
        }

        static Result failure(Error error) {
            return new Result(null, error);
        }

        boolean isSuccess() {
            return candidate != null && error == Error.NONE;
        }

        File getCandidate() {
            return candidate;
        }

        Error getError() {
            return error;
        }
    }

    private final File workDirectory;

    RomImportService(File workDirectory) {
        this.workDirectory = workDirectory;
    }

    Result importFile(File source, String displayName, Set<String> supportedHashes) {
        if (source == null || displayName == null || supportedHashes == null
                || !displayName.toLowerCase(Locale.ROOT).endsWith(".z64")) {
            return Result.failure(Error.UNSUPPORTED_INPUT);
        }

        File candidate = null;
        try {
            ensureWorkDirectory();
            candidate = File.createTempFile("rom-candidate-", ".z64", workDirectory);
            String sha1;
            try (InputStream input = new FileInputStream(source);
                 OutputStream output = new FileOutputStream(candidate)) {
                sha1 = copyAndHash(input, output);
            }
            if (!supportedHashes.contains(sha1)) {
                deleteQuietly(candidate);
                return Result.failure(Error.NO_SUPPORTED_ROM);
            }
            return Result.success(candidate);
        } catch (UnsafeInputException e) {
            deleteQuietly(candidate);
            return Result.failure(Error.UNSAFE_ARCHIVE);
        } catch (IOException e) {
            deleteQuietly(candidate);
            return Result.failure(Error.IO_ERROR);
        }
    }

    private String copyAndHash(InputStream input, OutputStream output) throws IOException, UnsafeInputException {
        MessageDigest sha1 = sha1();
        byte[] buffer = new byte[BUFFER_SIZE];
        long written = 0;
        for (int read; (read = input.read(buffer)) != -1;) {
            written += read;
            if (written > MAX_ROM_BYTES) {
                throw new UnsafeInputException();
            }
            sha1.update(buffer, 0, read);
            output.write(buffer, 0, read);
        }
        return hex(sha1.digest());
    }

    private void ensureWorkDirectory() throws IOException {
        if ((!workDirectory.isDirectory() && !workDirectory.mkdirs()) || !workDirectory.isDirectory()) {
            throw new IOException("ROM import work directory is unavailable");
        }
    }

    private static MessageDigest sha1() throws IOException {
        try {
            return MessageDigest.getInstance("SHA-1");
        } catch (NoSuchAlgorithmException e) {
            throw new IOException("SHA-1 is unavailable", e);
        }
    }

    private static String hex(byte[] bytes) {
        StringBuilder output = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            output.append(String.format(Locale.ROOT, "%02x", value & 0xff));
        }
        return output.toString();
    }

    private static void deleteQuietly(File file) {
        if (file != null && file.exists()) {
            file.delete();
        }
    }

    private static final class UnsafeInputException extends Exception {
    }
}
