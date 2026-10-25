# Android Archived ROM Import Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the Android port import a supported Ocarina of Time `.z64` directly from a raw file, ZIP, or 7z archive without modifying the user's source archive.

**Architecture:** Keep Android URI and UI handling in `MainActivity`, and add a plain-Java `RomImportService` for bounded archive traversal, SHA-1 verification, temporary-file cleanup, and promotion. Package the canonical upstream `docs/supportedHashes.json` through a generated-assets Gradle task, use Apache Commons Compress for seekable ZIP/7z reads, and retain native ROM validation as a second gate.

**Tech Stack:** Java, Android Storage Access Framework, Apache Commons Compress 1.28.0, XZ for Java 1.12, JUnit 4, Gradle 8.10.2, PowerShell/Pester contract tests.

---

## File map

- Create `android/app/src/main/java/com/dishii/soh/SupportedRomHashes.java`: strict loader for canonical SHA-1 values.
- Create `android/app/src/main/java/com/dishii/soh/RomImportService.java`: format detection, scanning, validation, cleanup, and result model.
- Create `android/app/src/test/java/com/dishii/soh/SupportedRomHashesTest.java`: canonical hash parsing tests.
- Create `android/app/src/test/java/com/dishii/soh/RomImportServiceTest.java`: raw, ZIP, 7z, ambiguity, safety, preservation, and cleanup tests.
- Modify `android/app/src/main/java/com/dishii/soh/MainActivity.java`: asynchronous URI materialization, importer call, atomic destination promotion, and user messages.
- Modify `android/app/build.gradle`: dependencies and generated canonical hash asset.
- Modify `README.md`: direct archive selection instructions.
- Modify `docs/android/VALIDATION.md`: validation and release evidence.
- Modify `scripts/android/tests/verify-portable-includes.Tests.ps1`: source-contract assertions for the archive importer.

### Task 1: Canonical hash asset and parser

**Files:**
- Modify: `android/app/build.gradle`
- Create: `android/app/src/main/java/com/dishii/soh/SupportedRomHashes.java`
- Create: `android/app/src/test/java/com/dishii/soh/SupportedRomHashesTest.java`

- [ ] **Step 1: Write the failing parser tests**

Create tests that use an in-memory canonical fragment and assert normalization plus strict rejection:

```java
@Test
public void loadsLowercaseSha1Values() throws Exception {
    String json = "[{\"name\":\"US\",\"sha1\":\"AD69C91157F6705E8AB06C79FE08AAD47BB57BA7\"}]";
    assertEquals(Collections.singleton("ad69c91157f6705e8ab06c79fe08aad47bb57ba7"),
            SupportedRomHashes.load(bytes(json)));
}

@Test(expected = IOException.class)
public void rejectsDocumentsWithoutHashes() throws Exception {
    SupportedRomHashes.load(bytes("[]"));
}
```

- [ ] **Step 2: Run the focused test and verify RED**

Run from `android`:

```powershell
.\gradlew.bat :app:testReleaseUnitTest --tests com.dishii.soh.SupportedRomHashesTest
```

Expected: compilation fails because `SupportedRomHashes` does not exist.

- [ ] **Step 3: Add the canonical generated asset and dependencies**

Add to `android/app/build.gradle`:

```groovy
def generatedCanonicalAssets = layout.buildDirectory.dir('generated/canonicalAssets')

tasks.register('syncSupportedRomHashes', Copy) {
    from rootProject.file('../docs/supportedHashes.json')
    into generatedCanonicalAssets
}

android.sourceSets.main.assets.srcDir(generatedCanonicalAssets)
tasks.configureEach { task ->
    if (task.name.startsWith('merge') && task.name.endsWith('Assets')) {
        task.dependsOn('syncSupportedRomHashes')
    }
}
```

Add the pinned dependencies:

```groovy
implementation 'org.apache.commons:commons-compress:1.28.0'
implementation 'org.tukaani:xz:1.12'
```

- [ ] **Step 4: Implement the strict loader**

