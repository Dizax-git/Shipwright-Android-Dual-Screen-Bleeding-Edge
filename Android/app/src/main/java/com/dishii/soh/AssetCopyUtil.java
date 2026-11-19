package com.dishii.soh;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.AtomicFile;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Enumeration;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class AssetCopyUtil {

    /** Called from the copy threads with the number of files written so far. */
    public interface Progress {
        void onProgress(int done, int total);
    }

    // Writes on /sdcard go through Android's FUSE layer, where each file costs a few
    // round trips. A few writers in parallel hide most of that latency.
    private static final int COPY_THREADS = 4;
    private static final int PROGRESS_STEP = 50;

    public static boolean needsBundledAssetRefresh(int currentVersion, int copiedVersion, boolean hasTorchConfig) {
        return currentVersion != copiedVersion || !hasTorchConfig;
    }

    public static void copyBundledFile(Context context, String assetPath, File destination) throws IOException {
        AtomicFile atomic = new AtomicFile(destination);
        FileOutputStream output = null;
        try (InputStream input = context.getAssets().open(assetPath)) {
            output = atomic.startWrite();
            byte[] buffer = new byte[65536];
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
            atomic.finishWrite(output);
        } catch (IOException error) {
            atomic.failWrite(output);
            throw error;
        }
    }

    public static void copyAssetsToExternal(Context context, String assetsFolderPath, String externalFolderPath) throws IOException {
        copyAssetsToExternal(context, assetsFolderPath, externalFolderPath, null);
    }

    /**
     * Copies every file of an APK asset folder to external storage.
     *
     * The previous version asked AssetManager.list() for every one of the ~28,000 asset
     * files (each call scans the APK) and wrote each file atomically (temp file, fsync,
     * rename). On /sdcard that took well over 5 minutes on a Thor. Now the file list comes
     * from one pass over the APK, and small files are written directly by a few threads.
     * Per-file atomicity is not needed: MainActivity only records the copied version once
     * every file and soh.o2r are written, so an interrupted copy is simply redone.
     */
    public static void copyAssetsToExternal(Context context, String assetsFolderPath, String externalFolderPath,
                                            Progress progress) throws IOException {
        AssetManager assetManager = context.getAssets();
        List<String> files = null;
        try {
            files = listAssetFilesInApk(context.getPackageCodePath(), assetsFolderPath);
        } catch (IOException | RuntimeException ignored) {
            // Fall back to AssetManager below.
        }
        if (files == null || files.isEmpty()) {
            files = new ArrayList<>();
            listAssetFilesSlow(assetManager, assetsFolderPath, "", files);
        }

        File root = new File(externalFolderPath);
        for (String directory : parentDirectories(files)) {
            File dir = directory.isEmpty() ? root : new File(root, directory);
            if (!dir.isDirectory() && !dir.mkdirs() && !dir.isDirectory()) {
                throw new IOException("Could not create " + dir.getAbsolutePath());
            }
        }

        final int total = files.size();
        final AtomicInteger done = new AtomicInteger();
        if (progress != null) {
            progress.onProgress(0, total);
        }

        // If one file fails, the other threads stop at their next file, and this method only
        // returns once every thread has stopped: no write or progress call after the error
        // (the caller shows the Retry dialog, which must not be covered or raced).
        final AtomicBoolean failed = new AtomicBoolean();
        ExecutorService pool = Executors.newFixedThreadPool(COPY_THREADS);
        IOException error = null;
        try {
            List<Future<?>> tasks = new ArrayList<>();
            for (List<String> chunk : split(files, COPY_THREADS)) {
                tasks.add(pool.submit(() -> {
                    byte[] buffer = new byte[65536];
                    for (String relative : chunk) {
                        if (failed.get()) {
                            return null;
                        }
                        try {
                            copyAssetDirect(assetManager, assetsFolderPath + "/" + relative,
                                    new File(root, relative), buffer);
                        } catch (IOException | RuntimeException e) {
                            failed.set(true);
                            throw e;
                        }
                        int count = done.incrementAndGet();
                        if (progress != null && !failed.get() && (count % PROGRESS_STEP == 0 || count == total)) {
                            progress.onProgress(count, total);
                        }
                    }
                    return null;
                }));
            }
            for (Future<?> task : tasks) {
                try {
                    task.get();
                } catch (java.util.concurrent.ExecutionException e) {
                    failed.set(true);
                    if (error == null) {
                        Throwable cause = e.getCause();
                        error = cause instanceof IOException ? (IOException) cause
                                : new IOException("Asset copy failed", cause);
                    }
                } catch (InterruptedException e) {
                    failed.set(true);
                    Thread.currentThread().interrupt();
                    if (error == null) {
                        error = new IOException("Asset copy interrupted", e);
                    }
                }
            }
        } finally {
            pool.shutdown();
        }
        if (error != null) {
            throw error;
        }
    }

    /** Relative paths (with '/') of the files under assets/<folder>/ in the APK, in one pass. */
    static List<String> listAssetFilesInApk(String apkPath, String assetsFolderPath) throws IOException {
        List<String> names = new ArrayList<>();
        try (ZipFile apk = new ZipFile(apkPath)) {
            Enumeration<? extends ZipEntry> entries = apk.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                if (!entry.isDirectory()) {
                    names.add(entry.getName());
                }
            }
        }
        return filesUnder(names, "assets/" + assetsFolderPath + "/");
    }

    /** Keeps the entries under prefix, returned relative to it and sorted. */
    static List<String> filesUnder(Iterable<String> entryNames, String prefix) {
        List<String> files = new ArrayList<>();
        for (String name : entryNames) {
            if (name.startsWith(prefix) && name.length() > prefix.length() && !name.endsWith("/")) {
                String relative = name.substring(prefix.length());
                // Never write outside the target folder.
                if (!relative.startsWith("/") && !("/" + relative + "/").contains("/../")) {
                    files.add(relative);
                }
            }
        }
        Collections.sort(files);
        return files;
    }

    /** Every folder that must exist before the files are written ("" = the root). */
    static Set<String> parentDirectories(List<String> files) {
        Set<String> directories = new LinkedHashSet<>();
        directories.add("");
        for (String file : files) {
            int slash = file.lastIndexOf('/');
            if (slash > 0) {
                directories.add(file.substring(0, slash));
            }
        }
        return directories;
    }

    static <T> List<List<T>> split(List<T> items, int parts) {
        List<List<T>> chunks = new ArrayList<>();
        for (int i = 0; i < parts; i++) {
            chunks.add(new ArrayList<>());
        }
        for (int i = 0; i < items.size(); i++) {
            chunks.get(i % parts).add(items.get(i));
        }
        return chunks;
    }

    private static void listAssetFilesSlow(AssetManager assetManager, String assetRoot, String relative,
                                           List<String> out) throws IOException {
        String path = relative.isEmpty() ? assetRoot : assetRoot + "/" + relative;
        String[] children = assetManager.list(path);
        if (children == null || children.length == 0) {
            if (!relative.isEmpty()) {
                out.add(relative);
            }
            return;
        }
        for (String child : children) {
            listAssetFilesSlow(assetManager, assetRoot, relative.isEmpty() ? child : relative + "/" + child, out);
        }
    }

    private static void copyAssetDirect(AssetManager assetManager, String assetPath, File destination,
                                        byte[] buffer) throws IOException {
        try (InputStream input = assetManager.open(assetPath);
             OutputStream output = new FileOutputStream(destination)) {
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
        }
    }

    public static void copyDirectory(File sourceDir, File targetDir) throws IOException {
        if (!targetDir.exists()) {
            targetDir.mkdirs();
        }
        for (File file : sourceDir.listFiles()) {
            File dest = new File(targetDir, file.getName());
            if (file.isDirectory()) {
                copyDirectory(file, dest);
            } else {
                copyFile(file, dest);
            }
        }
    }

    public static void copyFile(File source, File dest) throws IOException {
        try (InputStream in = new FileInputStream(source);
             OutputStream out = new FileOutputStream(dest)) {
            byte[] buf = new byte[64*1024];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
        }
    }
}
