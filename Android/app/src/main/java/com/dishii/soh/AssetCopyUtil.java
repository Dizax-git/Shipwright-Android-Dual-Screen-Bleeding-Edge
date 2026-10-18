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

public class AssetCopyUtil {

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
        AssetManager assetManager = context.getAssets();
        String[] assetFiles = assetManager.list(assetsFolderPath);

        for (String assetFile : assetFiles) {
            String assetPath = assetsFolderPath + File.separator + assetFile;
            String externalPath = externalFolderPath + File.separator + assetFile;

            if (assetManager.list(assetPath).length > 0) {
                // It's a directory
                // Check if the directory exists in the external storage
                File externalDir = new File(externalPath);
                if (!externalDir.exists()) {
                    externalDir.mkdirs(); // Create the directory if it doesn't exist
                }

                // Recursively copy contents of the directory
                copyAssetsToExternal(context, assetPath, externalPath);
            } else {
                // Only APK-owned extractor metadata is refreshed here, never user ROMs or saves.
                copyBundledFile(context, assetPath, new File(externalPath));
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
