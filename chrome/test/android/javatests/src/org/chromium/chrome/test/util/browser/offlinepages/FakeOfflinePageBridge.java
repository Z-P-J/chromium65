// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.offlinepages;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;

import java.util.ArrayList;
import java.util.List;

/**
 * For testing clients of OfflinePageBridge.
 */
public class FakeOfflinePageBridge extends OfflinePageBridge {
    private boolean mIsOfflinePageModelLoaded;
    private final List<OfflinePageItem> mItems = new ArrayList<>();

    public FakeOfflinePageBridge() {
        super(0);
    }

    public static OfflinePageItem createOfflinePageItem(String url, long offlineId) {
        return new OfflinePageItem(url, offlineId, "", "", "", "", 0, 0, 0, 0, "");
    }

    public void setIsOfflinePageModelLoaded(boolean isOfflinePageModelLoaded) {
        mIsOfflinePageModelLoaded = isOfflinePageModelLoaded;
    }

    @Override
    public boolean isOfflinePageModelLoaded() {
        return mIsOfflinePageModelLoaded;
    }

    public void setItems(List<OfflinePageItem> items) {
        mItems.clear();
        mItems.addAll(items);
    }

    @Override
    public void selectPageForOnlineUrl(
            String onlineUrl, int tabId, Callback<OfflinePageItem> callback) {
        OfflinePageItem result = null;
        for (OfflinePageItem item : mItems) {
            if (!item.getUrl().equals(onlineUrl)) continue;
            if (result == null || item.getCreationTimeMs() > result.getCreationTimeMs()) {
                result = item;
            }
        }
        callback.onResult(result);
    }

    public void fireOfflinePageModelLoaded() {
        super.offlinePageModelLoaded();
    }
}
