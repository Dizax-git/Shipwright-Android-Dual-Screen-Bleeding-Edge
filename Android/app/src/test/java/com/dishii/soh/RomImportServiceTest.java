package com.dishii.soh;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.LinkedHashMap;
import java.util.Collections;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;
import org.apache.commons.compress.archivers.sevenz.SevenZArchiveEntry;
import org.apache.commons.compress.archivers.sevenz.SevenZOutputFile;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class RomImportServiceTest {
    private static final byte[] SUPPORTED_BYTES = new byte[] { 1, 3, 3, 7 };

    @Rule
    public final TemporaryFolder temporary = new TemporaryFolder();

    @Test
    public void supportedRawZ64ProducesPendingCandidate() throws Exception {
        File work = temporary.newFolder("work");
        File source = write(temporary.newFile("owned.z64"), SUPPORTED_BYTES);
        RomImportService service = new RomImportService(work);

        RomImportService.Result result = service.importFile(source, "owned.z64", hashes(SUPPORTED_BYTES));

        assertTrue(result.isSuccess());
        assertArrayEquals(SUPPORTED_BYTES, Files.readAllBytes(result.getCandidate().toPath()));
    }

    @Test
    public void unsupportedRawZ64PreservesExistingRom() throws Exception {
        File work = temporary.newFolder("work");
        File existing = write(new File(work, "OOT.z64"), new byte[] { 9, 8, 7 });
        File source = write(temporary.newFile("wrong.z64"), new byte[] { 1 });
        RomImportService service = new RomImportService(work);

        RomImportService.Result result = service.importFile(source, "wrong.z64", hashes(SUPPORTED_BYTES));

        assertFalse(result.isSuccess());
        assertEquals(RomImportService.Error.NO_SUPPORTED_ROM, result.getError());
        assertArrayEquals(new byte[] { 9, 8, 7 }, Files.readAllBytes(existing.toPath()));
        assertEquals(1, work.listFiles().length);
    }

    @Test
    public void rawSelectionRequiresZ64Extension() throws Exception {
        File work = temporary.newFolder("work");
        File source = write(temporary.newFile("owned.bin"), SUPPORTED_BYTES);
        RomImportService service = new RomImportService(work);

        RomImportService.Result result = service.importFile(source, "owned.bin", hashes(SUPPORTED_BYTES));

        assertEquals(RomImportService.Error.UNSUPPORTED_INPUT, result.getError());
        assertEquals(0, work.listFiles().length);
    }

    @Test
    public void zipIteratesUntilTheSingleSupportedMatch() throws Exception {
        File work = temporary.newFolder("work");
        Map<String, byte[]> entries = new LinkedHashMap<>();
        entries.put("notes.txt", new byte[] { 4 });
        entries.put("wrong.z64", new byte[] { 5 });
        entries.put("roms/OOT.Z64", SUPPORTED_BYTES);
        File archive = zip(temporary.newFile("owned.zip"), entries);

        RomImportService.Result result = new RomImportService(work)
                .importFile(archive, "owned.zip", hashes(SUPPORTED_BYTES));

        assertTrue(result.isSuccess());
        assertArrayEquals(SUPPORTED_BYTES, Files.readAllBytes(result.getCandidate().toPath()));
    }

    @Test
    public void zipRejectsTwoSupportedEntriesAndCleansCandidates() throws Exception {
        File work = temporary.newFolder("work");
        Map<String, byte[]> entries = new LinkedHashMap<>();
        entries.put("one.z64", SUPPORTED_BYTES);
        entries.put("two.z64", SUPPORTED_BYTES);
        File archive = zip(temporary.newFile("duplicate.zip"), entries);

        RomImportService.Result result = new RomImportService(work)
                .importFile(archive, "duplicate.zip", hashes(SUPPORTED_BYTES));

        assertEquals(RomImportService.Error.MULTIPLE_SUPPORTED_ROMS, result.getError());
        assertEquals(0, work.listFiles().length);
    }

    @Test
    public void zipWithNoSupportedRomCleansCandidates() throws Exception {
        File work = temporary.newFolder("work");
        Map<String, byte[]> entries = new LinkedHashMap<>();
        entries.put("wrong.z64", new byte[] { 5 });
        File archive = zip(temporary.newFile("wrong.zip"), entries);

        RomImportService.Result result = new RomImportService(work)
                .importFile(archive, "wrong.zip", hashes(SUPPORTED_BYTES));

        assertEquals(RomImportService.Error.NO_SUPPORTED_ROM, result.getError());
        assertEquals(0, work.listFiles().length);
    }

    @Test
    public void zipRejectsTraversalEntryNames() throws Exception {
        File work = temporary.newFolder("work");
        Map<String, byte[]> entries = new LinkedHashMap<>();
        entries.put("../OOT.z64", SUPPORTED_BYTES);
        File archive = zip(temporary.newFile("traversal.zip"), entries);

        RomImportService.Result result = new RomImportService(work)
                .importFile(archive, "traversal.zip", hashes(SUPPORTED_BYTES));

        assertEquals(RomImportService.Error.UNSAFE_ARCHIVE, result.getError());
        assertEquals(0, work.listFiles().length);
    }

    @Test
    public void zipRejectsSuspiciousCompressionRatio() throws Exception {
        File work = temporary.newFolder("work");
        byte[] highlyCompressible = new byte[2 * 1024 * 1024];
        Map<String, byte[]> entries = new LinkedHashMap<>();
        entries.put("OOT.z64", highlyCompressible);
        File archive = zip(temporary.newFile("ratio.zip"), entries);

        RomImportService.Result result = new RomImportService(work)
                .importFile(archive, "ratio.zip", hashes(highlyCompressible));

        assertEquals(RomImportService.Error.UNSAFE_ARCHIVE, result.getError());
        assertEquals(0, work.listFiles().length);
    }

    @Test
    public void sevenZFindsOneSupportedEntryAmongSeveralCandidates() throws Exception {
        File work = temporary.newFolder("work");
        Map<String, byte[]> entries = new LinkedHashMap<>();
        entries.put("wrong.z64", new byte[] { 5 });
        entries.put("folder/oot.z64", SUPPORTED_BYTES);
        File archive = sevenZ(temporary.newFile("owned.7z"), entries);

        RomImportService.Result result = new RomImportService(work)
                .importFile(archive, "owned.7z", hashes(SUPPORTED_BYTES));

        assertTrue(result.isSuccess());
        assertArrayEquals(SUPPORTED_BYTES, Files.readAllBytes(result.getCandidate().toPath()));
    }

    @Test
    public void sevenZRejectsTwoSupportedEntriesAndCleansCandidates() throws Exception {
        File work = temporary.newFolder("work");
        Map<String, byte[]> entries = new LinkedHashMap<>();
        entries.put("one.z64", SUPPORTED_BYTES);
        entries.put("two.Z64", SUPPORTED_BYTES);
        File archive = sevenZ(temporary.newFile("duplicate.7z"), entries);

        RomImportService.Result result = new RomImportService(work)
                .importFile(archive, "duplicate.7z", hashes(SUPPORTED_BYTES));

        assertEquals(RomImportService.Error.MULTIPLE_SUPPORTED_ROMS, result.getError());
        assertEquals(0, work.listFiles().length);
    }

    @Test
    public void sevenZRejectsTraversalEntryNames() throws Exception {
        File work = temporary.newFolder("work");
        Map<String, byte[]> entries = new LinkedHashMap<>();
        entries.put("../OOT.z64", SUPPORTED_BYTES);
        File archive = sevenZ(temporary.newFile("traversal.7z"), entries);

        RomImportService.Result result = new RomImportService(work)
                .importFile(archive, "traversal.7z", hashes(SUPPORTED_BYTES));

        assertEquals(RomImportService.Error.UNSAFE_ARCHIVE, result.getError());
        assertEquals(0, work.listFiles().length);
    }

    private static File write(File file, byte[] bytes) throws Exception {
        try (FileOutputStream output = new FileOutputStream(file)) {
            output.write(bytes);
        }
        return file;
    }

    private static Set<String> hashes(byte[] bytes) throws Exception {
        return Collections.singleton(hex(MessageDigest.getInstance("SHA-1").digest(bytes)));
    }

    private static File zip(File archive, Map<String, byte[]> entries) throws Exception {
        try (ZipOutputStream output = new ZipOutputStream(new FileOutputStream(archive))) {
            for (Map.Entry<String, byte[]> entry : entries.entrySet()) {
                output.putNextEntry(new ZipEntry(entry.getKey()));
                output.write(entry.getValue());
                output.closeEntry();
            }
        }
        return archive;
    }

    private static File sevenZ(File archive, Map<String, byte[]> entries) throws Exception {
        try (SevenZOutputFile output = new SevenZOutputFile(archive)) {
            for (Map.Entry<String, byte[]> item : entries.entrySet()) {
                SevenZArchiveEntry entry = new SevenZArchiveEntry();
                entry.setName(item.getKey());
                entry.setSize(item.getValue().length);
                output.putArchiveEntry(entry);
                output.write(item.getValue());
                output.closeArchiveEntry();
            }
        }
        return archive;
    }

    private static String hex(byte[] bytes) {
        StringBuilder output = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            output.append(String.format("%02x", value & 0xff));
        }
        return output.toString();
    }
}
