// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tether/tether_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/tether/tether_service_factory.h"
#include "chrome/browser/cryptauth/chrome_cryptauth_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/network/tether_notification_presenter.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/components/tether/gms_core_notifications_state_tracker_impl.h"
#include "chromeos/components/tether/tether_component.h"
#include "chromeos/components/tether/tether_component_impl.h"
#include "chromeos/components/tether/tether_host_fetcher_impl.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/cryptauth_service.h"
#include "components/cryptauth/remote_device_provider_impl.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace {

constexpr int64_t kMinAdvertisingIntervalMilliseconds = 100;
constexpr int64_t kMaxAdvertisingIntervalMilliseconds = 100;

constexpr int64_t kMetricFalsePositiveSeconds = 2;

}  // namespace

// static
TetherService* TetherService::Get(Profile* profile) {
  if (!IsFeatureFlagEnabled())
    return nullptr;

  // Tether networks are only available for the primary user; thus, no
  // TetherService object should be created for secondary users. If multiple
  // instances were created for each user, inconsistencies could lead to browser
  // crashes. See https://crbug.com/809357.
  if (!chromeos::ProfileHelper::Get()->IsPrimaryProfile(profile))
    return nullptr;

  return TetherServiceFactory::GetForBrowserContext(profile);
}

// static
void TetherService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kInstantTetheringAllowed, true);
  registry->RegisterBooleanPref(prefs::kInstantTetheringEnabled, true);

  // If we initially assume that BLE advertising is not supported, it will
  // result in Tether's Settings and Quick Settings sections not being visible
  // when the user logs in with Bluetooth disabled (because the TechnologyState
  // will be UNAVAILABLE, instead of the desired UNINITIALIZED).
  //
  // Initially assuming that BLE advertising *is* supported works well for most
  // devices, but if a user first logs into a device without BLE advertising
  // support and with Bluetooth disabled, Tether will be visible in Settings and
  // Quick Settings, but disappear upon enabling Bluetooth. This is an
  // acceptable edge case, and likely rare because Bluetooth is enabled by
  // default on new logins. Additionally, through this pref, we will record if
  // BLE advertising is not supported and remember that for future logins.
  registry->RegisterBooleanPref(prefs::kInstantTetheringBleAdvertisingSupported,
                                true);

  chromeos::tether::TetherComponentImpl::RegisterProfilePrefs(registry);
}

// static
bool TetherService::IsFeatureFlagEnabled() {
  return base::FeatureList::IsEnabled(features::kInstantTethering);
}

// static.
std::string TetherService::TetherFeatureStateToString(
    const TetherFeatureState& state) {
  switch (state) {
    case (TetherFeatureState::SHUT_DOWN):
      return "[TetherService shut down]";
    case (TetherFeatureState::BLE_ADVERTISING_NOT_SUPPORTED):
      return "[BLE advertising not supported]";
    case (TetherFeatureState::NO_AVAILABLE_HOSTS):
      return "[no potential Tether hosts]";
    case (TetherFeatureState::CELLULAR_DISABLED):
      return "[Cellular setting disabled]";
    case (TetherFeatureState::PROHIBITED):
      return "[prohibited by device policy]";
    case (TetherFeatureState::BLUETOOTH_DISABLED):
      return "[Bluetooth is disabled]";
    case (TetherFeatureState::USER_PREFERENCE_DISABLED):
      return "[Instant Tethering preference is disabled]";
    case (TetherFeatureState::ENABLED):
      return "[Enabled]";
    case (TetherFeatureState::BLE_NOT_PRESENT):
      return "[BLE is not present on the device]";
    case (TetherFeatureState::WIFI_NOT_PRESENT):
      return "[Wi-Fi is not present on the device]";
    case (TetherFeatureState::SUSPENDED):
      return "[Suspended]";
    case (TetherFeatureState::TETHER_FEATURE_STATE_MAX):
      // |previous_feature_state_| is initialized to TETHER_FEATURE_STATE_MAX,
      // and this value is never actually used in practice.
      return "[TetherService initializing]";
    default:
      NOTREACHED();
      return "[Invalid state]";
  }
}

