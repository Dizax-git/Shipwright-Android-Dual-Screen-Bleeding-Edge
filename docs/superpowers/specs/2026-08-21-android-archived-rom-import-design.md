# Android Archived ROM Import Design

## Goal

Allow the Android app to accept an Ocarina of Time base ROM directly as a `.z64`, `.zip`, or `.7z` selection. For archives, the importer automatically finds the supported `.z64` entry without requiring the user to extract the archive.

## Required behavior

- Preserve direct `.z64` import.
- Treat `.zip` and `.7z` archives as read-only inputs.
- Scan `.z64` archive entries case-insensitively, including entries in subdirectories.
- Calculate SHA-1 while streaming each candidate and compare it with the canonical hashes in `docs/supportedHashes.json`.
- Continue scanning after a match so the importer can prove there is exactly one supported candidate.
- Accept the archive only when exactly one `.z64` entry matches a supported hash.
- Ignore non-`.z64` entries and unsupported `.z64` entries.
- Preserve the existing installed ROM and notify native code of failure when there are zero matches, multiple matches, a corrupt or unsupported archive, or an I/O failure.
- Never modify the selected archive or embed a ROM in the APK or repository.

## Architecture

Add a plain-Java `RomImportService` behind `MainActivity`'s document-picker callback. The activity remains responsible for Android UI and URI access; the service owns format detection, bounded archive traversal, candidate hashing, temporary-file lifecycle, and atomic promotion.

Use Apache Commons Compress for ZIP and 7z reading, plus XZ for Java for 7z's LZMA/LZMA2 support. Materialize the selected document into the app cache because 7z requires seekable input. Package a build-generated copy of `docs/supportedHashes.json` as an Android asset so the canonical upstream list is not manually duplicated.

The importer exposes one asynchronous operation returning either the verified destination path or a typed failure. `MainActivity` passes the successful path to `nativeHandleSelectedFile`; every failure passes `null` and shows a concise reason.

## Data flow

1. The native extractor opens Android's document picker for `.z64`, `.zip`, and `.7z` selections.
2. `MainActivity` submits the returned content URI to a background executor.
3. The importer identifies raw ROM, ZIP, or 7z input and reads the canonical supported SHA-1 set.
4. A raw `.z64` is streamed once into a pending file while hashing it.
5. An archive is copied into cache, then every `.z64` entry is streamed into an isolated candidate file while hashing it. Unmatched candidates are removed immediately. Scanning continues after the first supported match.
6. Exactly one supported candidate is copied to `SOH/OOT.z64.pending`, flushed, and atomically promoted to `SOH/OOT.z64`. The existing destination is not removed before successful promotion.
7. Temporary archive and candidate files are removed in all outcomes.
8. Native extraction continues with the verified `OOT.z64` path.

## Safety limits

- Reject encrypted archives because there is no password workflow.
- Reject malformed archives and unsupported compression methods.
- Reject candidate entries larger than 64 MiB or whose actual streamed bytes exceed their declared size.
- Reject a selected archive larger than 512 MiB.
- Apply a 128 MiB Commons Compress 7z memory limit.
- Reject a candidate whose observed compression ratio exceeds 200:1 after its first expanded MiB, or when the total expanded bytes across `.z64` candidates exceeds 512 MiB.
- Never construct an output path from an archive entry name; this prevents traversal through names such as `../OOT.z64`.
- Do not recursively inspect nested archives.
- Do not log ROM contents, hashes of rejected personal ROMs, or document-provider URIs.

## User experience

The picker accepts the three supported extensions. During import it displays a non-blocking progress indication. Success continues into the existing asset-generation flow. Failures distinguish no supported ROM, multiple supported ROMs, encrypted/unsupported archive, unsafe archive, and read/write failure without exposing private file details.

## Testing

Write tests before production code for:

- A supported raw `.z64`.
- A ZIP containing one supported `.z64`.
- A 7z containing one supported `.z64`.
- Multiple `.z64` entries with exactly one supported match.
- No supported match.
- Multiple supported matches.
- Uppercase `.Z64` and subdirectory entries.
- Corrupt, encrypted, oversized, high-ratio, and traversal-name archives.
- Preservation of an existing `OOT.z64` on every failure.
- Cleanup of every temporary file.
- Exact synchronization between the packaged hash data and `docs/supportedHashes.json`.

Run Android unit tests, the repository Android contract tests, a release build, APK verification, and a real-device import using an owned archive before publishing a signed release.

## Non-goals

- `.n64` or `.v64` entries inside archives.
- RAR or other archive formats.
- Password prompts, nested archives, or an archive-entry picker.
- Changing the supported ROM list independently of upstream `docs/supportedHashes.json`.
