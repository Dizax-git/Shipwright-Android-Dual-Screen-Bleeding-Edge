
package com.dishii.soh;
import org.libsdl.app.SDLActivity;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.database.Cursor;
import android.provider.OpenableColumns;
import android.system.ErrnoException;
import android.system.Os;

import android.provider.Settings;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;

import android.Manifest;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.os.Build;
import android.view.WindowManager;
import android.widget.Toast;

import android.util.Log;

import android.view.KeyEvent;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.inputmethod.InputMethodManager;
import android.view.ViewTreeObserver;
import android.widget.ImageView;
import java.util.Arrays;

import java.util.concurrent.Executors;
import android.app.AlertDialog;

import android.view.InputDevice;
import android.os.Vibrator;
import android.os.VibrationEffect;

//This class is the main SDLActivity and just sets up a bunch of default files
public class MainActivity extends SDLActivity{

    // Called by the menu's search field after an explicit tap (Menu.cpp,
    // AndroidRequestSearchKeyboard). SDL2 keeps its text-input state even if
    // Back dismissed the keyboard, so SDL_StartTextInput() alone does not always
    // reopen it. Same as the 2 Ship (Majora) Thor build.
    public void requestSearchKeyboardFromNative() {
        runOnUiThread(() -> {
            View editor = SDLActivity.mTextEdit;
            if (editor == null) {
                // SDL's editor does not exist yet: SDL creates it and opens the keyboard.
                SDLActivity.showTextInput(0, 0, 1, 1);
                return;
            }

            editor.setVisibility(View.VISIBLE);
            editor.requestFocus();
            editor.post(() -> {
                if (!editor.hasFocus() || !editor.hasWindowFocus()) {
                    return;
                }
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    WindowInsetsController controller = editor.getWindowInsetsController();
                    if (controller != null) {
                        controller.show(WindowInsets.Type.ime());
                        return;
                    }
                }
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.showSoftInput(editor, 0);
                }
            });
        });
    }

    SharedPreferences preferences;
    private static final CountDownLatch setupLatch = new CountDownLatch(1);
    private volatile boolean mIsAiming = false;
    private static final int COPY_BUFFER_SIZE = 65536;
    private static final int RUMBLE_MAX_DURATION_MS = 5000;
    private final ExecutorService romImportExecutor = Executors.newSingleThreadExecutor();
    private DualScreenManager dualScreenManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        moveTaskToMainDisplayIfNeeded();
        super.onCreate(savedInstanceState);
        ImmersiveMode.apply(getWindow());

        Log.i("SoH", "onCreate start");

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        preferences = getSharedPreferences("com.dishii.soh.prefs", Context.MODE_PRIVATE);

        Log.i("SoH", "hasStoragePermission=" + hasStoragePermission());

        // Check if storage permissions are granted
        if (hasStoragePermission()) {
            startSetupIfPermitted();
        } else {
            permissionAsked = true;
            requestStoragePermission();
        }

        setupControllerOverlay();
        attachControllerOverlayToPrimary();
        attachController();
        dualScreenManager = new DualScreenManager(this);

        Log.i("SoH", "onCreate complete");
    }

    public static void waitForSetupFromNative() {
        try {
            setupLatch.await();  // Block until setup is complete
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    // Setup must start exactly once, as soon as file access is granted. The "all files access"
    // page is opened from a singleInstance activity, so Android returns RESULT_CANCELED to
    // onActivityResult right away, before the user has decided: onResume checks again when the
    // user comes back from that page (and onRequestPermissionsResult on Android 6-10).
    private boolean setupStarted;
    private boolean permissionAsked;

    private void startSetupIfPermitted() {
        if (setupStarted || !hasStoragePermission()) {
            return;
        }
        setupStarted = true;
        doVersionCheck();
        checkAndSetupFiles();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == STORAGE_PERMISSION_REQUEST_CODE) {
            startSetupIfPermitted();
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults); // SDL's own requests
        }
    }

    private void doVersionCheck(){
        int currentVersion = BuildConfig.VERSION_CODE;
        int storedVersion = preferences.getInt("appVersion", currentVersion);

        if (currentVersion > storedVersion) {
            deleteOutdatedAssets();
            preferences.edit().putInt("appVersion", currentVersion).apply();
        }
    }

    private void deleteOutdatedAssets() {
        File targetRootFolder = new File(Environment.getExternalStorageDirectory(), "SOH");

        File sohFile = new File(targetRootFolder, "soh.otr");
        File ootFile = new File(targetRootFolder, "oot.otr");
        File ootMqFile = new File(targetRootFolder, "oot-mq.otr");
        File sohO2rFile = new File(targetRootFolder, "soh.o2r");
        File ootO2rFile = new File(targetRootFolder, "oot.o2r");
        File ootMqO2rFile = new File(targetRootFolder, "oot-mq.o2r");
        File assetsFolder = new File(targetRootFolder, "assets");

        deleteIfExists(sohFile);
        deleteIfExists(ootFile);
        deleteIfExists(ootMqFile);
        deleteIfExists(sohO2rFile);
        deleteIfExists(ootO2rFile);
        deleteIfExists(ootMqO2rFile);
        deleteRecursiveIfExists(assetsFolder);
    }

    private void deleteIfExists(File file) {
        if (file.exists()) {
            if (file.delete()) {
                Log.i("deleteAssets", "Deleted file: " + file.getAbsolutePath());
            } else {
                Log.w("deleteAssets", "Failed to delete file: " + file.getAbsolutePath());
            }
        } else {
            Log.i("deleteAssets", "File not found (skipped): " + file.getAbsolutePath());
        }
    }

    private void deleteRecursiveIfExists(File dir) {
        if (dir.exists()) {
            deleteRecursive(dir);
            Log.i("deleteAssets", "Deleted directory: " + dir.getAbsolutePath());
        } else {
            Log.i("deleteAssets", "Directory not found (skipped): " + dir.getAbsolutePath());
        }
    }

    private void deleteRecursive(File fileOrDirectory) {
        if (fileOrDirectory.isDirectory()) {
            File[] children = fileOrDirectory.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursive(child);
                }
            }
        }
        fileOrDirectory.delete();
    }



    // Check if storage permission is granted
    private boolean hasStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            // Android 11 and above
            return Environment.isExternalStorageManager();
        } else {
            // Android 10 and below
            return ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
                    == PackageManager.PERMISSION_GRANTED &&
                    ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                            == PackageManager.PERMISSION_GRANTED;
        }
    }

    private static final int STORAGE_PERMISSION_REQUEST_CODE = 2296;
    private static final int FILE_PICKER_REQUEST_CODE = 0;

    private void requestStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            // Android 11+ → MANAGE_EXTERNAL_STORAGE
            if (!Environment.isExternalStorageManager()) {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.setData(Uri.parse("package:" + getPackageName()));
                startActivityForResult(intent, STORAGE_PERMISSION_REQUEST_CODE);
            } else {
                // Already granted
                startSetupIfPermitted();
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Android 6–10 → request READ/WRITE at runtime
            ActivityCompat.requestPermissions(this,
                    new String[]{
                            Manifest.permission.READ_EXTERNAL_STORAGE,
                            Manifest.permission.WRITE_EXTERNAL_STORAGE
                    },
                    STORAGE_PERMISSION_REQUEST_CODE);
        } else {
            // Below Android 6 → permissions granted at install time
            startSetupIfPermitted();
        }
    }

    public void checkAndSetupFiles() {
        File targetRootFolder = new File(Environment.getExternalStorageDirectory(), "SOH");
        File assetsFolder = new File(targetRootFolder, "assets");
        // Support both .otr (9.0.x) and .o2r (9.2.x) archive formats
        File sohOtrFile = new File(targetRootFolder, "soh.o2r");
        File sohOtrFileLegacy = new File(targetRootFolder, "soh.otr");
        // oot.o2r / oot-mq.o2r also count; soh.o2r is not bundled, game can run with just the ROM archive
        File ootO2rFile = new File(targetRootFolder, "oot.o2r");
        File ootMqO2rFile = new File(targetRootFolder, "oot-mq.o2r");

        boolean isMissingAssets = AssetCopyUtil.needsBundledAssetRefresh(
                BuildConfig.VERSION_CODE,
                getSharedPreferences("bundled-assets", MODE_PRIVATE).getInt("versionCode", 0),
                new File(assetsFolder, "config.yml").isFile());
        boolean isMissingSohOtr = !sohOtrFile.exists() && !sohOtrFileLegacy.exists() && !ootO2rFile.exists() && !ootMqO2rFile.exists();

        if (!targetRootFolder.exists() || isMissingAssets || isMissingSohOtr) {
            new AlertDialog.Builder(this)
                    .setTitle("Setup Required")
                    .setMessage("Ship of Harkinian needs to set up its files. This can take a few minutes; "
                            + "a counter shows the progress. Press OK to begin.")
                    .setCancelable(false)
                    .setPositiveButton("OK", (dialog, which) -> {
                        Executors.newSingleThreadExecutor().execute(() -> setupFilesInBackground(targetRootFolder));
                    })
                    .show();
        } else {
            // No setup needed; but always ensure soh.o2r is present from APK assets
            if (!sohOtrFile.exists()) {
                Executors.newSingleThreadExecutor().execute(() -> {
                    try {
                        try (InputStream in = getAssets().open("soh.o2r");
                             OutputStream out = new FileOutputStream(sohOtrFile)) {
                            byte[] buffer = new byte[COPY_BUFFER_SIZE];
                            int read;
                            while ((read = in.read(buffer)) != -1) {
                                out.write(buffer, 0, read);
                            }
                        }
                    } catch (IOException e) {
                        // not bundled, nothing to do
                    }
                    setupLatch.countDown();
                });
            } else {
                setupLatch.countDown();
            }
        }
    }


    private AlertDialog setupProgressDialog;

    // A black screen for several minutes looks like a hang, so the copy shows a counter.
    private void showSetupProgress(String message) {
        runOnUiThread(() -> {
            if (isFinishing()) {
                return;
            }
            if (setupProgressDialog == null) {
                setupProgressDialog = new AlertDialog.Builder(this)
                        .setTitle("Setting up files")
                        .setMessage(message)
                        .setCancelable(false)
                        .create();
                setupProgressDialog.show();
            } else {
                setupProgressDialog.setMessage(message);
            }
        });
    }

    private void hideSetupProgress() {
        runOnUiThread(() -> {
            if (setupProgressDialog != null) {
                setupProgressDialog.dismiss();
                setupProgressDialog = null;
            }
        });
    }

    private void setupFilesInBackground(File targetRootFolder) {
        showSetupProgress("Preparing...");

        File sourceOldRoot = getExternalFilesDir(null);
        File sourceSavesDir = new File(sourceOldRoot, "Save"); // how to tell if there's anything to migrate

        // === Migration from old Android/data/.../files/ directory ===
        if (sourceOldRoot != null && sourceSavesDir.isDirectory()) {
            Log.i("setupFiles", "Migrating old data from: " + sourceOldRoot.getAbsolutePath());

            File[] sourceFiles = sourceOldRoot.listFiles();
            if (sourceFiles != null) {
                for (File file : sourceFiles) {
                    String name = file.getName();
                    if (name.equals("assets") || name.equals("soh.otr") || name.equals("oot-mq.otr") || name.equals("oot.otr") || name.equals("soh.o2r") || name.equals("oot-mq.o2r") || name.equals("oot.o2r")) {
                        continue; // Skip these
                    }

                    File dest = new File(targetRootFolder, name);
                    try {
                        if (file.isDirectory()) {
                            AssetCopyUtil.copyDirectory(file, dest);
                        } else {
                            AssetCopyUtil.copyFile(file, dest);
                        }
                        Log.i("setupFiles", "Migrated: " + name);
                    } catch (IOException e) {
                        Log.e("setupFiles", "Failed to migrate: " + name, e);
                    }
                }
            }

            runOnUiThread(() -> Toast.makeText(this, "Save data migrated", Toast.LENGTH_SHORT).show());
        }

        // Ensure root folder exists
        if (!targetRootFolder.exists()) {
            targetRootFolder.mkdirs();
            if (!targetRootFolder.exists()) {
                Log.e("setupFiles", "Failed to create root folder");
                hideSetupProgress();
                runOnUiThread(() -> Toast.makeText(this, "Failed to create folder", Toast.LENGTH_LONG).show());
                setupLatch.countDown();
                return;
            }
        }

        // Always ensure mods folder exists
        File targetModsDir = new File(targetRootFolder, "mods");
        if (!targetModsDir.exists()) {
            targetModsDir.mkdirs();
        }

        // Copy assets/ from internal
        File targetAssetsDir = new File(targetRootFolder, "assets");
        try {
            if (!targetAssetsDir.exists()) {
                targetAssetsDir.mkdirs();
            }
            long started = android.os.SystemClock.elapsedRealtime();
            AssetCopyUtil.copyAssetsToExternal(this, "assets", targetAssetsDir.getAbsolutePath(),
                    (done, total) -> showSetupProgress(String.format(java.util.Locale.US,
                            "Copying game data: %,d / %,d files\n\nThis only happens on the first launch and after an update.",
                            done, total)));
            Log.i("setupFiles", "Assets copied in " + (android.os.SystemClock.elapsedRealtime() - started) + " ms");
        } catch (IOException e) {
            e.printStackTrace();
            showBundledAssetCopyError(targetRootFolder, e);
            return;
        }

        // Replace only the project-owned archive atomically; never touch oot.o2r or saves.
        try {
            File targetOtrFile = new File(targetRootFolder, "soh.o2r");
            showSetupProgress("Copying soh.o2r...");
            AssetCopyUtil.copyBundledFile(this, "soh.o2r", targetOtrFile);
            // Recorded only once everything is written, so an interrupted copy is redone.
            getSharedPreferences("bundled-assets", MODE_PRIVATE).edit()
                    .putInt("versionCode", BuildConfig.VERSION_CODE).apply();
        } catch (IOException e) {
            showBundledAssetCopyError(targetRootFolder, e);
            return;
        }

        hideSetupProgress();
        setupLatch.countDown();
    }

    private void showBundledAssetCopyError(File targetRootFolder, IOException error) {
        Log.e("setupFiles", "Could not refresh bundled assets", error);
        hideSetupProgress();
        runOnUiThread(() -> new AlertDialog.Builder(this)
                .setTitle("Asset setup failed")
                .setMessage("The bundled assets could not be copied. Check available storage and permissions, then retry. Your ROMs and saves have not been removed.")
                .setCancelable(false)
                .setPositiveButton("Retry", (dialog, which) -> romImportExecutor
                        .execute(() -> setupFilesInBackground(targetRootFolder)))
                .show());
    }




    private native void nativeHandleSelectedFile(String filePath);
    private native void nativeDialogResult(int result);

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == FILE_PICKER_REQUEST_CODE && resultCode == RESULT_OK && data != null) {
            Uri selectedFileUri = data.getData();
            if (selectedFileUri != null) {
                Toast.makeText(this, "Checking selected ROM...", Toast.LENGTH_SHORT).show();
                importSelectedRom(selectedFileUri);
            } else {
                nativeHandleSelectedFile(null);
            }

        } else if (requestCode == STORAGE_PERMISSION_REQUEST_CODE) {
            // Handle MANAGE_EXTERNAL_STORAGE result (may arrive before the user decided, see
            // startSetupIfPermitted; onResume checks again).
            startSetupIfPermitted();
        }
    }

    public void showAlertDialog(String title, String message, String btn1, String btn2) {
        runOnUiThread(() -> {
            AlertDialog.Builder builder = new AlertDialog.Builder(this)
                    .setTitle(title)
                    .setMessage(message)
                    .setCancelable(false)
                    .setPositiveButton(btn1, (dialog, which) -> nativeDialogResult(0));
            if (btn2 != null && !btn2.isEmpty()) {
                builder.setNegativeButton(btn2, (dialog, which) -> nativeDialogResult(1));
            }
            AlertDialog dialog = builder.create();
            // FLAG_NOT_FOCUSABLE prevents stealing SDL window focus so onWindowFocusChanged
            // is never called — SDL never fires FOCUS_LOST, ImGui keeps gamepad state intact,
            // SELECT/BACK menu toggle continues to work after dialog dismissal.
            dialog.getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
            dialog.show();
        });
    }

    public void openFilePicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {
                "application/octet-stream",
                "application/zip",
                "application/x-7z-compressed"
        });
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        runOnUiThread(() -> startActivityForResult(intent, 0));
    }

    private void importSelectedRom(Uri selectedFileUri) {
        romImportExecutor.execute(() -> {
            File importDirectory = new File(getCacheDir(), "rom-import");
            File selectedFile = null;
            File candidate = null;
            File pending = null;
            try {
                ensureDirectory(importDirectory);
                selectedFile = materializeSelection(selectedFileUri, importDirectory);
                Set<String> supportedHashes;
                try (InputStream hashes = getAssets().open("supportedHashes.json")) {
                    supportedHashes = SupportedRomHashes.load(hashes);
                }

                RomImportService service = new RomImportService(importDirectory);
                RomImportService.Result result = service.importFile(
                        selectedFile, queryDisplayName(selectedFileUri), supportedHashes);
                if (!result.isSuccess()) {
                    finishRomImport(null, messageFor(result.getError()));
                    return;
                }

                candidate = result.getCandidate();
                File destinationDirectory = new File(Environment.getExternalStorageDirectory(), "SOH");
                ensureDirectory(destinationDirectory);
                File destination = new File(destinationDirectory, "OOT.z64");
                pending = new File(destinationDirectory, "OOT.z64.pending");
                deleteQuietly(pending);
                copyAndSync(candidate, pending);
                Os.rename(pending.getAbsolutePath(), destination.getAbsolutePath());
                pending = null;
                finishRomImport(destination.getPath(), "Supported ROM imported");
            } catch (IOException | ErrnoException e) {
                finishRomImport(null, "Could not import the selected ROM");
            } finally {
                deleteQuietly(pending);
                deleteQuietly(candidate);
                deleteQuietly(selectedFile);
                deleteDirectoryIfEmpty(importDirectory);
            }
        });
    }

    private File materializeSelection(Uri uri, File importDirectory) throws IOException {
        File selected = File.createTempFile("selected-", ".input", importDirectory);
        long copied = 0;
        try (InputStream input = getContentResolver().openInputStream(uri);
             OutputStream output = new FileOutputStream(selected)) {
            if (input == null) {
                throw new IOException("Selected document is unavailable");
            }
            byte[] buffer = new byte[COPY_BUFFER_SIZE];
            for (int read; (read = input.read(buffer)) != -1;) {
                copied += read;
                if (copied > RomImportService.MAX_ARCHIVE_BYTES) {
                    throw new IOException("Selected document is too large");
                }
                output.write(buffer, 0, read);
            }
        } catch (IOException e) {
            deleteQuietly(selected);
            throw e;
        }
        return selected;
    }

    private String queryDisplayName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(
                uri, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (index >= 0) {
                    String value = cursor.getString(index);
                    if (value != null && !value.isEmpty()) {
                        return value;
                    }
                }
            }
        } catch (RuntimeException ignored) {
        }
        String fallback = uri.getLastPathSegment();
        return fallback == null ? "selection" : fallback;
    }

    private static void copyAndSync(File source, File destination) throws IOException {
        try (InputStream input = new java.io.FileInputStream(source);
             FileOutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[COPY_BUFFER_SIZE];
            for (int read; (read = input.read(buffer)) != -1;) {
                output.write(buffer, 0, read);
            }
            output.flush();
            output.getFD().sync();
        }
    }

    private void finishRomImport(String path, String message) {
        runOnUiThread(() -> {
            Toast.makeText(this, message, Toast.LENGTH_LONG).show();
            nativeHandleSelectedFile(path);
        });
    }

    private static String messageFor(RomImportService.Error error) {
        switch (error) {
            case NO_SUPPORTED_ROM:
                return "No supported .z64 ROM was found";
            case MULTIPLE_SUPPORTED_ROMS:
                return "The archive contains multiple supported ROMs";
            case ENCRYPTED_ARCHIVE:
                return "Encrypted archives are not supported";
            case UNSAFE_ARCHIVE:
                return "The selected archive exceeds safe import limits";
            case UNSUPPORTED_INPUT:
                return "Select a .z64, .zip, or .7z file";
            default:
                return "Could not read the selected ROM";
        }
    }

    private static void ensureDirectory(File directory) throws IOException {
        if ((!directory.isDirectory() && !directory.mkdirs()) || !directory.isDirectory()) {
            throw new IOException("Required directory is unavailable");
        }
    }

    private static void deleteQuietly(File file) {
        if (file != null && file.exists()) {
            file.delete();
        }
    }

    private static void deleteDirectoryIfEmpty(File directory) {
        if (directory == null || !directory.isDirectory()) {
            return;
        }
        File[] children = directory.listFiles();
        if (children != null && children.length == 0) {
            directory.delete();
        }
    }

    // Check if external storage is available and writable
    private boolean isExternalStorageWritable() {
        String state = Environment.getExternalStorageState();
        return Environment.MEDIA_MOUNTED.equals(state);
    }

    public native void attachController();
    public native void detachController();
    // Native method for setting button state
    public native void setButton(int button, boolean value);
    public native void setCameraState(int axis, float value);
    private native void setItemButtonPulse();
    private native void setItemButtonHeld(boolean held);

    // Native method for setting joystick axis value
    public native void setAxis(int axis, short value);

    // Signals C++ that the gamepad BACK/SELECT button was pressed, bypassing SDL.
    public native void nativeGamepadBackPressed();
    // Injects a directional menu nav key: dir 0=up 1=down 2=left 3=right.
    public native void nativeMenuNavKey(int dir, boolean pressed);
    public native void nativeSecondaryDisplaySurfaceChanged(Surface surface, int width, int height);
    public native void nativeSecondaryDisplaySurfaceDestroyed();
    // Touch on the secondary display (bottom screen trackers). action = MotionEvent.getActionMasked().
    public native void nativeSecondaryTouch(int action, float x, float y);

    public void SetFirstPersonAimingActive(boolean active) {
        mIsAiming = active;
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (!setupStarted && permissionAsked) {
            startSetupIfPermitted();
        }
        if (dualScreenManager != null) {
            dualScreenManager.start();
        }
        ImmersiveMode.apply(getWindow());
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            ImmersiveMode.apply(getWindow());
        }
    }

    @Override
    protected void onPause() {
        if (dualScreenManager != null) {
            dualScreenManager.stop();
        }
        setItemButtonHeld(false);
        mIsAiming = false;
        super.onPause();
    }

    // Thor: front-ends (Cocoon and others) can start the game on the bottom screen, where it
    // shows nothing usable (the trackers presentation covers it). If so, ask for MainActivity again
    // on the main display: Android moves the SAME task (singleInstance) without restarting it
    // (configChanges covers the screen change). The bottom-screen trackers always go to the
    // non-default display, so they end up on the bottom screen. Same fix as the Majora port.
    // Called before any initialisation (SDL, files).
    // Game already running (back to the launcher without quitting) and started again by a
    // front-end on the bottom screen: Android moves the existing task there without onCreate.
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        getWindow().getDecorView().post(this::moveTaskToMainDisplayIfNeeded);
    }

    @Override
    public void onConfigurationChanged(android.content.res.Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        getWindow().getDecorView().post(this::moveTaskToMainDisplayIfNeeded);
    }

    private int moveToMainDisplayTries;

    private void moveTaskToMainDisplayIfNeeded() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }
        int displayId = android.view.Display.DEFAULT_DISPLAY;
        try {
            android.view.Display display = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                    ? getDisplay() : getWindowManager().getDefaultDisplay();
            if (display != null) {
                displayId = display.getDisplayId();
            }
        } catch (Exception e) {
            Log.w("ThorLaunch", "unknown launch display", e);
        }
        if (displayId == android.view.Display.DEFAULT_DISPLAY) {
            moveToMainDisplayTries = 0;
            return;
        }
        // If Android refuses the move (virtual display, desktop mode...), don't retry forever.
        if (isFinishing() || ++moveToMainDisplayTries > 3) {
            return;
        }
        Log.i("ThorLaunch", "started on display " + displayId + ": moving task " + getTaskId()
                + " to the main display");
        Intent intent = new Intent(this, MainActivity.class);
        intent.setAction(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            startActivity(intent, android.app.ActivityOptions.makeBasic()
                    .setLaunchDisplayId(android.view.Display.DEFAULT_DISPLAY).toBundle());
        } catch (Exception e) {
            Log.w("ThorLaunch", "move to the main display refused", e);
        }
    }

    // Thor: "Quit" in the game menu must really close the game, not leave it
    // running in the background behind the launcher (Cocoon).
    @Override
    public void finish() {
        super.finishAndRemoveTask();
    }

    @Override
    protected void onDestroy() {
        if (dualScreenManager != null) {
            dualScreenManager.stop();
        }
        romImportExecutor.shutdown();
        boolean quitting = isFinishing();
        super.onDestroy();
        if (quitting) {
            android.os.Process.killProcess(android.os.Process.myPid());
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
            int keyCode = event.getKeyCode();
            // SDL loses controller tracking after activity transitions; catch BACK at the activity level.
            boolean isGamepad = (event.getSource() & android.view.InputDevice.SOURCE_GAMEPAD) != 0
                             || (event.getSource() & android.view.InputDevice.SOURCE_JOYSTICK) != 0;
            if (keyCode == KeyEvent.KEYCODE_BUTTON_SELECT ||
                    (keyCode == KeyEvent.KEYCODE_BACK && isGamepad)) {
                nativeGamepadBackPressed();
            }
        }
        return super.dispatchKeyEvent(event);
    }

    private Button button1, button2, button3, button4;
    private Button buttonA, buttonB, buttonX, buttonY;
    private Button buttonDpadUp, buttonDpadDown, buttonDpadLeft, buttonDpadRight;
    private Button buttonLB, buttonRB, buttonZ, buttonStart, buttonBack;
    private Button buttonToggle;
    private FrameLayout leftJoystick;
    private ImageView leftJoystickKnob;
    private View overlayView;

    // Function to set up the controller overlay (inflate layout and initialize buttons)
    private void setupControllerOverlay() {
        // Inflate the touchcontrol_overlay layout
        LayoutInflater inflater = (LayoutInflater) getSystemService(LAYOUT_INFLATER_SERVICE);
        overlayView = inflater.inflate(R.layout.touchcontrol_overlay, null);

        // Set layout params for overlayView to control positioning and sizing
        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
        );
        overlayView.setLayoutParams(layoutParams);
        final ViewGroup buttonGroup = overlayView.findViewById(R.id.button_group);

        buttonA = overlayView.findViewById(R.id.buttonA);
        buttonB = overlayView.findViewById(R.id.buttonB);
        buttonX = overlayView.findViewById(R.id.buttonX);
        buttonY = overlayView.findViewById(R.id.buttonY);

        buttonDpadUp = overlayView.findViewById(R.id.buttonDpadUp);
        buttonDpadDown = overlayView.findViewById(R.id.buttonDpadDown);
        buttonDpadLeft = overlayView.findViewById(R.id.buttonDpadLeft);
        buttonDpadRight = overlayView.findViewById(R.id.buttonDpadRight);

        buttonLB = overlayView.findViewById(R.id.buttonLB);
        buttonRB = overlayView.findViewById(R.id.buttonRB);
        buttonZ = overlayView.findViewById(R.id.buttonZ);

        buttonStart = overlayView.findViewById(R.id.buttonStart);
        buttonBack = overlayView.findViewById(R.id.buttonBack);

        buttonToggle = overlayView.findViewById(R.id.buttonToggle);

        // Initialize joysticks and joystick knobs from the inflated layout
        leftJoystick = overlayView.findViewById(R.id.left_joystick);
        leftJoystickKnob = overlayView.findViewById(R.id.left_joystick_knob);

        FrameLayout rightScreenArea = overlayView.findViewById(R.id.right_screen_area);

        addTouchListener(buttonA, ControllerButtons.BUTTON_A);
        addTouchListener(buttonB, ControllerButtons.BUTTON_B);
        addTouchListener(buttonX, ControllerButtons.BUTTON_X);
        addTouchListener(buttonY, ControllerButtons.BUTTON_Y);

        setupCButtons(buttonDpadUp, ControllerButtons.AXIS_RY, 1, 0);
        setupCButtons(buttonDpadDown, ControllerButtons.AXIS_RY, -1, 1);
        setupCButtons(buttonDpadLeft, ControllerButtons.AXIS_RX, 1, 2);
        setupCButtons(buttonDpadRight, ControllerButtons.AXIS_RX, -1, 3);

        addTouchListener(buttonLB, ControllerButtons.BUTTON_LB);
        addTouchListener(buttonRB, ControllerButtons.BUTTON_RB);
        addTouchListener(buttonZ, ControllerButtons.AXIS_RT);

        addTouchListener(buttonStart, ControllerButtons.BUTTON_START);
        // BACK uses nativeGamepadBackPressed directly; setButton(BUTTON_BACK) feeds ImGuiKey_GamepadBack, unreliable when SDL Gamepads list is empty.
        buttonBack.setOnTouchListener((v, event) -> {
            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                nativeGamepadBackPressed();
            }
            return true;
        });


        setupJoystick(leftJoystick, leftJoystickKnob, true);

        setupLookAround(rightScreenArea);

        setupToggleButton(buttonToggle,buttonGroup);

        // Exclude Back/Start from gesture nav zones (they sit at screen edges in landscape).
        // Must be called on each button in its own local coordinates.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ViewTreeObserver.OnGlobalLayoutListener gestureListener = new ViewTreeObserver.OnGlobalLayoutListener() {
                @Override
                public void onGlobalLayout() {
                    overlayView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                    Rect selfRect = new Rect(0, 0, buttonBack.getWidth(), buttonBack.getHeight());
                    buttonBack.setSystemGestureExclusionRects(Arrays.asList(selfRect));
                    selfRect = new Rect(0, 0, buttonStart.getWidth(), buttonStart.getHeight());
                    buttonStart.setSystemGestureExclusionRects(Arrays.asList(selfRect));
                }
            };
            overlayView.getViewTreeObserver().addOnGlobalLayoutListener(gestureListener);
        }

    }

    void attachControllerOverlay(ViewGroup host) {
        if (overlayView == null || host == null || overlayView.getParent() == host) {
            return;
        }
        if (overlayView.getParent() instanceof ViewGroup) {
            ((ViewGroup) overlayView.getParent()).removeView(overlayView);
        }
        host.addView(overlayView);
        host.setKeepScreenOn(true);
        overlayView.requestLayout();
    }

    void attachControllerOverlayToPrimary() {
        attachControllerOverlay((ViewGroup) findViewById(android.R.id.content));
    }

    private void setupToggleButton(Button button, ViewGroup uiGroup){
        // Thor build: the console has its own buttons and the overlay would sit on the bottom-screen
        // trackers, so touch controls start hidden on a new install ("controlsVisible" stores "hidden").
        boolean isHidden = preferences.getBoolean("controlsVisible", true);
        uiGroup.setVisibility(isHidden ? View.INVISIBLE : View.VISIBLE);
        if (isHidden) {
            DisableTouchArea();
            overlayView.setOnTouchListener(null);
        } else {
            EnableTouchArea();
            overlayView.setOnTouchListener((view, e) -> true);
        }
        boolean toggleVisible = preferences.getBoolean("toggleButtonVisible", false); // Eye hidden by default (Settings > Controls > Touch Controls)
        button.setVisibility(toggleVisible ? View.VISIBLE : View.GONE);
        button.setOnClickListener(v -> {
            boolean currentlyHidden = uiGroup.getVisibility() != View.VISIBLE;
            if (currentlyHidden) {
                uiGroup.setVisibility(View.VISIBLE);
                EnableTouchArea();
                overlayView.setOnTouchListener((view, e) -> true);
            } else {
                uiGroup.setVisibility(View.INVISIBLE);
                DisableTouchArea();
                overlayView.setOnTouchListener(null);
            }
            preferences.edit().putBoolean("controlsVisible", !currentlyHidden).apply();
        });
    }

    // Function to set a touch listener for each button
    private void addTouchListener(Button button, int buttonNum) {
        // dir>=4: face button nav keys (4=A/select, 5=B/back). -1 = no nav injection.
        int navDir = (buttonNum == ControllerButtons.BUTTON_A) ? 4
                   : (buttonNum == ControllerButtons.BUTTON_B) ? 5 : -1;
        button.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        setButton(buttonNum, true);
                        if (navDir >= 0) nativeMenuNavKey(navDir, true);
                        button.setPressed(true);
                        return true;
                    case MotionEvent.ACTION_UP:
                        setButton(buttonNum, false);
                        if (navDir >= 0) nativeMenuNavKey(navDir, false);
                        button.setPressed(false);
                        return true;
                    case MotionEvent.ACTION_CANCEL:
                        setButton(buttonNum, false);
                        if (navDir >= 0) nativeMenuNavKey(navDir, false);
                        return true;
                }
                return false;
            }
        });
    }

    private void setupCButtons(Button button, int axis, int direction, int navDir) {
        button.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        setAxis(axis, direction < 0 ? Short.MAX_VALUE : Short.MIN_VALUE);
                        nativeMenuNavKey(navDir, true);
                        button.setPressed(true);
                        return true;
                    case MotionEvent.ACTION_UP:
                        setAxis(axis, (short) 0);
                        nativeMenuNavKey(navDir, false);
                        button.setPressed(false);
                        return true;
                    case MotionEvent.ACTION_CANCEL:
                        setAxis(axis, (short) 0);
                        nativeMenuNavKey(navDir, false);
                        button.setPressed(false);
                        return true;
                }
                return false;
            }
        });
    }

    boolean TouchAreaEnabled = true;

    void DisableTouchArea(){
        TouchAreaEnabled = false;
    }
    void EnableTouchArea(){
        TouchAreaEnabled = true;
    }

    void SetToggleButtonVisible(boolean visible) {
        runOnUiThread(() -> {
            if (buttonToggle != null) {
                buttonToggle.setVisibility(visible ? View.VISIBLE : View.GONE);
            }
            preferences.edit().putBoolean("toggleButtonVisible", visible).apply();
        });
    }

    private void setupLookAround(FrameLayout rightScreenArea) {
        rightScreenArea.setOnTouchListener(new View.OnTouchListener() {
            private float lastX = 0;
            private float lastY = 0;
            private boolean isTouching = false;

            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        // Start tracking the finger's position
                        lastX = event.getX();
                        lastY = event.getY();
                        isTouching = true;
                        if (mIsAiming && TouchAreaEnabled) {
                            setItemButtonPulse();
                            setItemButtonHeld(true);
                        }
                        break;

                    case MotionEvent.ACTION_MOVE:
                        if (isTouching) {
                            // Calculate the change in position (delta)
                            float deltaX = event.getX() - lastX;
                            float deltaY = event.getY() - lastY;

                            // Update the last position
                            lastX = event.getX();
                            lastY = event.getY();

                            // Increase sensitivity by using a larger multiplier
                            // Adjust these multipliers to suit your needs
                            float sensitivityMultiplier = 15; // Higher value for more sensitivity
                            float rx = (deltaX * sensitivityMultiplier);
                            float ry = (deltaY * sensitivityMultiplier);

                            // Send the mapped values to the joystick axes
                            setCameraState(0, rx); // Right stick X axis
                            setCameraState(1, ry); // Right stick Y axis
                        }
                        break;

                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        // Stop tracking the finger's position and reset joystick input
                        isTouching = false;
                        setCameraState(0, 0.0f); // Reset right stick X axis
                        setCameraState(1, 0.0f); // Reset right stick Y axis
                        if (mIsAiming && TouchAreaEnabled) {
                            setItemButtonHeld(false);
                        }
                        break;
                }
                return TouchAreaEnabled; // Event full handled
            }
        });
    }





    // Function to set joystick movement with reset to center when not touched
    private void setupJoystick(FrameLayout joystickLayout, ImageView joystickKnob, boolean isLeft) {
        joystickLayout.post(() -> {
            // Calculate the joystick center once, before any events
            final float joystickCenterX = joystickLayout.getWidth() / 2f;
            final float joystickCenterY = joystickLayout.getHeight() / 2f;

            joystickLayout.setOnTouchListener(new View.OnTouchListener() {
                @Override
                public boolean onTouch(View v, MotionEvent event) {
                    switch (event.getAction()) {
                        case MotionEvent.ACTION_DOWN:
                        case MotionEvent.ACTION_MOVE:
                            // Calculate the joystick movement and move the knob
                            float deltaX = event.getX() - joystickCenterX;
                            float deltaY = event.getY() - joystickCenterY;

                            // Clamp the joystick movement to prevent it from going outside the area
                            float maxRadius = joystickLayout.getWidth() / 2f - joystickKnob.getWidth() / 2f;
                            float distance = (float) Math.sqrt(deltaX * deltaX + deltaY * deltaY);
                            if (distance > maxRadius) {
                                float scale = maxRadius / distance;
                                deltaX *= scale;
                                deltaY *= scale;
                            }

                            joystickKnob.setX(joystickCenterX + deltaX - joystickKnob.getWidth() / 2f);
                            joystickKnob.setY(joystickCenterY + deltaY - joystickKnob.getHeight() / 2f);

                            // Send joystick values to native C code
                            short x = (short) (deltaX / maxRadius * Short.MAX_VALUE);
                            short y = (short) (deltaY / maxRadius * Short.MAX_VALUE);

                            // Send X-axis and Y-axis values
                            setAxis(isLeft ? ControllerButtons.AXIS_LX : ControllerButtons.AXIS_RX, x); // X-axis
                            setAxis(isLeft ? ControllerButtons.AXIS_LY : ControllerButtons.AXIS_RY, y); // Y-axis
                            break;

                        case MotionEvent.ACTION_UP:
                        case MotionEvent.ACTION_CANCEL:
                            // Reset joystick knob to the center position (ensure it's placed correctly)
                            joystickKnob.setX(joystickCenterX - joystickKnob.getWidth() / 2f);
                            joystickKnob.setY(joystickCenterY - joystickKnob.getHeight() / 2f);

                            // Reset joystick values to 0 when released or canceled
                            setAxis(isLeft ? ControllerButtons.AXIS_LX : ControllerButtons.AXIS_RX, (short) 0); // X-axis
                            setAxis(isLeft ? ControllerButtons.AXIS_LY : ControllerButtons.AXIS_RY, (short) 0); // Y-axis
                            break;
                    }
                    return true;
                }
            });
        });


    }

    public void startRumble(int lowIntensity, int highIntensity) {
        int amplitude = Math.max(lowIntensity, highIntensity);
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice device = InputDevice.getDevice(id);
            if (device == null) continue;
            int sources = device.getSources();
            if ((sources & InputDevice.SOURCE_GAMEPAD) != InputDevice.SOURCE_GAMEPAD &&
                (sources & InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK) continue;
            Vibrator dv = device.getVibrator();
            if (dv != null && dv.hasVibrator()) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    dv.vibrate(VibrationEffect.createOneShot(RUMBLE_MAX_DURATION_MS, amplitude > 0 ? amplitude : VibrationEffect.DEFAULT_AMPLITUDE));
                } else {
                    dv.vibrate(RUMBLE_MAX_DURATION_MS);
                }
                return;
            }
        }
        Vibrator sv = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        if (sv != null && sv.hasVibrator()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                sv.vibrate(VibrationEffect.createOneShot(RUMBLE_MAX_DURATION_MS, amplitude > 0 ? amplitude : VibrationEffect.DEFAULT_AMPLITUDE));
            } else {
                sv.vibrate(RUMBLE_MAX_DURATION_MS);
            }
        }
    }

    public void stopRumble() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice device = InputDevice.getDevice(id);
            if (device == null) continue;
            int sources = device.getSources();
            if ((sources & InputDevice.SOURCE_GAMEPAD) != InputDevice.SOURCE_GAMEPAD &&
                (sources & InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK) continue;
            Vibrator dv = device.getVibrator();
            if (dv != null && dv.hasVibrator()) dv.cancel();
        }
        Vibrator sv = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        if (sv != null) sv.cancel();
    }

}
