// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.graphics.Bitmap;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for the navigation popup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NavigationPopupTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final int INVALID_NAVIGATION_INDEX = -1;

    private Profile mProfile;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking((Runnable) () -> mProfile = Profile.getLastUsedProfile());
    }

    // Exists solely to expose protected methods to this test.
    private static class TestNavigationHistory extends NavigationHistory {
        @Override
        public void addEntry(NavigationEntry entry) {
            super.addEntry(entry);
        }
    }

    // Exists solely to expose protected methods to this test.
    private static class TestNavigationEntry extends NavigationEntry {
        public TestNavigationEntry(int index, String url, String virtualUrl, String originalUrl,
                String title, Bitmap favicon, int transition) {
            super(index, url, virtualUrl, originalUrl, title, favicon, transition);
        }
    }

    private static class TestNavigationController implements NavigationController {
        private final TestNavigationHistory mHistory;
        private int mNavigatedIndex = INVALID_NAVIGATION_INDEX;

        public TestNavigationController() {
            mHistory = new TestNavigationHistory();
            mHistory.addEntry(new TestNavigationEntry(
                    1, "about:blank", null, null, "About Blank", null, 0));
            mHistory.addEntry(new TestNavigationEntry(
                    5, UrlUtils.encodeHtmlDataUri("<html>1</html>"), null, null, null, null, 0));
        }

        @Override
        public boolean canGoBack() {
            return false;
        }

        @Override
        public boolean canGoForward() {
            return false;
        }

        @Override
        public boolean canGoToOffset(int offset) {
            return false;
        }

        @Override
        public void goToOffset(int offset) {
        }

        @Override
        public void goBack() {
        }

        @Override
        public void goForward() {
        }

        @Override
        public boolean isInitialNavigation() {
            return false;
        }

        @Override
        public void loadIfNecessary() {
        }

        @Override
        public void requestRestoreLoad() {
        }

        @Override
        public void reload(boolean checkForRepost) {
        }

        @Override
        public void reloadBypassingCache(boolean checkForRepost) {
        }

        @Override
        public void cancelPendingReload() {
        }

        @Override
        public void continuePendingReload() {
        }

        @Override
        public void loadUrl(LoadUrlParams params) {
        }

        @Override
        public void clearHistory() {
        }

        @Override
        public NavigationHistory getNavigationHistory() {
            return null;
        }

        @Override
        public String getOriginalUrlForVisibleNavigationEntry() {
            return null;
        }

        @Override
        public void clearSslPreferences() {
        }

        @Override
        public boolean getUseDesktopUserAgent() {
            return false;
        }

        @Override
        public void setUseDesktopUserAgent(boolean override, boolean reloadOnChange) {
        }

        @Override
        public NavigationEntry getEntryAtIndex(int index) {
            return null;
        }

        @Override
        public NavigationEntry getPendingEntry() {
            return null;
        }

        @Override
        public NavigationHistory getDirectedNavigationHistory(boolean isForward, int itemLimit) {
            return mHistory;
        }

        @Override
        public void goToNavigationIndex(int index) {
            mNavigatedIndex = index;
        }

        @Override
        public int getLastCommittedEntryIndex() {
            return -1;
        }

        @Override
        public boolean removeEntryAtIndex(int index) {
            return false;
        }

        @Override
        public boolean canCopyStateOver() {
            return false;
        }

        @Override
        public boolean canPruneAllButLastCommitted() {
            return false;
        }

        @Override
        public void copyStateFrom(NavigationController source, boolean needsReload) {}

        @Override
        public void copyStateFromAndPrune(NavigationController source, boolean replaceEntry) {
        }

        @Override
        public String getEntryExtraData(int index, String key) {
            return null;
        }

        @Override
        public void setEntryExtraData(int index, String key, String value) {}
    }

    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testFaviconFetching() {
        final TestNavigationController controller = new TestNavigationController();
        final AtomicReference<NavigationPopup> popupReference = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            NavigationPopup popup = new NavigationPopup(
                    mProfile, mActivityTestRule.getActivity(), controller, true);
            popup.setWidth(300);
            popup.setHeight(300);
            popup.setAnchorView(mActivityTestRule.getActivity()
                    .getCurrentContentViewCore()
                    .getContainerView());

            popup.show();
            popupReference.set(popup);
        });

        CriteriaHelper.pollUiThread(new Criteria("All favicons did not get updated.") {
            @Override
            public boolean isSatisfied() {
                NavigationHistory history = controller.mHistory;
                for (int i = 0; i < history.getEntryCount(); i++) {
                    if (history.getEntryAtIndex(i).getFavicon() == null) {
                        return false;
                    }
                }
                return true;
            }
        });

        ThreadUtils.runOnUiThreadBlocking(() -> popupReference.get().dismiss());
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testItemSelection() {
        final TestNavigationController controller = new TestNavigationController();
        final AtomicReference<NavigationPopup> popupReference = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            NavigationPopup popup = new NavigationPopup(
                    mProfile, mActivityTestRule.getActivity(), controller, true);
            popup.setWidth(300);
            popup.setHeight(300);
            popup.setAnchorView(mActivityTestRule.getActivity()
                    .getCurrentContentViewCore()
                    .getContainerView());

            popup.show();
            popupReference.set(popup);
        });

        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> popupReference.get().performItemClick(1));

        Assert.assertFalse("Popup did not hide as expected.", popupReference.get().isShowing());
        Assert.assertEquals(
                "Popup attempted to navigate to the wrong index", 5, controller.mNavigatedIndex);
    }

}
