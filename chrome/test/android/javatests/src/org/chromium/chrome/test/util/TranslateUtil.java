// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Activity;
import android.app.Instrumentation;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarCompactLayout;
import org.chromium.chrome.browser.infobar.TranslateCompactInfoBar;
import org.chromium.chrome.browser.infobar.translate.TranslateMenu;
import org.chromium.chrome.browser.infobar.translate.TranslateTabLayout;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;

/**
 * Utility functions for dealing with Translate InfoBars.
 */
public class TranslateUtil {
    /**
     * Finds the first clickable span inside a TextView and clicks it.
     */
    public static void openLanguagePanel(
            Instrumentation instrumentation, Activity activity, InfoBar infoBar) {
        View view = infoBar.getView().findViewById(R.id.infobar_message);
        Assert.assertNotNull(view);

        TextView text = (TextView) view.findViewById(R.id.infobar_message);

        SpannableString spannable = (SpannableString) text.getText();
        ClickableSpan[] clickable =
            spannable.getSpans(0, spannable.length() - 1, ClickableSpan.class);
        Assert.assertTrue(clickable.length > 0);

        // Find the approximate coordinates of the first link of the first line of text so we can
        // click there. Clicking on any character of the link will work so instead of focusing on
        // the beginning of the link we add one more character so that finding a valid coordinate
        // is more reliable.
        int x = spannable.getSpanStart(clickable[0]) + 1;
        float nChars = text.getLayout().getLineVisibleEnd(0);

        // Not all characters have the same width but this is a good approximation.
        float sizePerChar =  text.getLayout().getLineWidth(0) / nChars;
        float xPos = text.getPaddingLeft() + (sizePerChar * x);
        float yPos = text.getHeight() / (float) 2;

        TestTouchUtils.singleClickView(instrumentation, text, (int) xPos, (int) yPos);

        assertInfoBarText(infoBar, activity.getString(R.string.translate_infobar_change_languages));
    }

    public static void assertInfoBarText(InfoBar infoBar, String expectedText) {
        View view = infoBar.getView().findViewById(R.id.infobar_message);
        Assert.assertNotNull(view);
        String actualText = findInfoBarText(view);
        Assert.assertEquals(expectedText, actualText);
    }

    public static void assertCompactTranslateInfoBar(InfoBar infoBar) {
        Assert.assertTrue(infoBar.getView() instanceof InfoBarCompactLayout);

        View content = infoBar.getView().findViewById(R.id.translate_infobar_content);
        Assert.assertNotNull(content);

        View tabLayout = content.findViewById(R.id.translate_infobar_tabs);
        Assert.assertTrue(tabLayout instanceof TranslateTabLayout);
    }

    public static void assertHasAtLeastTwoLanguageTabs(TranslateCompactInfoBar infoBar) {
        View content = infoBar.getView().findViewById(R.id.translate_infobar_content);
        Assert.assertNotNull(content);

        TranslateTabLayout tabLayout =
                (TranslateTabLayout) content.findViewById(R.id.translate_infobar_tabs);
        Assert.assertTrue(tabLayout.getTabCount() >= 2);
    }

    /**
     * Checks if the menu button exists on the InfoBar.
     * @return True if the View was found.
     */
    public static boolean hasMenuButton(InfoBar infoBar) {
        return InfoBarUtil.findButton(infoBar, R.id.translate_infobar_menu_button, false);
    }

    /**
     * Simulates clicking the menu button in the specified infobar.
     * @return True if the View was found.
     */
    public static boolean clickMenuButton(InfoBar infoBar) {
        return InfoBarUtil.findButton(infoBar, R.id.translate_infobar_menu_button, true);
    }

    /**
     * Simulates clicking the menu button and check if overflow menu is shown.
     */
    public static void clickMenuButtonAndAssertMenuShown(final TranslateCompactInfoBar infoBar) {
        clickMenuButton(infoBar);
        CriteriaHelper.pollInstrumentationThread(new Criteria("Overflow menu did not show") {
            @Override
            public boolean isSatisfied() {
                return infoBar.isShowingOverflowMenuForTesting();
            }
        });
    }

    /**
     * Simulates clicking the 'More Language' menu item and check if language menu is shown.
     */
    public static void clickMoreLanguageButtonAndAssertLanguageMenuShown(
            Instrumentation instrumentation, final TranslateCompactInfoBar infoBar) {
        invokeOverflowMenuActionSync(
                instrumentation, infoBar, TranslateMenu.ID_OVERFLOW_MORE_LANGUAGE);
        CriteriaHelper.pollInstrumentationThread(new Criteria("Language menu did not show") {
            @Override
            public boolean isSatisfied() {
                return infoBar.isShowingLanguageMenuForTesting();
            }
        });
    }

    /**
     * Execute a particular menu item from the overflow menu.
     * The item is executed even if it is disabled or not visible.
     */
    public static void invokeOverflowMenuActionSync(
            Instrumentation instrumentation, final TranslateCompactInfoBar infoBar, final int id) {
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                infoBar.onOverflowMenuItemClicked(id);
            }
        });
    }

    private static String findInfoBarText(View view) {
        TextView text = (TextView) view.findViewById(R.id.infobar_message);
        return text != null ? text.getText().toString() : null;
    }
}
