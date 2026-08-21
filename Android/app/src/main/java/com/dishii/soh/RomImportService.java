package com.dishii.soh;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Enumeration;
import java.util.Locale;
import java.util.Set;
import org.apache.commons.compress.PasswordRequiredException;
import org.apache.commons.compress.archivers.sevenz.SevenZArchiveEntry;
import org.apache.commons.compress.archivers.sevenz.SevenZFile;
import org.apache.commons.compress.archivers.zip.ZipArchiveEntry;
import org.apache.commons.compress.archivers.zip.ZipFile;
import org.apache.commons.compress.utils.InputStreamStatistics;

final class RomImportService {
    static final long MAX_ROM_BYTES = 64L * 1024 * 1024;
    static final long MAX_ARCHIVE_BYTES = 512L * 1024 * 1024;
    static final long MAX_EXPANDED_CANDIDATE_BYTES = 512L * 1024 * 1024;
    static final long RATIO_CHECK_THRESHOLD_BYTES = 1024L * 1024;
    static final long MAX_COMPRESSION_RATIO = 200;
    static final int SEVEN_Z_MEMORY_LIMIT_KIB = 128 * 1024;
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
        if (source == null || displayName == null || supportedHashes == null) {
            return Result.failure(Error.UNSUPPORTED_INPUT);
        }

