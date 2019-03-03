// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.policy.test.annotations.Policies;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for {@link LocaleManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocaleManagerTest {
    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                try {
                    ChromeBrowserInitializer.getInstance(InstrumentationRegistry.getTargetContext())
                            .handleSynchronousStartup();
                } catch (ProcessInitException e) {
                    Assert.fail("Failed to load browser");
                }
            }
        });
    }

    @Policies.Add({ @Policies.Item(key = "DefaultSearchProviderEnabled", string = "false") })
    @SmallTest
    @Test
    public void testShowSearchEnginePromoDseDisabled()
            throws InterruptedException, TimeoutException {
        final CallbackHelper getShowTypeCallback = new CallbackHelper();
        LocaleManager.setInstanceForTest(new LocaleManager() {
            @Override
            public int getSearchEnginePromoShowType() {
                getShowTypeCallback.notifyCalled();
                return LocaleManager.SEARCH_ENGINE_PROMO_DONT_SHOW;
            }
        });

        // Launch any activity as an Activity ref is required to attempt to show the activity.
        final SearchActivity searchActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SearchActivity.class);

        final CallbackHelper searchEnginesFinalizedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                LocaleManager.getInstance().showSearchEnginePromoIfNeeded(
                        searchActivity, new Callback<Boolean>() {
                            @Override
                            public void onResult(Boolean result) {
                                Assert.assertTrue(result);
                                searchEnginesFinalizedCallback.notifyCalled();
                            }
                        });
            }
        });
        searchEnginesFinalizedCallback.waitForCallback(0);
        Assert.assertEquals(0, getShowTypeCallback.getCallCount());
    }
}
