// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/components/tether/tether_component.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "device/bluetooth/bluetooth_adapter.h"

class Profile;

namespace chromeos {
class NetworkStateHandler;
namespace tether {
class GmsCoreNotificationsStateTracker;
class GmsCoreNotificationsStateTrackerImpl;
class NotificationPresenter;
}  // namespace tether
}  // namespace chromeos

namespace cryptauth {
class CryptAuthService;
class RemoteDeviceProvider;
}  // namespace cryptauth

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Service providing access to the Instant Tethering component. Provides an
// interface to start up the component as well as to retrieve metadata about
// ongoing Tether connections.
//
// This service starts up when the user logs in (or recovers from a crash) and
// is shut down when the user logs out.
class TetherService : public KeyedService,
                      public chromeos::PowerManagerClient::Observer,
                      public chromeos::tether::TetherHostFetcher::Observer,
                      public device::BluetoothAdapter::Observer,
                      public chromeos::NetworkStateHandlerObserver,
                      public chromeos::tether::TetherComponent::Observer {
 public:
  TetherService(Profile* profile,
                chromeos::PowerManagerClient* power_manager_client,
                cryptauth::CryptAuthService* cryptauth_service,
                chromeos::NetworkStateHandler* network_state_handler);
  ~TetherService() override;

  // Gets TetherService instance.
  static TetherService* Get(Profile* profile);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Whether the Tether feature has been enabled via a chrome://about or
  // command line flag.
  static bool IsFeatureFlagEnabled();

  // Attempt to start the Tether module. Only succeeds if all conditions to
  // reach chromeos::NetworkStateHandler::TechnologyState::ENABLED are reached.
  // Should only be called once a user is logged in.
  virtual void StartTetherIfPossible();

  virtual chromeos::tether::GmsCoreNotificationsStateTracker*
  GetGmsCoreNotificationsStateTracker();

 protected:
  // KeyedService:
  void Shutdown() override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // chromeos::tether::TetherHostFetcher::Observer
  void OnTetherHostsUpdated() override;

  // device::BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  // chromeos::NetworkStateHandlerObserver:
  void DeviceListChanged() override;
  void DevicePropertiesUpdated(const chromeos::DeviceState* device) override;

  // Helper method called from NetworkStateHandlerObserver methods.
  void UpdateEnabledState();

  // chromeos::tether::TetherComponent::Observer:
  void OnShutdownComplete() override;

  // Callback when the controlling pref changes.
  void OnPrefsChanged();

  // Stop the Tether module if it is currently enabled; if it was not enabled,
  // this function is a no-op.
  virtual void StopTetherIfNecessary();

  // Whether Tether hosts are available.
  virtual bool HasSyncedTetherHosts() const;

  virtual void UpdateTetherTechnologyState();
  chromeos::NetworkStateHandler::TechnologyState GetTetherTechnologyState();

  chromeos::NetworkStateHandler* network_state_handler() {
    return network_state_handler_;
  }

 private:
  friend class TetherServiceTest;
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestSuspend);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestBleAdvertisingNotSupported);
  FRIEND_TEST_ALL_PREFIXES(
      TetherServiceTest,
      TestBleAdvertisingNotSupported_BluetoothIsInitiallyNotPowered);
  FRIEND_TEST_ALL_PREFIXES(
      TetherServiceTest,
      TestBleAdvertisingNotSupportedAndRecorded_BluetoothIsInitiallyNotPowered);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest,
                           TestBleAdvertisingSupportedButIncorrectlyRecorded);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest,
                           TestGet_PrimaryUser_FeatureFlagEnabled);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestNoTetherHosts);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestProhibitedByPolicy);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestIsBluetoothPowered);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestCellularIsUnavailable);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestCellularIsAvailable);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestDisabled);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestEnabled);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestBluetoothNotification);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestBluetoothNotPresent);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestMetricsFalsePositives);
  FRIEND_TEST_ALL_PREFIXES(TetherServiceTest, TestWifiNotPresent);

  // Reflects InstantTethering_TechnologyStateAndReason enum in enums.xml. Do
  // not rearrange.
  enum TetherFeatureState {
    // Note: Value 0 was previously OTHER_OR_UNKNOWN, but this was a vague
    // description.
    SHUT_DOWN = 0,
    BLE_ADVERTISING_NOT_SUPPORTED = 1,
    // Note: Value 2 was previously SCREEN_LOCKED, but this value is obsolete
    // and should no longer be used.
    NO_AVAILABLE_HOSTS = 3,
    CELLULAR_DISABLED = 4,
    PROHIBITED = 5,
    BLUETOOTH_DISABLED = 6,
    USER_PREFERENCE_DISABLED = 7,
    ENABLED = 8,
    BLE_NOT_PRESENT = 9,
    WIFI_NOT_PRESENT = 10,
    SUSPENDED = 11,
    TETHER_FEATURE_STATE_MAX
  };

  // For debug logs.
  static std::string TetherFeatureStateToString(
      const TetherFeatureState& state);

  void OnBluetoothAdapterFetched(
      scoped_refptr<device::BluetoothAdapter> adapter);
  void OnBluetoothAdapterAdvertisingIntervalSet();
  void OnBluetoothAdapterAdvertisingIntervalError(
      device::BluetoothAdvertisement::ErrorCode status);

  void SetBleAdvertisingInterval();

  // Whether BLE advertising is supported on this device. This should only
  // return true if a call to BluetoothAdapter::SetAdvertisingInterval() during
  // TetherService construction succeeds. That method will fail in cases like
  // those captured in crbug.com/738222.
  bool GetIsBleAdvertisingSupportedPref();
  void SetIsBleAdvertisingSupportedPref(bool is_ble_advertising_supported);

  bool IsBluetoothPresent() const;
  bool IsBluetoothPowered() const;

  bool IsWifiPresent() const;

  bool IsCellularAvailableButNotEnabled() const;

  // Whether Tether is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted if the flag is enabled.
  bool IsAllowedByPolicy() const;

  // Whether Tether is enabled.
  bool IsEnabledbyPreference() const;

  TetherFeatureState GetTetherFeatureState();

  // Record to UMA Tether's current feature state.
  void RecordTetherFeatureState();

  // Attempt to record the current Tether FeatureState.
  void RecordTetherFeatureStateIfPossible();

  // Handles potential false positive metric states which may occur normally
  // during startup. In the normal case (i.e., when Tether is enabled), the
  // state transitions from OTHER_OR_UNKNOWN -> BLE_NOT_PRESENT ->
  // NO_AVAILABLE_HOSTS -> ENABLED, but we do not wish to log metrics for the
  // intermediate states (BLE_NOT_PRESENT or NO_AVAILABLE_HOSTS), since these
  // are ephemeral. Returns whether a false positive case was handled.
  bool HandleFeatureStateMetricIfUninitialized();

  void SetTestDoubles(std::unique_ptr<chromeos::tether::NotificationPresenter>
                          notification_presenter,
                      std::unique_ptr<base::Timer> timer);

  // Whether the service has been shut down.
  bool shut_down_ = false;

  // Whether the device and service have been suspended (e.g. the laptop lid
  // was closed).
  bool suspended_ = false;

  // Whether the BLE advertising interval has attempted to be set during this
  // session.
  bool has_attempted_to_set_ble_advertising_interval_ = false;

  // The first report of TetherFeatureState::BLE_NOT_PRESENT is usually
  // incorrect and hence is a false positive. This property tracks if the first
  // report has been hit yet.
  bool ble_not_present_false_positive_encountered_ = false;

  // The first report of TetherFeatureState::NO_AVAILABLE_HOSTS may be incorrect
  // and hence a false positive. This property tracks if the first report has
  // been hit yet.
  bool no_available_hosts_false_positive_encountered_ = false;

  // The TetherFeatureState obtained the last time that
  // GetTetherTechnologyState() was called. Used only for logging purposes.
  TetherFeatureState previous_feature_state_ =
      TetherFeatureState::TETHER_FEATURE_STATE_MAX;

  Profile* profile_;
  chromeos::PowerManagerClient* power_manager_client_;
  cryptauth::CryptAuthService* cryptauth_service_;
  chromeos::NetworkStateHandler* network_state_handler_;
  std::unique_ptr<chromeos::tether::NotificationPresenter>
      notification_presenter_;
  std::unique_ptr<chromeos::tether::GmsCoreNotificationsStateTrackerImpl>
      gms_core_notifications_state_tracker_;
  std::unique_ptr<cryptauth::RemoteDeviceProvider> remote_device_provider_;
  std::unique_ptr<chromeos::tether::TetherHostFetcher> tether_host_fetcher_;
  std::unique_ptr<chromeos::tether::TetherComponent> tether_component_;

  PrefChangeRegistrar registrar_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<base::Timer> timer_;

  base::WeakPtrFactory<TetherService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TetherService);
};

#endif  // CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_H_