TetherService::TetherService(
    Profile* profile,
    chromeos::PowerManagerClient* power_manager_client,
    cryptauth::CryptAuthService* cryptauth_service,
    chromeos::NetworkStateHandler* network_state_handler)
    : profile_(profile),
      power_manager_client_(power_manager_client),
      cryptauth_service_(cryptauth_service),
      network_state_handler_(network_state_handler),
      notification_presenter_(
          std::make_unique<chromeos::tether::TetherNotificationPresenter>(
              profile_,
              chromeos::NetworkConnect::Get())),
      gms_core_notifications_state_tracker_(
          std::make_unique<
              chromeos::tether::GmsCoreNotificationsStateTrackerImpl>()),
      remote_device_provider_(
          cryptauth::RemoteDeviceProviderImpl::Factory::NewInstance(
              cryptauth_service->GetCryptAuthDeviceManager(),
              cryptauth_service->GetAccountId(),
              cryptauth_service->GetCryptAuthEnrollmentManager()
                  ->GetUserPrivateKey(),
              cryptauth_service)),
      tether_host_fetcher_(
          chromeos::tether::TetherHostFetcherImpl::Factory::NewInstance(
              remote_device_provider_.get())),
      timer_(std::make_unique<base::OneShotTimer>()),
      weak_ptr_factory_(this) {
  tether_host_fetcher_->AddObserver(this);
  power_manager_client_->AddObserver(this);
  network_state_handler_->AddObserver(this, FROM_HERE);

  registrar_.Init(profile_->GetPrefs());
  registrar_.Add(prefs::kInstantTetheringAllowed,
                 base::Bind(&TetherService::OnPrefsChanged,
                            weak_ptr_factory_.GetWeakPtr()));

  UMA_HISTOGRAM_BOOLEAN("InstantTethering.UserPreference.OnStartup",
                        IsEnabledbyPreference());
  PA_LOG(INFO) << "TetherService has started. Initial user preference value: "
               << IsEnabledbyPreference();

  // GetAdapter may call OnBluetoothAdapterFetched immediately which can cause
  // problems with the Fake implementation since the class is not fully
  // constructed yet. Post the GetAdapter call to avoid this.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(device::BluetoothAdapterFactory::GetAdapter,
                 base::Bind(&TetherService::OnBluetoothAdapterFetched,
                            weak_ptr_factory_.GetWeakPtr())));
}

TetherService::~TetherService() {
  if (tether_component_)
    tether_component_->RemoveObserver(this);
}

void TetherService::StartTetherIfPossible() {
  if (GetTetherTechnologyState() !=
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED) {
    return;
  }

  // Do not initialize the TetherComponent if it already exists.
  if (tether_component_)
    return;

  PA_LOG(INFO) << "Starting up TetherComponent.";
  tether_component_ =
      chromeos::tether::TetherComponentImpl::Factory::NewInstance(
          cryptauth_service_, tether_host_fetcher_.get(),
          notification_presenter_.get(),
          gms_core_notifications_state_tracker_.get(), profile_->GetPrefs(),
          network_state_handler_,
          chromeos::NetworkHandler::Get()
              ->managed_network_configuration_handler(),
          chromeos::NetworkConnect::Get(),
          chromeos::NetworkHandler::Get()->network_connection_handler(),
          adapter_);
}

chromeos::tether::GmsCoreNotificationsStateTracker*
TetherService::GetGmsCoreNotificationsStateTracker() {
  return gms_core_notifications_state_tracker_.get();
}