Implement `SupportedRomHashes.load(InputStream)` with a UTF-8 reader and the exact pattern `"sha1"\s*:\s*"([0-9a-fA-F]{40})"`. Normalize matches with `Locale.ROOT`, return an unmodifiable set, and throw `IOException` if the document contains no hashes or duplicate values.

```java
static Set<String> load(InputStream input) throws IOException {
    String json = readUtf8(input);
    Matcher matcher = SHA1_FIELD.matcher(json);
    Set<String> hashes = new LinkedHashSet<>();
    while (matcher.find()) {
        String hash = matcher.group(1).toLowerCase(Locale.ROOT);
        if (!hashes.add(hash)) throw new IOException("Duplicate supported ROM hash");
    }
    if (hashes.isEmpty()) throw new IOException("No supported ROM hashes found");
    return Collections.unmodifiableSet(hashes);
}
```

- [ ] **Step 5: Run tests and verify GREEN**

Run the focused test command again. Expected: `SupportedRomHashesTest` passes.

- [ ] **Step 6: Commit**

```powershell
git add android/app/build.gradle android/app/src/main/java/com/dishii/soh/SupportedRomHashes.java android/app/src/test/java/com/dishii/soh/SupportedRomHashesTest.java
git commit -m "feat(android): package supported ROM hashes"
```

### Task 2: Raw ROM import and failure preservation

**Files:**
- Create: `android/app/src/main/java/com/dishii/soh/RomImportService.java`
- Create: `android/app/src/test/java/com/dishii/soh/RomImportServiceTest.java`

- [ ] **Step 1: Write failing raw-import tests**

Build a deterministic small fixture whose SHA-1 is placed in the injected supported set. Assert successful import returns a pending file, unsupported input returns `NO_SUPPORTED_ROM`, and an existing destination marker remains unchanged on failure.

```java
@Test
public void supportedRawZ64ProducesPendingCandidate() throws Exception {
    File source = write("owned.z64", SUPPORTED_BYTES);
    RomImportService.Result result = service.importFile(source, "owned.z64", hashes(SUPPORTED_BYTES));
    assertTrue(result.isSuccess());
    assertArrayEquals(SUPPORTED_BYTES, Files.readAllBytes(result.getCandidate().toPath()));
}

@Test
public void unsupportedRawZ64DoesNotTouchDestination() throws Exception {
    File destination = write("OOT.z64", new byte[] { 9, 8, 7 });
    RomImportService.Result result = service.importFile(
            write("wrong.z64", new byte[] { 1 }), "wrong.z64", hashes(SUPPORTED_BYTES));
    assertEquals(RomImportService.Error.NO_SUPPORTED_ROM, result.getError());
    assertArrayEquals(new byte[] { 9, 8, 7 }, Files.readAllBytes(destination.toPath()));
}
```

- [ ] **Step 2: Run the focused test and verify RED**

```powershell
.\gradlew.bat :app:testReleaseUnitTest --tests com.dishii.soh.RomImportServiceTest
```

Expected: compilation fails because `RomImportService` does not exist.

- [ ] **Step 3: Implement raw import and typed results**

Add `Format { RAW_Z64, ZIP, SEVEN_Z }`, `Error { NONE, UNSUPPORTED_INPUT, NO_SUPPORTED_ROM, MULTIPLE_SUPPORTED_ROMS, ENCRYPTED_ARCHIVE, UNSAFE_ARCHIVE, IO_ERROR }`, and immutable `Result` fields. Detect the selected type by case-insensitive display-name suffix and validate ZIP/7z magic before archive parsing.

Stream raw input into a service-owned temporary candidate while computing SHA-1. Enforce `MAX_ROM_BYTES = 64L * 1024 * 1024`; delete the candidate unless its normalized digest belongs to the injected set.

```java
private Candidate copyAndHash(InputStream input, long declaredSize) throws IOException {
    MessageDigest sha1 = sha1();
    File candidate = File.createTempFile("rom-candidate-", ".z64", workDirectory);
    long written = 0;
    try (OutputStream output = new FileOutputStream(candidate)) {
        byte[] buffer = new byte[64 * 1024];
        for (int read; (read = input.read(buffer)) != -1;) {
            written += read;
            if (written > MAX_ROM_BYTES) throw new UnsafeArchiveException();
            sha1.update(buffer, 0, read);
            output.write(buffer, 0, read);
        }
    }
    return new Candidate(candidate, hex(sha1.digest()), written);
}
```

