package com.dishii.soh;

import java.util.List;
import java.util.OptionalInt;

public final class SecondaryDisplaySelector {
    private SecondaryDisplaySelector() {
    }

    public static OptionalInt select(List<Candidate> candidates) {
        return candidates.stream()
                .filter(Candidate::isPresentationCapable)
                .filter(candidate -> !candidate.isDefaultDisplay())
                .mapToInt(Candidate::getDisplayId)
                .min();
    }

    public static final class Candidate {
        private final int displayId;
        private final boolean presentationCapable;
        private final boolean defaultDisplay;

        public Candidate(int displayId, boolean presentationCapable, boolean defaultDisplay) {
            this.displayId = displayId;
            this.presentationCapable = presentationCapable;
            this.defaultDisplay = defaultDisplay;
        }

        public int getDisplayId() {
            return displayId;
        }

        public boolean isPresentationCapable() {
            return presentationCapable;
        }

        public boolean isDefaultDisplay() {
            return defaultDisplay;
        }
    }
}