void TetherService::StopTetherIfNecessary() {
  if (!tether_component_ ||
      tether_component_->status() !=
          chromeos::tether::TetherComponent::Status::ACTIVE) {
    return;
  }

  PA_LOG(INFO) << "Shutting down TetherComponent.";

  chromeos::tether::TetherComponent::ShutdownReason shutdown_reason;
  switch (GetTetherFeatureState()) {
    case SHUT_DOWN:
      shutdown_reason =
          chromeos::tether::TetherComponent::ShutdownReason::USER_LOGGED_OUT;
      break;
    case SUSPENDED:
      shutdown_reason =
          chromeos::tether::TetherComponent::ShutdownReason::USER_CLOSED_LID;
      break;
    case CELLULAR_DISABLED:
      shutdown_reason =
          chromeos::tether::TetherComponent::ShutdownReason::CELLULAR_DISABLED;
      break;
    case BLUETOOTH_DISABLED:
      shutdown_reason =
          chromeos::tether::TetherComponent::ShutdownReason::BLUETOOTH_DISABLED;
      break;
    case USER_PREFERENCE_DISABLED:
      shutdown_reason =
          chromeos::tether::TetherComponent::ShutdownReason::PREF_DISABLED;
      break;
    case BLE_NOT_PRESENT:
      shutdown_reason = chromeos::tether::TetherComponent::ShutdownReason::
          BLUETOOTH_CONTROLLER_DISAPPEARED;
      break;
    default:
      PA_LOG(ERROR) << "Unexpected shutdown reason. FeatureState is "
                    << GetTetherFeatureState() << ".";
      shutdown_reason =
          chromeos::tether::TetherComponent::ShutdownReason::OTHER;
      break;
  }

  tether_component_->AddObserver(this);
  tether_component_->RequestShutdown(shutdown_reason);
}

void TetherService::Shutdown() {
  if (shut_down_)
    return;

  shut_down_ = true;

  // Remove all observers. This ensures that once Shutdown() is called, no more
  // calls to UpdateTetherTechnologyState() will be triggered.
  tether_host_fetcher_->RemoveObserver(this);
  power_manager_client_->RemoveObserver(this);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (adapter_)
    adapter_->RemoveObserver(this);
  registrar_.RemoveAll();

  // Shut down the feature. Note that this does not change Tether's technology
  // state in NetworkStateHandler because doing so could cause visual jank just
  // as the user logs out.
  StopTetherIfNecessary();
  tether_component_.reset();

  tether_host_fetcher_.reset();
  remote_device_provider_.reset();
  notification_presenter_.reset();
}

void TetherService::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  suspended_ = true;
  UpdateTetherTechnologyState();
}

void TetherService::SuspendDone(const base::TimeDelta& sleep_duration) {
  suspended_ = false;

  // If there was a previous TetherComponent instance in the process of an
  // asynchronous shutdown, that session is stale by this point. Kill it now, so
  // that the next session can start up immediately.
  if (tether_component_) {
    tether_component_->RemoveObserver(this);
    tether_component_.reset();
  }

  UpdateTetherTechnologyState();
}

void TetherService::OnTetherHostsUpdated() {
  UpdateTetherTechnologyState();
}

void TetherService::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                          bool powered) {
  // Once the BLE advertising interval has been set (regardless of if BLE
  // advertising is supported), simply update the TechnologyState.
  if (has_attempted_to_set_ble_advertising_interval_) {
    UpdateTetherTechnologyState();
    return;
  }

  // If the BluetoothAdapter was not powered when first fetched (see
  // OnBluetoothAdapterFetched()), now attempt to set the BLE advertising
  // interval.
  if (powered)
    SetBleAdvertisingInterval();
}

void TetherService::DeviceListChanged() {
  UpdateEnabledState();
}

void TetherService::DevicePropertiesUpdated(
    const chromeos::DeviceState* device) {
  if (device->Matches(chromeos::NetworkTypePattern::Tether() |
                      chromeos::NetworkTypePattern::WiFi())) {
    UpdateEnabledState();
  }
}

