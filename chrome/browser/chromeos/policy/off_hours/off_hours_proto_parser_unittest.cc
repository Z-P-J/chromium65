// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/off_hours/off_hours_proto_parser.h"

#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/off_hours/off_hours_interval.h"
#include "chrome/browser/chromeos/policy/off_hours/weekly_time.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {
namespace off_hours {

namespace {

constexpr em::WeeklyTimeProto_DayOfWeek kWeekdays[] = {
    em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED,
    em::WeeklyTimeProto::MONDAY,
    em::WeeklyTimeProto::TUESDAY,
    em::WeeklyTimeProto::WEDNESDAY,
    em::WeeklyTimeProto::THURSDAY,
    em::WeeklyTimeProto::FRIDAY,
    em::WeeklyTimeProto::SATURDAY,
    em::WeeklyTimeProto::SUNDAY};

constexpr base::TimeDelta kHour = base::TimeDelta::FromHours(1);

const char kUtcTimezone[] = "UTC";

const int kDeviceAllowNewUsersTag = 3;
const int kDeviceGuestModeEnabledTag = 8;

const std::vector<int> kDefaultIgnoredPolicies = {kDeviceAllowNewUsersTag,
                                                  kDeviceGuestModeEnabledTag};

struct OffHoursPolicy {
  std::string timezone;
  std::vector<OffHoursInterval> intervals;
  std::vector<int> ignored_policy_proto_tags;

  OffHoursPolicy(const std::string& timezone,
                 const std::vector<OffHoursInterval>& intervals,
                 const std::vector<int>& ignored_policy_proto_tags)
      : timezone(timezone),
        intervals(intervals),
        ignored_policy_proto_tags(ignored_policy_proto_tags) {}
};

em::DeviceOffHoursIntervalProto ConvertOffHoursIntervalToProto(
    const OffHoursInterval& off_hours_interval) {
  em::DeviceOffHoursIntervalProto interval_proto;
  em::WeeklyTimeProto* start = interval_proto.mutable_start();
  em::WeeklyTimeProto* end = interval_proto.mutable_end();
  start->set_day_of_week(kWeekdays[off_hours_interval.start().day_of_week()]);
  start->set_time(off_hours_interval.start().milliseconds());
  end->set_day_of_week(kWeekdays[off_hours_interval.end().day_of_week()]);
  end->set_time(off_hours_interval.end().milliseconds());
  return interval_proto;
}

void RemoveOffHoursPolicyFromProto(em::ChromeDeviceSettingsProto* proto) {
  proto->clear_device_off_hours();
}

void SetOffHoursPolicyToProto(em::ChromeDeviceSettingsProto* proto,
                              const OffHoursPolicy& off_hours_policy) {
  RemoveOffHoursPolicyFromProto(proto);
  auto* off_hours = proto->mutable_device_off_hours();
  for (auto interval : off_hours_policy.intervals) {
    auto interval_proto = ConvertOffHoursIntervalToProto(interval);
    auto* cur = off_hours->add_intervals();
    *cur = interval_proto;
  }
  off_hours->set_timezone(off_hours_policy.timezone);
  for (auto p : off_hours_policy.ignored_policy_proto_tags) {
    off_hours->add_ignored_policy_proto_tags(p);
  }
}

}  // namespace

class ConvertOffHoursProtoToValueTest : public testing::Test {
 protected:
  ConvertOffHoursProtoToValueTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ConvertOffHoursProtoToValueTest);
};

TEST_F(ConvertOffHoursProtoToValueTest, Test) {
  WeeklyTime start = WeeklyTime(1, kHour.InMilliseconds());
  WeeklyTime end = WeeklyTime(3, kHour.InMilliseconds() * 2);
  std::vector<OffHoursInterval> intervals = {OffHoursInterval(start, end)};

  em::ChromeDeviceSettingsProto proto;
  SetOffHoursPolicyToProto(
      &proto, OffHoursPolicy(kUtcTimezone, intervals, kDefaultIgnoredPolicies));

  std::unique_ptr<base::DictionaryValue> off_hours_value =
      policy::off_hours::ConvertOffHoursProtoToValue(proto.device_off_hours());

  base::DictionaryValue off_hours_expected;
  off_hours_expected.SetString("timezone", kUtcTimezone);
  auto intervals_value = std::make_unique<base::ListValue>();
  for (const auto& interval : intervals)
    intervals_value->Append(interval.ToValue());
  off_hours_expected.SetList("intervals", std::move(intervals_value));
  auto ignored_policies_value = std::make_unique<base::ListValue>();
  for (const auto& policy : kDefaultIgnoredPolicies)
    ignored_policies_value->GetList().emplace_back(policy);
  off_hours_expected.SetList("ignored_policy_proto_tags",
                             std::move(ignored_policies_value));

  EXPECT_EQ(*off_hours_value, off_hours_expected);
}

}  // namespace off_hours
}  // namespace policy
