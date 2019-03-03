// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/fake_server_verifier.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/json/json_writer.h"
#include "components/sync/test/fake_server/fake_server.h"

using base::JSONWriter;
using std::string;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;

namespace fake_server {

namespace {

AssertionResult DictionaryCreationAssertionFailure() {
  return AssertionFailure() << "FakeServer failed to create an entities "
                            << "dictionary.";
}

AssertionResult VerificationCountAssertionFailure(size_t actual_count,
                                                  size_t expected_count) {
  return AssertionFailure() << "Actual count: " << actual_count << "; "
                            << "Expected count: " << expected_count;
}

AssertionResult UnknownTypeAssertionFailure(const string& model_type) {
  return AssertionFailure() << "Verification not attempted. Unknown ModelType: "
                            << model_type;
}

AssertionResult VerifySessionsHierarchyEquality(
    const SessionsHierarchy& expected,
    const SessionsHierarchy& actual) {
  if (expected.Equals(actual))
    return AssertionSuccess() << "Sessions hierarchies are equal.";

  return AssertionFailure() << "Sessions hierarchies are not equal. "
                            << "FakeServer contents: " << actual.ToString()
                            << "; Expected contents: " << expected.ToString();
}

// Caller maintains ownership of |entities|.
string ConvertFakeServerContentsToString(
    const base::DictionaryValue& entities) {
  string entities_str;
  if (!JSONWriter::WriteWithOptions(entities, JSONWriter::OPTIONS_PRETTY_PRINT,
                                    &entities_str)) {
    entities_str = "Could not convert FakeServer contents to string.";
  }
  return "FakeServer contents:\n" + entities_str;
}

}  // namespace

FakeServerVerifier::FakeServerVerifier(FakeServer* fake_server)
    : fake_server_(fake_server) {}

FakeServerVerifier::~FakeServerVerifier() {}

AssertionResult FakeServerVerifier::VerifyEntityCountByType(
    size_t expected_count,
    syncer::ModelType model_type) const {
  std::unique_ptr<base::DictionaryValue> entities =
      fake_server_->GetEntitiesAsDictionaryValue();
  if (!entities.get()) {
    return DictionaryCreationAssertionFailure();
  }

  string model_type_string = ModelTypeToString(model_type);
  base::ListValue* entity_list = nullptr;
  if (!entities->GetList(model_type_string, &entity_list)) {
    return UnknownTypeAssertionFailure(model_type_string);
  } else if (expected_count != entity_list->GetSize()) {
    return VerificationCountAssertionFailure(entity_list->GetSize(),
                                             expected_count)
           << "\n\n"
           << ConvertFakeServerContentsToString(*entities);
  }

  return AssertionSuccess();
}

AssertionResult FakeServerVerifier::VerifyEntityCountByTypeAndName(
    size_t expected_count,
    syncer::ModelType model_type,
    const string& name) const {
  std::unique_ptr<base::DictionaryValue> entities =
      fake_server_->GetEntitiesAsDictionaryValue();
  if (!entities.get()) {
    return DictionaryCreationAssertionFailure();
  }

  string model_type_string = ModelTypeToString(model_type);
  base::ListValue* entity_list = nullptr;
  size_t actual_count = 0;
  if (entities->GetList(model_type_string, &entity_list)) {
    base::Value name_value(name);
    for (const auto& entity : *entity_list) {
      if (name_value.Equals(&entity))
        actual_count++;
    }
  }

  if (!entity_list) {
    return UnknownTypeAssertionFailure(model_type_string);
  } else if (actual_count != expected_count) {
    return VerificationCountAssertionFailure(actual_count, expected_count)
           << "; Name: " << name << "\n\n"
           << ConvertFakeServerContentsToString(*entities);
  }

  return AssertionSuccess();
}

AssertionResult FakeServerVerifier::VerifySessions(
    const SessionsHierarchy& expected_sessions) {
  std::vector<sync_pb::SyncEntity> sessions =
      fake_server_->GetSyncEntitiesByModelType(syncer::SESSIONS);
  // Look for the sessions entity containing a SessionHeader and cache all tab
  // IDs/URLs. These will be used later to construct a SessionsHierarchy.
  sync_pb::SessionHeader session_header;
  std::map<int, int> tab_ids_to_window_ids;
  std::map<int, std::string> tab_ids_to_urls;
  std::string session_tag;
  for (const auto& entity : sessions) {
    sync_pb::SessionSpecifics session_specifics = entity.specifics().session();

    // Ensure that all session tags match the first entity. Only one session is
    // supported for verification at this time.
    if (session_tag.empty())
      session_tag = session_specifics.session_tag();
    else if (session_specifics.session_tag() != session_tag)
      return AssertionFailure() << "Multiple session tags found.";

    if (session_specifics.has_header()) {
      session_header = session_specifics.header();
    } else if (session_specifics.has_tab()) {
      const sync_pb::SessionTab& tab = session_specifics.tab();
      const sync_pb::TabNavigation& nav =
          tab.navigation(tab.current_navigation_index());
      // Only read from tabs that have a title on their current navigation
      // entry. This the result of an oddity around the timing of sessions
      // related changes. Sometimes when opening a new window, the first
      // navigation will be committed before the title has been set. Then a
      // subsequent commit will go through for that same navigation. Because
      // this logic is used to ensure synchronization, we are going to exclude
      // partially omitted navigations. The full navigation should typically be
      // committed in full immediately after we fail a check because of this.
      if (nav.has_title()) {
        tab_ids_to_window_ids[tab.tab_id()] = tab.window_id();
        tab_ids_to_urls[tab.tab_id()] = nav.virtual_url();
      }
    }
  }

  // Create a SessionsHierarchy from the cached SyncEntity data. This loop over
  // the SessionHeader also ensures its data corresponds to the data stored in
  // each SessionTab.
  SessionsHierarchy actual_sessions;
  for (const auto& window : session_header.window()) {
    std::multiset<std::string> tab_urls;
    for (int tab_id : window.tab()) {
      if (tab_ids_to_window_ids.find(tab_id) == tab_ids_to_window_ids.end()) {
        return AssertionFailure() << "Malformed data: Tab entity not found.";
      }
      tab_urls.insert(tab_ids_to_urls[tab_id]);
    }
    actual_sessions.AddWindow(tab_urls);
  }
  return VerifySessionsHierarchyEquality(expected_sessions, actual_sessions);
}

}  // namespace fake_server
