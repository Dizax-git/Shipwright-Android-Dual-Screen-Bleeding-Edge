package com.dishii.soh;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Locale;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class SupportedRomHashes {
    private static final Pattern SHA1_FIELD = Pattern.compile(
            "\\\"sha1\\\"\\s*:\\s*\\\"([0-9a-fA-F]{40})\\\"");
    private static final int BUFFER_SIZE = 8192;

    private SupportedRomHashes() {
    }

    static Set<String> load(InputStream input) throws IOException {
        if (input == null) {
            throw new IOException("Supported ROM hash data is missing");
        }

        String json = readUtf8(input);
        Matcher matcher = SHA1_FIELD.matcher(json);
        Set<String> hashes = new LinkedHashSet<>();
        while (matcher.find()) {
            String hash = matcher.group(1).toLowerCase(Locale.ROOT);
            if (!hashes.add(hash)) {
                throw new IOException("Duplicate supported ROM hash");
            }
        }
        if (hashes.isEmpty()) {
            throw new IOException("No supported ROM hashes found");
        }
        return Collections.unmodifiableSet(hashes);
    }

    private static String readUtf8(InputStream input) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[BUFFER_SIZE];
        for (int read; (read = input.read(buffer)) != -1;) {
            output.write(buffer, 0, read);
        }
        return new String(output.toByteArray(), StandardCharsets.UTF_8);
    }
}
