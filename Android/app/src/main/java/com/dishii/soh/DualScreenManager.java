package com.dishii.soh;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.util.Log;
import android.view.Display;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.OptionalInt;

/** Owns the secondary-display listener and presentation lifecycle. */
public final class DualScreenManager {
    private static final String TAG = "DualScreenManager";

    interface DisplayChangeListener {
        void onDisplaysChanged();
    }

    interface DisplayBackend {
        void register(DisplayChangeListener listener);

        void unregister(DisplayChangeListener listener);

        List<SecondaryDisplaySelector.Candidate> getCandidates();
    }

    interface PresentationHost {
        void show(int displayId);

        void dismiss();
    }

    interface SurfaceNotifier {
        void destroyed();
    }

    private final DisplayBackend displays;
    private final PresentationHost presentation;
    private final SurfaceNotifier surfaceNotifier;
    private final DisplayChangeListener displayChangeListener = this::refreshPresentation;
    private boolean started;
    private OptionalInt selectedDisplayId = OptionalInt.empty();

    public DualScreenManager(MainActivity activity) {
        AndroidDisplayBackend displayBackend = new AndroidDisplayBackend(activity);
        AndroidPresentationHost presentationHost = new AndroidPresentationHost(activity, displayBackend);
        this.displays = displayBackend;
        this.presentation = presentationHost;
        this.surfaceNotifier = presentationHost;
    }

    DualScreenManager(
            DisplayBackend displays, PresentationHost presentation, SurfaceNotifier surfaceNotifier) {
        this.displays = displays;
        this.presentation = presentation;
        this.surfaceNotifier = surfaceNotifier;
    }

    public void start() {
        if (started) {
            return;
        }

        started = true;
        displays.register(displayChangeListener);
        refreshPresentation();
    }

    public void stop() {
        if (!started) {
            return;
        }

        started = false;
        displays.unregister(displayChangeListener);
        clearPresentation();
    }

    private void refreshPresentation() {
        if (!started) {
            return;
        }

        OptionalInt nextDisplayId = SecondaryDisplaySelector.select(displays.getCandidates());
        if (sameSelection(selectedDisplayId, nextDisplayId)) {
            return;
        }

        clearPresentation();
        if (!nextDisplayId.isPresent()) {
            return;
        }

        try {
            presentation.show(nextDisplayId.getAsInt());
            selectedDisplayId = nextDisplayId;
        } catch (RuntimeException exception) {
            Log.w(TAG, "Failed to show the secondary-display presentation", exception);
        }
    }

    private void clearPresentation() {
        if (!selectedDisplayId.isPresent()) {
            return;
        }

        surfaceNotifier.destroyed();
        presentation.dismiss();
        selectedDisplayId = OptionalInt.empty();
    }

    private static boolean sameSelection(OptionalInt left, OptionalInt right) {
        return left.isPresent() == right.isPresent()
                && (!left.isPresent() || left.getAsInt() == right.getAsInt());
    }

    private static final class AndroidDisplayBackend implements DisplayBackend {
        private final DisplayManager displayManager;
        private final List<ListenerRegistration> registrations = new ArrayList<>();

        AndroidDisplayBackend(MainActivity activity) {
            displayManager = (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
        }

        @Override
        public void register(DisplayChangeListener listener) {
            if (displayManager == null) {
                return;
            }

            DisplayManager.DisplayListener androidListener = new DisplayManager.DisplayListener() {
                @Override
                public void onDisplayAdded(int displayId) {
                    listener.onDisplaysChanged();
                }

                @Override
                public void onDisplayRemoved(int displayId) {
                    listener.onDisplaysChanged();
                }

                @Override
                public void onDisplayChanged(int displayId) {
                    listener.onDisplaysChanged();
                }
            };
            registrations.add(new ListenerRegistration(listener, androidListener));
            displayManager.registerDisplayListener(androidListener, null);
        }

        @Override
        public void unregister(DisplayChangeListener listener) {
            if (displayManager == null) {
                return;
            }

            for (int index = registrations.size() - 1; index >= 0; index--) {
                ListenerRegistration registration = registrations.get(index);
                if (registration.owner == listener) {
                    displayManager.unregisterDisplayListener(registration.androidListener);
                    registrations.remove(index);
                    return;
                }
            }
        }

        @Override
        public List<SecondaryDisplaySelector.Candidate> getCandidates() {
            if (displayManager == null) {
                return Collections.emptyList();
            }

            List<SecondaryDisplaySelector.Candidate> candidates = new ArrayList<>();
            for (Display display : displayManager.getDisplays()) {
                candidates.add(new SecondaryDisplaySelector.Candidate(
                        display.getDisplayId(),
                        (display.getFlags() & Display.FLAG_PRESENTATION) != 0,
                        display.getDisplayId() == Display.DEFAULT_DISPLAY));
            }
            return candidates;
        }

        Display getDisplay(int displayId) {
            return displayManager == null ? null : displayManager.getDisplay(displayId);
        }

        private static final class ListenerRegistration {
            final DisplayChangeListener owner;
            final DisplayManager.DisplayListener androidListener;

            ListenerRegistration(
                    DisplayChangeListener owner, DisplayManager.DisplayListener androidListener) {
                this.owner = owner;
                this.androidListener = androidListener;
            }
        }
    }

    private static final class AndroidPresentationHost implements PresentationHost, SurfaceNotifier {
        private final MainActivity activity;
        private final AndroidDisplayBackend displays;
        private DualScreenHudPresentation presentation;

        AndroidPresentationHost(MainActivity activity, AndroidDisplayBackend displays) {
            this.activity = activity;
            this.displays = displays;
        }

        @Override
        public void show(int displayId) {
            Display display = displays.getDisplay(displayId);
            if (display == null) {
                throw new IllegalStateException("Secondary display disappeared before presentation setup");
            }

            presentation = new DualScreenHudPresentation(activity, display);
            presentation.show();
        }

        @Override
        public void dismiss() {
            if (presentation == null) {
                return;
            }

            presentation.dismiss();
            presentation = null;
        }

        @Override
        public void destroyed() {
            if (presentation != null) {
                presentation.notifySurfaceDestroyedIfNeeded();
            }
        }
    }
}