void TetherService::UpdateEnabledState() {
  bool was_pref_enabled = IsEnabledbyPreference();
  chromeos::NetworkStateHandler::TechnologyState tether_technology_state =
      network_state_handler_->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether());

  // If |was_tether_setting_enabled| differs from the new Tether
  // TechnologyState, the settings toggle has been changed. Update the
  // kInstantTetheringEnabled user pref accordingly.
  bool is_enabled;
  if (was_pref_enabled && tether_technology_state ==
                              chromeos::NetworkStateHandler::TechnologyState::
                                  TECHNOLOGY_AVAILABLE) {
    is_enabled = false;
  } else if (!was_pref_enabled && tether_technology_state ==
                                      chromeos::NetworkStateHandler::
                                          TechnologyState::TECHNOLOGY_ENABLED) {
    is_enabled = true;
  } else {
    is_enabled = was_pref_enabled;
  }

  if (is_enabled != was_pref_enabled) {
    profile_->GetPrefs()->SetBoolean(prefs::kInstantTetheringEnabled,
                                     is_enabled);
    UMA_HISTOGRAM_BOOLEAN("InstantTethering.UserPreference.OnToggle",
                          is_enabled);
    PA_LOG(INFO) << "Tether user preference changed. New value: " << is_enabled;
  }
  UpdateTetherTechnologyState();
}

void TetherService::OnShutdownComplete() {
  DCHECK(tether_component_->status() ==
         chromeos::tether::TetherComponent::Status::SHUT_DOWN);
  tether_component_->RemoveObserver(this);
  tether_component_.reset();
  PA_LOG(INFO) << "TetherComponent was shut down.";

  // It is possible that the Tether TechnologyState was set to ENABLED while the
  // previous TetherComponent instance was shutting down. If that was the case,
  // restart TetherComponent.
  if (!shut_down_)
    StartTetherIfPossible();
}

void TetherService::OnPrefsChanged() {
  UpdateTetherTechnologyState();
}

bool TetherService::HasSyncedTetherHosts() const {
  return tether_host_fetcher_->HasSyncedTetherHosts();
}

void TetherService::UpdateTetherTechnologyState() {
  if (!adapter_)
    return;

  chromeos::NetworkStateHandler::TechnologyState new_tether_technology_state =
      GetTetherTechnologyState();

  if (new_tether_technology_state ==
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED) {
    // If Tether should be enabled, notify NetworkStateHandler before starting
    // up the component. This ensures that it is not possible to add Tether
    // networks before the network stack is ready for them.
    network_state_handler_->SetTetherTechnologyState(
        new_tether_technology_state);
    StartTetherIfPossible();
  } else {
    // If Tether should not be enabled, shut down the component before notifying
    // NetworkStateHandler. This ensures that nothing in TetherComponent
    // attempts to edit Tether networks or properties when the network stack is
    // not ready for them.
    StopTetherIfNecessary();
    network_state_handler_->SetTetherTechnologyState(
        new_tether_technology_state);
  }
}

chromeos::NetworkStateHandler::TechnologyState
TetherService::GetTetherTechnologyState() {
  TetherFeatureState new_feature_state = GetTetherFeatureState();
  if (new_feature_state != previous_feature_state_) {
    PA_LOG(INFO) << "Tether state has changed. New state: "
                 << TetherFeatureStateToString(new_feature_state)
                 << ", Old state: "
                 << TetherFeatureStateToString(previous_feature_state_);
    previous_feature_state_ = new_feature_state;

    RecordTetherFeatureStateIfPossible();
  }

  switch (new_feature_state) {
    case SHUT_DOWN:
    case SUSPENDED:
    case BLE_NOT_PRESENT:
    case BLE_ADVERTISING_NOT_SUPPORTED:
    case WIFI_NOT_PRESENT:
    case NO_AVAILABLE_HOSTS:
    case CELLULAR_DISABLED:
      return chromeos::NetworkStateHandler::TechnologyState::
          TECHNOLOGY_UNAVAILABLE;

    case PROHIBITED:
      return chromeos::NetworkStateHandler::TechnologyState::
          TECHNOLOGY_PROHIBITED;

    case BLUETOOTH_DISABLED:
      return chromeos::NetworkStateHandler::TechnologyState::
          TECHNOLOGY_UNINITIALIZED;

    case USER_PREFERENCE_DISABLED:
      return chromeos::NetworkStateHandler::TechnologyState::
          TECHNOLOGY_AVAILABLE;

    case ENABLED:
      return chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED;

    default:
      return chromeos::NetworkStateHandler::TechnologyState::
          TECHNOLOGY_UNAVAILABLE;
  }
}

