# Android Dual-Display Immersive Mode Design

## Problem

The primary game surface is vertically inset because Android's status bar remains visible. The manifest requests fullscreen, but `SDLActivity` initially forces a non-fullscreen window and the dual-screen `Presentation` is shown later without restoring immersive mode on either window.

## Considered approaches

1. **Manifest-only fullscreen:** too weak because system-bar ownership changes during resume and window-focus transitions.
2. **Main-Activity-only repair:** fixes the initial display but leaves the secondary `Presentation` unmanaged.
3. **Shared two-window policy (selected):** one helper applies the same immersive flags and modern window-insets request to both windows, with lifecycle restoration when either window becomes active.

## Design

Add a package-private `ImmersiveMode` helper that makes a supplied `Window` fullscreen, lays content out through system-bar areas, hides status and navigation bars, and permits transient reveal by edge swipe. It uses `WindowInsetsControllerCompat` while retaining the legacy SDL flags needed by the current Android port.

`MainActivity` applies the policy after SDL setup, on resume, and when its window regains focus. `DualScreenHudPresentation` applies it when created, started, and focused. This keeps both displays independent and avoids relying on a one-time native SDL fullscreen callback.

## Verification

A source-contract regression test must fail before implementation and then prove both windows invoke the shared policy at the required lifecycle points. Android unit tests and Java compilation must pass. The release APK must retain package `com.dishii.soh`, arm64 ABI, persistent signer, alignment, and forbidden-payload checks. Device access is read-only unless the user grants broader permission later.
