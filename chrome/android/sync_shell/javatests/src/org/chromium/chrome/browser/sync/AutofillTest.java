// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.support.test.filters.LargeTest;
import android.util.Pair;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.sync.SyncTestRule.DataCriteria;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.protocol.AutofillProfileSpecifics;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Test suite for the autofill profile sync data type.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure // crbug.com/637448
public class AutofillTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String TAG = "AutofillTest";

    private static final String AUTOFILL_TYPE = "Autofill Profiles";

    private static final String GUID = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
    private static final String ORIGIN = "https://www.chromium.org/";

    private static final String STREET = "1600 Amphitheatre Pkwy";
    private static final String CITY = "Mountain View";
    private static final String MODIFIED_CITY = "Sunnyvale";
    private static final String STATE = "CA";
    private static final String ZIP = "94043";

    // A container to store autofill profile information for data verification.
    private static class Autofill {
        public final String id;
        public final String street;
        public final String city;
        public final String state;
        public final String zip;

        public Autofill(String id, String street, String city, String state, String zip) {
            this.id = id;
            this.street = street;
            this.city = city;
            this.state = state;
            this.zip = zip;
        }
    }

    private abstract class ClientAutofillCriteria extends DataCriteria<Autofill> {
        @Override
        public List<Autofill> getData() throws Exception {
            return getClientAutofillProfiles();
        }
    }

    @Before
    public void setUp() throws Exception {
        mSyncTestRule.setUpTestAccountAndSignIn();
        // Make sure the initial state is clean.
        assertClientAutofillProfileCount(0);
        assertServerAutofillProfileCountWithName(0, STREET);
    }

    // Test syncing an autofill profile from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadAutofill() throws Exception {
        addServerAutofillProfile(STREET, CITY, STATE, ZIP);
        SyncTestUtil.triggerSync();
        waitForClientAutofillProfileCount(1);

        // Verify data synced to client.
        List<Autofill> autofills = getClientAutofillProfiles();
        Assert.assertEquals(
                "Only the injected autofill should exist on the client.", 1, autofills.size());
        Autofill autofill = autofills.get(0);
        Assert.assertEquals("The wrong street was found for the address.", STREET, autofill.street);
        Assert.assertEquals("The wrong city was found for the autofill.", CITY, autofill.city);
        Assert.assertEquals("The wrong state was found for the autofill.", STATE, autofill.state);
        Assert.assertEquals("The wrong zip was found for the autofill.", ZIP, autofill.zip);
    }

    // Test syncing an autofill profile modification from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadAutofillModification() throws Exception {
        // Add the entity to test modifying.
        EntitySpecifics specifics = addServerAutofillProfile(STREET, CITY, STATE, ZIP);
        SyncTestUtil.triggerSync();
        waitForClientAutofillProfileCount(1);

        // Modify on server, sync, and verify modification locally.
        Autofill autofill = getClientAutofillProfiles().get(0);
        specifics.autofillProfile.addressHomeCity = MODIFIED_CITY;
        mSyncTestRule.getFakeServerHelper().modifyEntitySpecifics(autofill.id, specifics);
        SyncTestUtil.triggerSync();
        mSyncTestRule.pollInstrumentationThread(new ClientAutofillCriteria() {
            @Override
            public boolean isSatisfied(List<Autofill> autofills) {
                Autofill modifiedAutofill = autofills.get(0);
                return modifiedAutofill.city.equals(MODIFIED_CITY);
            }
        });
    }

    // Test syncing an autofill profile deletion from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadDeletedAutofill() throws Exception {
        // Add the entity to test deleting.
        addServerAutofillProfile(STREET, CITY, STATE, ZIP);
        SyncTestUtil.triggerSync();
        waitForClientAutofillProfileCount(1);

        // Delete on server, sync, and verify deleted locally.
        Autofill autofill = getClientAutofillProfiles().get(0);
        mSyncTestRule.getFakeServerHelper().deleteEntity(autofill.id);
        SyncTestUtil.triggerSync();
        waitForClientAutofillProfileCount(0);
    }

    // Test that autofill profiles don't get synced if the data type is disabled.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDisabledNoDownloadAutofill() throws Exception {
        // The AUTOFILL type here controls both AUTOFILL and AUTOFILL_PROFILE.
        mSyncTestRule.disableDataType(ModelType.AUTOFILL);
        addServerAutofillProfile(STREET, CITY, STATE, ZIP);
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        assertClientAutofillProfileCount(0);
    }

    private EntitySpecifics addServerAutofillProfile(
            String street, String city, String state, String zip) {
        EntitySpecifics specifics = new EntitySpecifics();
        AutofillProfileSpecifics profile = new AutofillProfileSpecifics();
        profile.guid = GUID;
        profile.origin = ORIGIN;
        profile.addressHomeLine1 = street;
        profile.addressHomeCity = city;
        profile.addressHomeState = state;
        profile.addressHomeZip = zip;
        specifics.autofillProfile = profile;
        mSyncTestRule.getFakeServerHelper().injectUniqueClientEntity(street /* name */, specifics);
        return specifics;
    }

    private List<Autofill> getClientAutofillProfiles() throws JSONException {
        List<Pair<String, JSONObject>> entities =
                SyncTestUtil.getLocalData(mSyncTestRule.getTargetContext(), AUTOFILL_TYPE);
        List<Autofill> autofills = new ArrayList<Autofill>(entities.size());
        for (Pair<String, JSONObject> entity : entities) {
            String id = entity.first;
            JSONObject profile = entity.second;
            String street = profile.getString("address_home_line1");
            String city = profile.getString("address_home_city");
            String state = profile.getString("address_home_state");
            String zip = profile.getString("address_home_zip");
            autofills.add(new Autofill(id, street, city, state, zip));
        }
        return autofills;
    }

    private void assertClientAutofillProfileCount(int count) throws JSONException {
        Assert.assertEquals("There should be " + count + " local autofill profiles.", count,
                SyncTestUtil.getLocalData(mSyncTestRule.getTargetContext(), AUTOFILL_TYPE).size());
    }

    private void assertServerAutofillProfileCountWithName(int count, String name) {
        Assert.assertTrue("Expected " + count + " server autofill profiles with name " + name + ".",
                mSyncTestRule.getFakeServerHelper().verifyEntityCountByTypeAndName(
                        count, ModelType.AUTOFILL_PROFILE, name));
    }

    private void waitForClientAutofillProfileCount(int count) {
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(count, new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return SyncTestUtil.getLocalData(mSyncTestRule.getTargetContext(), AUTOFILL_TYPE)
                        .size();
            }
        }), SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }
}
