package com.dishii.soh;

import static org.junit.Assert.assertEquals;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import org.junit.Test;

public class DualScreenManagerTest {
    @Test
    public void startIsIdempotent() {
        FakeDisplayBackend displays = new FakeDisplayBackend();
        displays.candidates = Collections.singletonList(candidate(4, true, false));
        FakePresentationHost presentation = new FakePresentationHost();
        FakeSurfaceNotifier notifier = new FakeSurfaceNotifier();
        DualScreenManager manager = new DualScreenManager(displays, presentation, notifier);

        manager.start();
        manager.start();

        assertEquals(1, displays.registerCount);
        assertEquals(Collections.singletonList(4), presentation.shownDisplayIds);
    }

    @Test
    public void stopUnregistersAndDestroysExactlyOnce() {
        FakeDisplayBackend displays = new FakeDisplayBackend();
        displays.candidates = Collections.singletonList(candidate(4, true, false));
        FakePresentationHost presentation = new FakePresentationHost();
        FakeSurfaceNotifier notifier = new FakeSurfaceNotifier();
        DualScreenManager manager = new DualScreenManager(displays, presentation, notifier);

        manager.start();
        manager.stop();
        manager.stop();

        assertEquals(1, displays.unregisterCount);
        assertEquals(1, presentation.dismissCount);
        assertEquals(1, notifier.destroyedCount);
    }

    @Test
    public void displayChangesSwitchToTheLowestEligibleDisplay() {
        FakeDisplayBackend displays = new FakeDisplayBackend();
        displays.candidates = Collections.singletonList(candidate(7, true, false));
        FakePresentationHost presentation = new FakePresentationHost();
        FakeSurfaceNotifier notifier = new FakeSurfaceNotifier();
        DualScreenManager manager = new DualScreenManager(displays, presentation, notifier);

        manager.start();
        displays.candidates = Arrays.asList(candidate(7, true, false), candidate(3, true, false));
        displays.listener.onDisplaysChanged();

        assertEquals(Arrays.asList(7, 3), presentation.shownDisplayIds);
        assertEquals(1, presentation.dismissCount);
        assertEquals(1, notifier.destroyedCount);
    }

    private static SecondaryDisplaySelector.Candidate candidate(
            int displayId, boolean presentationCapable, boolean defaultDisplay) {
        return new SecondaryDisplaySelector.Candidate(displayId, presentationCapable, defaultDisplay);
    }

    private static final class FakeDisplayBackend implements DualScreenManager.DisplayBackend {
        List<SecondaryDisplaySelector.Candidate> candidates = Collections.emptyList();
        DualScreenManager.DisplayChangeListener listener;
        int registerCount;
        int unregisterCount;

        @Override
        public void register(DualScreenManager.DisplayChangeListener listener) {
            this.listener = listener;
            registerCount++;
        }

        @Override
        public void unregister(DualScreenManager.DisplayChangeListener listener) {
            unregisterCount++;
        }

        @Override
        public List<SecondaryDisplaySelector.Candidate> getCandidates() {
            return candidates;
        }
    }

    private static final class FakePresentationHost implements DualScreenManager.PresentationHost {
        final List<Integer> shownDisplayIds = new ArrayList<>();
        int dismissCount;

        @Override
        public void show(int displayId) {
            shownDisplayIds.add(displayId);
        }

        @Override
        public void dismiss() {
            dismissCount++;
        }
    }

    private static final class FakeSurfaceNotifier implements DualScreenManager.SurfaceNotifier {
        int destroyedCount;

        @Override
        public void destroyed() {
            destroyedCount++;
        }
    }
}
