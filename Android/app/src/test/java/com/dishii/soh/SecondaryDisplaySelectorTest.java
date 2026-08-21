package com.dishii.soh;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import java.util.Arrays;
import java.util.Collections;
import java.util.OptionalInt;
import org.junit.Test;

public class SecondaryDisplaySelectorTest {
    @Test
    public void emptyCandidateListReturnsNoSelection() {
        OptionalInt selected = SecondaryDisplaySelector.select(Collections.emptyList());

        assertFalse(selected.isPresent());
    }

    @Test
    public void ignoresTheDefaultDisplay() {
        OptionalInt selected = SecondaryDisplaySelector.select(Collections.singletonList(
                new SecondaryDisplaySelector.Candidate(0, true, true)));

        assertFalse(selected.isPresent());
    }

    @Test
    public void selectsPresentationCapableNonDefaultDisplay() {
        OptionalInt selected = SecondaryDisplaySelector.select(Arrays.asList(
                new SecondaryDisplaySelector.Candidate(2, false, false),
                new SecondaryDisplaySelector.Candidate(4, true, false)));

        assertEquals(4, selected.getAsInt());
    }

    @Test
    public void selectsLowestEligibleNonDefaultDisplayId() {
        OptionalInt selected = SecondaryDisplaySelector.select(Arrays.asList(
                new SecondaryDisplaySelector.Candidate(7, true, false),
                new SecondaryDisplaySelector.Candidate(0, true, true),
                new SecondaryDisplaySelector.Candidate(3, true, false)));

        assertEquals(3, selected.getAsInt());
    }
}
