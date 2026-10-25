package com.dishii.soh;

import org.junit.Test;
import static org.junit.Assert.*;

public class BundledAssetsVersionTest {
    @Test public void firstInstallNeedsBundledAssets() {
        assertTrue(AssetCopyUtil.needsBundledAssetRefresh(903000001, 0, false));
    }
    @Test public void upgradeRefreshesExistingAssets() {
        assertTrue(AssetCopyUtil.needsBundledAssetRefresh(903000001, 902003005, true));
    }
    @Test public void unchangedVerifiedBundleDoesNotNeedRefresh() {
        assertFalse(AssetCopyUtil.needsBundledAssetRefresh(903000001, 903000001, true));
    }
    @Test public void missingTorchConfigurationForcesRepair() {
        assertTrue(AssetCopyUtil.needsBundledAssetRefresh(903000001, 903000001, false));
    }
}