- [ ] **Step 4: Run tests and verify GREEN**

Run the focused test command. Expected: raw success, raw rejection, and preservation tests pass.

- [ ] **Step 5: Commit**

```powershell
git add android/app/src/main/java/com/dishii/soh/RomImportService.java android/app/src/test/java/com/dishii/soh/RomImportServiceTest.java
git commit -m "feat(android): validate raw ROM imports"
```

### Task 3: ZIP scanning and archive safety

**Files:**
- Modify: `android/app/src/main/java/com/dishii/soh/RomImportService.java`
- Modify: `android/app/src/test/java/com/dishii/soh/RomImportServiceTest.java`

- [ ] **Step 1: Write failing ZIP behavior tests**

Add real ZIP fixtures for one match, several `.z64` entries with one match, no match, duplicate supported matches, uppercase `.Z64` under a subdirectory, a traversal name, oversized declared/observed data, and a ratio above 200:1.

```java
@Test
public void zipIteratesUntilTheSingleSupportedMatch() throws Exception {
    File archive = zip(entries(
            "notes.txt", bytes("ignore"),
            "wrong.z64", bytes("wrong"),
            "roms/OOT.Z64", SUPPORTED_BYTES));
    RomImportService.Result result = service.importFile(archive, "owned.zip", hashes(SUPPORTED_BYTES));
    assertTrue(result.isSuccess());
    assertArrayEquals(SUPPORTED_BYTES, read(result.getCandidate()));
}

@Test
public void zipRejectsTwoSupportedEntries() throws Exception {
    File archive = zip(entries("one.z64", SUPPORTED_BYTES, "two.z64", SUPPORTED_BYTES));
    assertEquals(RomImportService.Error.MULTIPLE_SUPPORTED_ROMS,
            service.importFile(archive, "owned.zip", hashes(SUPPORTED_BYTES)).getError());
}
```

- [ ] **Step 2: Run tests and verify RED**

Run the focused test command. Expected: ZIP cases fail because archive scanning is not implemented.

- [ ] **Step 3: Implement bounded ZIP traversal**

Use Commons Compress `ZipFile` with seekable file access. Ignore directories and non-`.z64` entries. Reject any selected archive larger than 512 MiB, unsafe entry names, encrypted entries, candidate sizes above 64 MiB, cumulative candidate expansion above 512 MiB, and an observed ratio above 200:1 after 1 MiB. Keep only the first supported candidate, continue scanning, and return `MULTIPLE_SUPPORTED_ROMS` after a second match.

```java
for (Enumeration<ZipArchiveEntry> entries = zip.getEntries(); entries.hasMoreElements();) {
    ZipArchiveEntry entry = entries.nextElement();
    if (entry.isDirectory() || !isZ64(entry.getName())) continue;
    validateEntryName(entry.getName());
    Candidate candidate = copyAndHash(zip.getInputStream(entry), entry.getSize());
    expandedTotal = checkedExpandedTotal(expandedTotal, candidate.size);
    verifyRatio(candidate.size, entry.getCompressedSize());
    accepted = retainSingleSupported(candidate, accepted, supportedHashes);
}
```

- [ ] **Step 4: Run tests and verify GREEN**

Run the focused test command. Expected: all raw and ZIP tests pass and temporary directories are empty after failures.

- [ ] **Step 5: Commit**

```powershell
git add android/app/src/main/java/com/dishii/soh/RomImportService.java android/app/src/test/java/com/dishii/soh/RomImportServiceTest.java
git commit -m "feat(android): import supported ROMs from zip"
```

### Task 4: 7z scanning

**Files:**
- Modify: `android/app/src/main/java/com/dishii/soh/RomImportService.java`
- Modify: `android/app/src/test/java/com/dishii/soh/RomImportServiceTest.java`

