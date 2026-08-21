package com.dishii.soh;

import static org.junit.Assert.assertEquals;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.Collections;
import org.junit.Test;

public class SupportedRomHashesTest {
    @Test
    public void loadsLowercaseSha1Values() throws Exception {
        String json = "[{\"name\":\"US\",\"sha1\":\"AD69C91157F6705E8AB06C79FE08AAD47BB57BA7\"}]";

        assertEquals(
                Collections.singleton("ad69c91157f6705e8ab06c79fe08aad47bb57ba7"),
                SupportedRomHashes.load(bytes(json)));
    }

    @Test(expected = IOException.class)
    public void rejectsDocumentsWithoutHashes() throws Exception {
        SupportedRomHashes.load(bytes("[]"));
    }

    @Test(expected = IOException.class)
    public void rejectsDuplicateHashes() throws Exception {
        String hash = "ad69c91157f6705e8ab06c79fe08aad47bb57ba7";
        SupportedRomHashes.load(bytes("[{\"sha1\":\"" + hash + "\"},{\"sha1\":\"" + hash + "\"}]"));
    }

    private static InputStream bytes(String value) {
        return new ByteArrayInputStream(value.getBytes(StandardCharsets.UTF_8));
    }
}
