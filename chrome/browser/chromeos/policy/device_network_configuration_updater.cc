// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/policy/policy_constants.h"

namespace policy {

DeviceNetworkConfigurationUpdater::~DeviceNetworkConfigurationUpdater() {}

// static
std::unique_ptr<DeviceNetworkConfigurationUpdater>
DeviceNetworkConfigurationUpdater::CreateForDevicePolicy(
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler,
    chromeos::NetworkDeviceHandler* network_device_handler,
    chromeos::CrosSettings* cros_settings) {
  std::unique_ptr<DeviceNetworkConfigurationUpdater> updater(
      new DeviceNetworkConfigurationUpdater(
          policy_service, network_config_handler, network_device_handler,
          cros_settings));
  updater->Init();
  return updater;
}

DeviceNetworkConfigurationUpdater::DeviceNetworkConfigurationUpdater(
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler,
    chromeos::NetworkDeviceHandler* network_device_handler,
    chromeos::CrosSettings* cros_settings)
    : NetworkConfigurationUpdater(onc::ONC_SOURCE_DEVICE_POLICY,
                                  key::kDeviceOpenNetworkConfiguration,
                                  policy_service,
                                  network_config_handler),
      network_device_handler_(network_device_handler),
      cros_settings_(cros_settings),
      weak_factory_(this) {
  DCHECK(network_device_handler_);
  data_roaming_setting_subscription_ = cros_settings->AddSettingsObserver(
      chromeos::kSignedDataRoamingEnabled,
      base::Bind(
          &DeviceNetworkConfigurationUpdater::OnDataRoamingSettingChanged,
          base::Unretained(this)));
}

std::vector<std::string>
DeviceNetworkConfigurationUpdater::GetAuthorityCertificates() {
  base::ListValue certificates_onc;
  ParseCurrentPolicy(nullptr /* network_configs */,
                     nullptr /* global_network_config */, &certificates_onc);

  std::vector<std::string> x509_authority_certs;
  for (size_t i = 0; i < certificates_onc.GetSize(); ++i) {
    const base::DictionaryValue* certificate = nullptr;
    certificates_onc.GetDictionary(i, &certificate);
    DCHECK(certificate);

    const base::Value* cert_type_value = certificate->FindKeyOfType(
        ::onc::certificate::kType, base::Value::Type::STRING);
    if (cert_type_value &&
        cert_type_value->GetString() == ::onc::certificate::kAuthority) {
      const base::Value* cert_x509_value = certificate->FindKeyOfType(
          ::onc::certificate::kX509, base::Value::Type::STRING);
      if (!cert_x509_value || cert_x509_value->GetString().empty()) {
        LOG(ERROR) << "Certificate missing X509 data.";
        continue;
      }
      x509_authority_certs.push_back(cert_x509_value->GetString());
    }
  }

  return x509_authority_certs;
}

void DeviceNetworkConfigurationUpdater::Init() {
  NetworkConfigurationUpdater::Init();

  const policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  // The highest authority regarding whether cellular data roaming should be
  // allowed is the Device Policy. If there is no Device Policy, then
  // data roaming should be allowed if this is a Cellular First device.
  if (!connector->IsEnterpriseManaged() &&
      chromeos::switches::IsCellularFirstDevice()) {
    network_device_handler_->SetCellularAllowRoaming(true);
  } else {
    // Apply the roaming setting initially.
    OnDataRoamingSettingChanged();
  }

  // Set up MAC address randomization if we are not enterprise managed.

  network_device_handler_->SetMACAddressRandomizationEnabled(
      !connector->IsEnterpriseManaged());
}

void DeviceNetworkConfigurationUpdater::ImportCertificates(
    const base::ListValue& certificates_onc) {
  // Importing client certificates from device policy is not implemented.
  // Permanently importing CA and server certs from device policy or giving such
  // certificates trust is not allowed. However, we make authority certificates
  // from device policy available (e.g. for use as intermediates in client
  // certificate discovery on the sign-in screen), see
  // GetAuthorityCertificates().
}

void DeviceNetworkConfigurationUpdater::ApplyNetworkPolicy(
    base::ListValue* network_configs_onc,
    base::DictionaryValue* global_network_config) {
  network_config_handler_->SetPolicy(onc_source_,
                                     std::string() /* no username hash */,
                                     *network_configs_onc,
                                     *global_network_config);
}

void DeviceNetworkConfigurationUpdater::OnDataRoamingSettingChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus trusted_status =
      cros_settings_->PrepareTrustedValues(base::Bind(
          &DeviceNetworkConfigurationUpdater::OnDataRoamingSettingChanged,
          weak_factory_.GetWeakPtr()));

  if (trusted_status == chromeos::CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    // Return, this function will be called again later by
    // PrepareTrustedValues.
    return;
  }

  bool data_roaming_setting = false;
  if (trusted_status == chromeos::CrosSettingsProvider::TRUSTED) {
    if (!cros_settings_->GetBoolean(chromeos::kSignedDataRoamingEnabled,
                                    &data_roaming_setting)) {
      LOG(ERROR) << "Couldn't get device setting "
                 << chromeos::kSignedDataRoamingEnabled;
      data_roaming_setting = false;
    }
  } else {
    DCHECK_EQ(trusted_status,
              chromeos::CrosSettingsProvider::PERMANENTLY_UNTRUSTED);
    // Roaming is disabled as we can't determine the correct setting.
  }

  network_device_handler_->SetCellularAllowRoaming(data_roaming_setting);
}

}  // namespace policy