- [ ] **Step 1: Write failing 7z tests**

Create fixtures with `SevenZOutputFile` for one supported candidate, several candidates with one match, duplicate supported matches, and unsafe/oversized entries. Assert parity with ZIP result codes and cleanup.

```java
@Test
public void sevenZFindsOneSupportedEntryAmongSeveralCandidates() throws Exception {
    File archive = sevenZ(entries("wrong.z64", bytes("wrong"), "folder/oot.z64", SUPPORTED_BYTES));
    RomImportService.Result result = service.importFile(archive, "owned.7z", hashes(SUPPORTED_BYTES));
    assertTrue(result.isSuccess());
    assertArrayEquals(SUPPORTED_BYTES, read(result.getCandidate()));
}
```

- [ ] **Step 2: Run tests and verify RED**

Run the focused test command. Expected: 7z cases fail because 7z scanning is not implemented.

- [ ] **Step 3: Implement bounded 7z traversal**

Open the materialized archive through `SevenZFile.builder()` with a 131072 KiB memory limit. Iterate sequentially, apply the same name, size, cumulative expansion, ratio, single-match, and cleanup rules as ZIP, and map password/encryption failures to `ENCRYPTED_ARCHIVE` without logging archive details.

```java
try (SevenZFile sevenZ = SevenZFile.builder()
        .setFile(source)
        .setMaxMemoryLimitKiB(SEVEN_Z_MEMORY_LIMIT_KIB)
        .get()) {
    for (SevenZArchiveEntry entry; (entry = sevenZ.getNextEntry()) != null;) {
        if (entry.isDirectory() || !isZ64(entry.getName())) continue;
        Candidate candidate = copyAndHash(new SevenZEntryInputStream(sevenZ), entry.getSize());
        accepted = retainSingleSupported(candidate, accepted, supportedHashes);
    }
}
```

- [ ] **Step 4: Run tests and verify GREEN**

Run the focused test command. Expected: raw, ZIP, and 7z tests pass.

- [ ] **Step 5: Commit**

```powershell
git add android/app/src/main/java/com/dishii/soh/RomImportService.java android/app/src/test/java/com/dishii/soh/RomImportServiceTest.java
git commit -m "feat(android): import supported ROMs from 7z"
```

### Task 5: Android picker integration and atomic promotion

**Files:**
- Modify: `android/app/src/main/java/com/dishii/soh/MainActivity.java`
- Modify: `scripts/android/tests/verify-portable-includes.Tests.ps1`

- [ ] **Step 1: Add failing source-contract assertions**

Assert that `MainActivity` advertises the supported MIME types, imports on a background executor, loads `supportedHashes.json`, uses `Os.rename` for same-filesystem replacement, calls `nativeHandleSelectedFile(null)` on failure, and no longer deletes `OOT.z64` before validation.

```powershell
$mainActivity | Should Match 'application/x-7z-compressed'
$mainActivity | Should Match 'supportedHashes\.json'
$mainActivity | Should Match 'Os\.rename'
$mainActivity | Should Not Match 'destinationFile\.delete\(\)'
```

- [ ] **Step 2: Run the contract test and verify RED**

```powershell
Invoke-Pester .\scripts\android\tests\verify-portable-includes.Tests.ps1 -Output Detailed
```

Expected: the new archived-ROM assertions fail.

- [ ] **Step 3: Integrate asynchronous import**

Resolve the display name through `OpenableColumns.DISPLAY_NAME`, materialize the content URI into `getCacheDir()/rom-import`, load the canonical asset, and call `RomImportService` from the executor. On success, copy the verified candidate to `SOH/OOT.z64.pending`, flush it, and call `android.system.Os.rename(pending, destination)` before notifying native code on the UI thread.

```java
private void importSelectedRom(Uri uri) {
    Executors.newSingleThreadExecutor().execute(() -> {
        RomImportService.Result result = importService.importFile(
                materializeSelection(uri), queryDisplayName(uri), loadSupportedRomHashes());
        if (!result.isSuccess()) {
            finishRomImport(null, messageFor(result.getError()));
            return;
        }
        File destination = promoteVerifiedRom(result.getCandidate());
        finishRomImport(destination.getPath(), "Supported ROM imported");
    });
}
```