void TetherService::OnBluetoothAdapterFetched(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (shut_down_)
    return;

  adapter_ = adapter;
  adapter_->AddObserver(this);

  // Update TechnologyState in case Tether is otherwise available but Bluetooth
  // is off.
  UpdateTetherTechnologyState();

  // If |adapter_| is not powered, wait until it is to call
  // SetBleAdvertisingInterval(). See AdapterPoweredChanged().
  if (IsBluetoothPowered())
    SetBleAdvertisingInterval();
}

void TetherService::OnBluetoothAdapterAdvertisingIntervalSet() {
  has_attempted_to_set_ble_advertising_interval_ = true;
  SetIsBleAdvertisingSupportedPref(true);

  UpdateTetherTechnologyState();
}

void TetherService::OnBluetoothAdapterAdvertisingIntervalError(
    device::BluetoothAdvertisement::ErrorCode status) {
  has_attempted_to_set_ble_advertising_interval_ = true;
  SetIsBleAdvertisingSupportedPref(false);

  UpdateTetherTechnologyState();
}

void TetherService::SetBleAdvertisingInterval() {
  DCHECK(IsBluetoothPowered());
  adapter_->SetAdvertisingInterval(
      base::TimeDelta::FromMilliseconds(kMinAdvertisingIntervalMilliseconds),
      base::TimeDelta::FromMilliseconds(kMaxAdvertisingIntervalMilliseconds),
      base::Bind(&TetherService::OnBluetoothAdapterAdvertisingIntervalSet,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&TetherService::OnBluetoothAdapterAdvertisingIntervalError,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool TetherService::GetIsBleAdvertisingSupportedPref() {
  return profile_->GetPrefs()->GetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported);
}

void TetherService::SetIsBleAdvertisingSupportedPref(
    bool is_ble_advertising_supported) {
  profile_->GetPrefs()->SetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported,
      is_ble_advertising_supported);
}

bool TetherService::IsBluetoothPresent() const {
  return adapter_.get() && adapter_->IsPresent();
}

bool TetherService::IsBluetoothPowered() const {
  return IsBluetoothPresent() && adapter_->IsPowered();
}

bool TetherService::IsWifiPresent() const {
  return network_state_handler_->IsTechnologyAvailable(
      chromeos::NetworkTypePattern::WiFi());
}

bool TetherService::IsCellularAvailableButNotEnabled() const {
  return (network_state_handler_->IsTechnologyAvailable(
              chromeos::NetworkTypePattern::Cellular()) &&
          !network_state_handler_->IsTechnologyEnabled(
              chromeos::NetworkTypePattern::Cellular()));
}

bool TetherService::IsAllowedByPolicy() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kInstantTetheringAllowed);
}

bool TetherService::IsEnabledbyPreference() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kInstantTetheringEnabled);
}

