// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_HATS_HATS_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_HATS_HATS_NOTIFICATION_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/notification_delegate.h"

class Profile;
class NetworkState;

namespace chromeos {

// Happiness tracking survey (HaTS) notification controller is responsible for
// managing the HaTS notification that is displayed to the user.
class HatsNotificationController : public message_center::NotificationDelegate,
                                   public NetworkPortalDetector::Observer {
 public:
  static const char kNotificationId[];

  explicit HatsNotificationController(Profile* profile);

  // Returns true if the survey needs to be displayed for the given |profile|.
  static bool ShouldShowSurveyToProfile(Profile* profile);

 private:
  friend class HatsNotificationControllerTest;
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           NewDevice_ShouldNotShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           OldDevice_ShouldShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           NoInternet_DoNotShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           InternetConnected_ShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           DismissNotification_ShouldUpdatePref);

  ~HatsNotificationController() override;

  // NotificationDelegate overrides:
  void Initialize(bool is_new_device);
  void ButtonClick(int button_index) override;
  void Close(bool by_user) override;
  void Click() override;

  // NetworkPortalDetector::Observer override:
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

  void UpdateLastInteractionTime();

  Profile* profile_;
  base::WeakPtrFactory<HatsNotificationController> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(HatsNotificationController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_HATS_HATS_NOTIFICATION_CONTROLLER_H_
