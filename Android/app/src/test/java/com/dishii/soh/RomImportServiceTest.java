package com.dishii.soh;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.Collections;
import java.util.Set;
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

    private static File write(File file, byte[] bytes) throws Exception {
        try (FileOutputStream output = new FileOutputStream(file)) {
            output.write(bytes);
        }
        return file;
    }

    private static Set<String> hashes(byte[] bytes) throws Exception {
        return Collections.singleton(hex(MessageDigest.getInstance("SHA-1").digest(bytes)));
    }

    private static String hex(byte[] bytes) {
        StringBuilder output = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            output.append(String.format("%02x", value & 0xff));
        }
        return output.toString();
    }
}