        String lowerName = displayName.toLowerCase(Locale.ROOT);
        try {
            ensureWorkDirectory();
            if (lowerName.endsWith(".zip")) {
                if (source.length() > MAX_ARCHIVE_BYTES) {
                    return Result.failure(Error.UNSAFE_ARCHIVE);
                }
                return importZip(source, supportedHashes);
            }
            if (lowerName.endsWith(".7z")) {
                if (source.length() > MAX_ARCHIVE_BYTES) {
                    return Result.failure(Error.UNSAFE_ARCHIVE);
                }
                return importSevenZ(source, supportedHashes);
            }
            if (!lowerName.endsWith(".z64")) {
                return Result.failure(Error.UNSUPPORTED_INPUT);
            }
            return importRaw(source, supportedHashes);
        } catch (EncryptedArchiveException e) {
            return Result.failure(Error.ENCRYPTED_ARCHIVE);
        } catch (UnsafeInputException e) {
            return Result.failure(Error.UNSAFE_ARCHIVE);
        } catch (IOException e) {
            return Result.failure(Error.IO_ERROR);
        }
    }

    private Result importRaw(File source, Set<String> supportedHashes) throws IOException, UnsafeInputException {
        Candidate candidate;
        try (InputStream input = new FileInputStream(source)) {
            candidate = copyAndHash(input);
        }
        if (!supportedHashes.contains(candidate.sha1)) {
            deleteQuietly(candidate.file);
            return Result.failure(Error.NO_SUPPORTED_ROM);
        }
        return Result.success(candidate.file);
    }

    private Result importZip(File source, Set<String> supportedHashes)
            throws IOException, UnsafeInputException, EncryptedArchiveException {
        Candidate accepted = null;
        long expandedTotal = 0;
        try (ZipFile zip = ZipFile.builder().setFile(source).get()) {
            Enumeration<ZipArchiveEntry> entries = zip.getEntries();
            while (entries.hasMoreElements()) {
                ZipArchiveEntry entry = entries.nextElement();
                if (entry.isDirectory() || !isZ64(entry.getName())) {
                    continue;
                }
                validateEntryName(entry.getName());
                if (entry.getGeneralPurposeBit().usesEncryption()) {
                    throw new EncryptedArchiveException();
                }
                validateDeclaredSize(entry.getSize());

                Candidate candidate;
                try (InputStream input = zip.getInputStream(entry)) {
                    candidate = copyAndHash(input);
                }
                try {
                    expandedTotal = addExpandedBytes(expandedTotal, candidate.size);
                    validateCompressionRatio(candidate.size, entry.getCompressedSize());
                } catch (UnsafeInputException e) {
                    deleteQuietly(candidate.file);
                    throw e;
                }

                if (!supportedHashes.contains(candidate.sha1)) {
                    deleteQuietly(candidate.file);
                    continue;
                }
                if (accepted != null) {
                    deleteQuietly(accepted.file);
                    deleteQuietly(candidate.file);
                    return Result.failure(Error.MULTIPLE_SUPPORTED_ROMS);
                }
                accepted = candidate;
            }
        } catch (IOException | UnsafeInputException | EncryptedArchiveException e) {
            if (accepted != null) {
                deleteQuietly(accepted.file);
            }
            throw e;
        }

        if (accepted == null) {
            return Result.failure(Error.NO_SUPPORTED_ROM);
        }
        return Result.success(accepted.file);
    }

    private Result importSevenZ(File source, Set<String> supportedHashes)
            throws IOException, UnsafeInputException, EncryptedArchiveException {
        Candidate accepted = null;
        long expandedTotal = 0;
        try (SevenZFile sevenZ = SevenZFile.builder()
                .setFile(source)
                .setMaxMemoryLimitKiB(SEVEN_Z_MEMORY_LIMIT_KIB)
                .get()) {
            for (SevenZArchiveEntry entry; (entry = sevenZ.getNextEntry()) != null;) {
                if (entry.isDirectory() || !entry.hasStream() || !isZ64(entry.getName())) {
                    continue;
                }
                validateEntryName(entry.getName());
                validateDeclaredSize(entry.getSize());

                Candidate candidate;
                long compressedSize;
                try (InputStream input = sevenZ.getInputStream(entry)) {
                    candidate = copyAndHash(input);
                    InputStreamStatistics statistics = sevenZ.getStatisticsForCurrentEntry();
                    compressedSize = statistics == null ? -1 : statistics.getCompressedCount();
                }
                try {
                    expandedTotal = addExpandedBytes(expandedTotal, candidate.size);
                    validateCompressionRatio(candidate.size, compressedSize);
                } catch (UnsafeInputException e) {
                    deleteQuietly(candidate.file);
                    throw e;
                }

                if (!supportedHashes.contains(candidate.sha1)) {
                    deleteQuietly(candidate.file);
                    continue;
                }
                if (accepted != null) {
                    deleteQuietly(accepted.file);
                    deleteQuietly(candidate.file);
                    return Result.failure(Error.MULTIPLE_SUPPORTED_ROMS);
                }
                accepted = candidate;
            }
        } catch (PasswordRequiredException e) {
            if (accepted != null) {
                deleteQuietly(accepted.file);
            }
            throw new EncryptedArchiveException();
        } catch (IOException | UnsafeInputException e) {
            if (accepted != null) {
                deleteQuietly(accepted.file);
            }
            throw e;
        }

        if (accepted == null) {
            return Result.failure(Error.NO_SUPPORTED_ROM);
        }
        return Result.success(accepted.file);
    }

    private Candidate copyAndHash(InputStream input) throws IOException, UnsafeInputException {
        MessageDigest sha1 = sha1();
        File candidate = File.createTempFile("rom-candidate-", ".z64", workDirectory);
        byte[] buffer = new byte[BUFFER_SIZE];
        long written = 0;
        try (OutputStream output = new FileOutputStream(candidate)) {
            for (int read; (read = input.read(buffer)) != -1;) {
                written += read;
                if (written > MAX_ROM_BYTES) {
                    throw new UnsafeInputException();
                }
                sha1.update(buffer, 0, read);
                output.write(buffer, 0, read);
            }
        } catch (IOException | UnsafeInputException e) {
            deleteQuietly(candidate);
            throw e;
        }
        return new Candidate(candidate, hex(sha1.digest()), written);
    }

    private static boolean isZ64(String name) {
        return name != null && name.toLowerCase(Locale.ROOT).endsWith(".z64");
    }

    private static void validateEntryName(String name) throws UnsafeInputException {
        String normalized = name.replace('\\', '/');
        if (normalized.startsWith("/") || normalized.matches("^[A-Za-z]:.*")) {
            throw new UnsafeInputException();
        }
        for (String segment : normalized.split("/")) {
            if (segment.equals("..")) {
                throw new UnsafeInputException();
            }
        }
    }

    private static void validateDeclaredSize(long size) throws UnsafeInputException {
        if (size > MAX_ROM_BYTES) {
            throw new UnsafeInputException();
        }
    }

    private static long addExpandedBytes(long current, long added) throws UnsafeInputException {
        if (added > MAX_EXPANDED_CANDIDATE_BYTES - current) {
            throw new UnsafeInputException();
        }
        return current + added;
    }

    private static void validateCompressionRatio(long expanded, long compressed) throws UnsafeInputException {
        if (expanded >= RATIO_CHECK_THRESHOLD_BYTES && compressed > 0
                && expanded / compressed > MAX_COMPRESSION_RATIO) {
            throw new UnsafeInputException();
        }
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

    private static final class EncryptedArchiveException extends Exception {
    }

    private static final class Candidate {
        final File file;
        final String sha1;
        final long size;

        Candidate(File file, String sha1, long size) {
            this.file = file;
            this.sha1 = sha1;
            this.size = size;
        }
    }
}