The picker remains `ACTION_OPEN_DOCUMENT` and uses `Intent.EXTRA_MIME_TYPES` for `application/octet-stream`, `application/zip`, and `application/x-7z-compressed` while retaining `*/*` compatibility with providers that do not classify `.z64`.

- [ ] **Step 4: Run focused and full Android tests**

```powershell
Invoke-Pester .\scripts\android\tests\verify-portable-includes.Tests.ps1 -Output Detailed
Set-Location android
.\gradlew.bat :app:testReleaseUnitTest
```

Expected: Pester and all Java release unit tests pass.

- [ ] **Step 5: Commit**

```powershell
git add android/app/src/main/java/com/dishii/soh/MainActivity.java scripts/android/tests/verify-portable-includes.Tests.ps1
git commit -m "feat(android): accept archived ROM selections"
```

### Task 6: Documentation, release build, signing, publication, and device verification

**Files:**
- Modify: `README.md`
- Modify: `docs/android/VALIDATION.md`
- Modify: `android/app/build.gradle`

- [ ] **Step 1: Update setup documentation and monotonic version**

Change the Android setup instructions to state that `.z64`, `.zip`, and `.7z` may be selected directly and that archives must contain exactly one supported `.z64` match. Increment the local revision from `9.2.3-dual-screen.1` / `902031` to `9.2.3-dual-screen.2` / `902032`.

- [ ] **Step 2: Run all host gates**

From the repository root:

```powershell
Invoke-Pester .\scripts\android\tests\*.Tests.ps1 -Output Detailed
Set-Location android
.\gradlew.bat :app:testReleaseUnitTest :app:assembleRelease
```

Expected: all contract/unit tests pass and one unsigned arm64 release APK is created.

- [ ] **Step 3: Verify and persistently sign**

Run `scripts/android/verify-apk.ps1` against the unsigned APK with its unsigned-input expectations. Normalize it using policy `shipwright-dual-screen` and `-AllowUnsignedInput`, outputting only to a unique `D:\Temp` staging path. Run the signing worker's Verify operation against the normalized APK and require package `com.dishii.soh`, version `9.2.3-dual-screen.2`, versionCode `902032`, exactly `arm64-v8a`, 16 KiB alignment, signer `5db60116997ae5cf8ea0f26fee6c0a150a8045a7cbfc10282892ff10c0348004`, and no ROM/OTR/O2R/save/quest payload except project asset `assets/soh.o2r` when permitted by the repository verifier.

- [ ] **Step 4: Verify on AYN Thor**

If `adb -s bfa98654 get-state` reports a connected device and its installed signer is compatible, install with `adb -s bfa98654 install -r <signed-apk>`. Select an owned ZIP or 7z containing the supported `.z64`, verify successful O2R generation and gameplay launch on both displays, then verify the selected source archive is byte-identical and no temporary import files remain. If the signer differs, do not uninstall or erase data; report the one-time migration requirement.

- [ ] **Step 5: Commit validation evidence**

Record exact dependency versions, test counts, APK metadata, SHA-256, signer, payload/alignment results, and device-flow evidence in `docs/android/VALIDATION.md`, then commit:

```powershell
git add README.md docs/android/VALIDATION.md android/app/build.gradle
git commit -m "docs(android): validate archived ROM import"
```

- [ ] **Step 6: Push and publish the authorized release**

Push the verified branch commits to `bleeding-edge/main`, create tag `v9.2.3-dual-screen.2`, and publish one normal GitHub release containing the verified signed APK and `.sha256` sidecar. Do not upload ROMs, generated game archives, saves, signing credentials, logs, or temporary files.

- [ ] **Step 7: Round-trip verify and clean staging**

Download both release assets through GitHub, verify the sidecar, rerun the APK verifier and persistent signing-policy Verify operation, confirm the GitHub releases API exposes the APK to ObtainX, and remove only this run's validated `D:\Temp` or `E:\Temp` staging directory.
