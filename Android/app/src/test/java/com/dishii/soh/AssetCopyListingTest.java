package com.dishii.soh;

import org.junit.Test;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import static org.junit.Assert.*;

public class AssetCopyListingTest {
    @Test public void keepsOnlyFilesUnderTheFolderRelativeAndSorted() {
        List<String> files = AssetCopyUtil.filesUnder(Arrays.asList(
                "assets/soh.o2r",
                "assets/assets/extractor/symbols/b.xml",
                "assets/assets/config.yml",
                "assets/assets/extractor/",
                "assets/assetsX/other.txt",
                "lib/arm64-v8a/libsoh.so"), "assets/assets/");
        assertEquals(Arrays.asList("config.yml", "extractor/symbols/b.xml"), files);
    }

    @Test public void refusesPathsThatLeaveTheFolder() {
        List<String> files = AssetCopyUtil.filesUnder(Arrays.asList(
                "assets/assets/../evil.txt",
                "assets/assets/a/../../evil.txt",
                "assets/assets//abs.txt",
                "assets/assets/ok..txt"), "assets/assets/");
        assertEquals(Arrays.asList("ok..txt"), files);
    }

    @Test public void createsEveryParentFolderOnce() {
        Set<String> dirs = AssetCopyUtil.parentDirectories(Arrays.asList(
                "config.yml", "extractor/a.xml", "extractor/b.xml", "extractor/symbols/c.xml"));
        assertEquals(Arrays.asList("", "extractor", "extractor/symbols"), Arrays.asList(dirs.toArray()));
    }

    @Test public void splitKeepsEveryFileExactlyOnce() {
        List<List<Integer>> parts = AssetCopyUtil.split(Arrays.asList(1, 2, 3, 4, 5, 6, 7), 4);
        assertEquals(4, parts.size());
        int count = 0;
        for (List<Integer> part : parts) {
            count += part.size();
        }
        assertEquals(7, count);
        assertEquals(Arrays.asList(1, 5), parts.get(0));
    }
}