TetherService::TetherFeatureState TetherService::GetTetherFeatureState() {
  if (shut_down_)
    return SHUT_DOWN;

  if (suspended_)
    return SUSPENDED;

  if (!IsBluetoothPresent())
    return BLE_NOT_PRESENT;

  if (!IsWifiPresent())
    return WIFI_NOT_PRESENT;

  if (!GetIsBleAdvertisingSupportedPref())
    return BLE_ADVERTISING_NOT_SUPPORTED;

  if (!HasSyncedTetherHosts())
    return NO_AVAILABLE_HOSTS;

  // If Cellular technology is available, then Tether technology is treated
  // as a subset of Cellular, and it should only be enabled when Cellular
  // technology is enabled.
  if (IsCellularAvailableButNotEnabled())
    return CELLULAR_DISABLED;

  if (!IsAllowedByPolicy())
    return PROHIBITED;

  if (!IsBluetoothPowered())
    return BLUETOOTH_DISABLED;

  if (!IsEnabledbyPreference())
    return USER_PREFERENCE_DISABLED;

  return ENABLED;
}

void TetherService::RecordTetherFeatureState() {
  TetherFeatureState tether_feature_state = GetTetherFeatureState();
  DCHECK(tether_feature_state != TetherFeatureState::TETHER_FEATURE_STATE_MAX);

  // If the feature is shut down, there is no need to log a metric. Since this
  // state occurs every time the user logs out (as of crbug.com/782879), logging
  // a metric here does not provide any value since it does not indicate
  // anything about how the user utilizes Instant Tethering and would dilute the
  // contributions of meaningful states.
  if (tether_feature_state == TetherFeatureState::SHUT_DOWN)
    return;

  UMA_HISTOGRAM_ENUMERATION("InstantTethering.FeatureState",
                            tether_feature_state,
                            TetherFeatureState::TETHER_FEATURE_STATE_MAX);
}

void TetherService::RecordTetherFeatureStateIfPossible() {
  if (HandleFeatureStateMetricIfUninitialized())
    return;

  // If the timer meant to record the initial
  // TetherFeatureState::BLE_NOT_PRESENT value is running, cancel it -- it is a
  // false positive report.
  if (timer_->IsRunning())
    timer_->Stop();

  RecordTetherFeatureState();
}

bool TetherService::HandleFeatureStateMetricIfUninitialized() {
  TetherFeatureState current_state = GetTetherFeatureState();
  if (current_state != TetherFeatureState::BLE_NOT_PRESENT &&
      current_state != TetherFeatureState::NO_AVAILABLE_HOSTS) {
    // These are the only two possible false-positive states. If the current
    // state is another state, no processing is needed.
    return false;
  }

  // When TetherService starts up, we expect that BLE is not present.
  // Eventually, BLE starts up, but at that point, Tether hosts are not yet
  // fetched. During startup, these states are transient, so it would be
  // incorrect to log a metric stating that this was the state upon startup.
  bool should_start_timer = false;
  if (current_state == TetherFeatureState::BLE_NOT_PRESENT &&
      !ble_not_present_false_positive_encountered_) {
    ble_not_present_false_positive_encountered_ = true;
    should_start_timer = true;
  } else if (current_state == TetherFeatureState::NO_AVAILABLE_HOSTS &&
             !no_available_hosts_false_positive_encountered_) {
    no_available_hosts_false_positive_encountered_ = true;
    should_start_timer = true;
  }

  if (!should_start_timer)
    return false;

  // Start the timer. If it fires without being stopped, the metric will be
  // recorded. |kMetricFalsePositiveSeconds| is chosen such that it is long
  // enough that we can assume a false positive did not occur and that the
  // metric value is actually correct.
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromSeconds(kMetricFalsePositiveSeconds),
                base::Bind(&TetherService::RecordTetherFeatureState,
                           weak_ptr_factory_.GetWeakPtr()));

  return true;
}

void TetherService::SetTestDoubles(
    std::unique_ptr<chromeos::tether::NotificationPresenter>
        notification_presenter,
    std::unique_ptr<base::Timer> timer) {
  notification_presenter_ = std::move(notification_presenter);
  timer_ = std::move(timer);
}
