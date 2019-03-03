// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.StrictMode;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;

/**
 * Tests ensuring that starting up WebView does not cause any diskRead StrictMode violations.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwStrictModeTest {
    @Rule
    public AwActivityTestRule mActivityTestRule =
            new AwActivityTestRule() {
                @Override
                public boolean needsAwBrowserContextCreated() {
                    return false;
                }

                @Override
                public boolean needsBrowserProcessStarted() {
                    // Don't start the browser process in AwActivityTestRule - we want to start it
                    // ourselves with strictmode policies turned on.
                    return false;
                }
            }

    ;

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mAwTestContainerView;

    private StrictMode.ThreadPolicy mOldThreadPolicy;
    private StrictMode.VmPolicy mOldVmPolicy;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        enableStrictModeOnUiThreadSync();
    }

    @After
    public void tearDown() throws Exception {
        disableStrictModeOnUiThreadSync();
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testStartup() throws Exception {
        startEverythingSync();
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testLoadEmptyData() throws Exception {
        startEverythingSync();
        mActivityTestRule.loadDataSync(mAwTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), "", "text/html", false);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testSetJavaScriptAndLoadData() throws Exception {
        startEverythingSync();
        mActivityTestRule.enableJavaScriptOnUiThread(mAwTestContainerView.getAwContents());
        mActivityTestRule.loadDataSync(mAwTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(), "", "text/html", false);
    }

    private void enableStrictModeOnUiThreadSync() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            mOldThreadPolicy = StrictMode.getThreadPolicy();
            mOldVmPolicy = StrictMode.getVmPolicy();
            StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                    .detectAll()
                    .penaltyLog()
                    .penaltyDeath()
                    .build());
            StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                    .detectAll()
                    .penaltyLog()
                    .penaltyDeath()
                    .build());
        });
    }

    private void disableStrictModeOnUiThreadSync() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            StrictMode.setThreadPolicy(mOldThreadPolicy);
            StrictMode.setVmPolicy(mOldVmPolicy);
        });
    }

    private void startEverythingSync() throws Exception {
        mActivityTestRule.getActivity();
        mActivityTestRule.createAwBrowserContext();
        mActivityTestRule.startBrowserProcess();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> mAwTestContainerView =
                mActivityTestRule.createAwTestContainerView(mContentsClient));
    }
}
