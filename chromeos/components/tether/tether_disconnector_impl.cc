// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_disconnector_impl.h"

#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/network_configuration_remover.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/components/tether/wifi_hotspot_disconnector.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

TetherDisconnectorImpl::TetherDisconnectorImpl(
    ActiveHost* active_host,
    WifiHotspotDisconnector* wifi_hotspot_disconnector,
    DisconnectTetheringRequestSender* disconnect_tethering_request_sender,
    TetherConnector* tether_connector,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
    TetherSessionCompletionLogger* tether_session_completion_logger)
    : active_host_(active_host),
      wifi_hotspot_disconnector_(wifi_hotspot_disconnector),
      disconnect_tethering_request_sender_(disconnect_tethering_request_sender),
      tether_connector_(tether_connector),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map),
      tether_session_completion_logger_(tether_session_completion_logger),
      weak_ptr_factory_(this) {}

TetherDisconnectorImpl::~TetherDisconnectorImpl() = default;

void TetherDisconnectorImpl::DisconnectFromNetwork(
    const std::string& tether_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback,
    const TetherSessionCompletionLogger::SessionCompletionReason&
        session_completion_reason) {
  DCHECK(!tether_network_guid.empty());

  ActiveHost::ActiveHostStatus status = active_host_->GetActiveHostStatus();
  std::string active_tether_network_guid = active_host_->GetTetherNetworkGuid();
  std::string active_wifi_network_guid = active_host_->GetWifiNetworkGuid();

  if (status == ActiveHost::ActiveHostStatus::DISCONNECTED) {
    PA_LOG(ERROR) << "Disconnect requested for Tether network with GUID "
                  << tether_network_guid << ", but no device is connected.";
    error_callback.Run(NetworkConnectionHandler::kErrorNotConnected);
    return;
  }

  if (tether_network_guid != active_tether_network_guid) {
    PA_LOG(ERROR) << "Disconnect requested for Tether network with GUID "
                  << tether_network_guid << ", but that device is not the "
                  << "active host.";
    error_callback.Run(NetworkConnectionHandler::kErrorNotConnected);
    return;
  }

  if (status == ActiveHost::ActiveHostStatus::CONNECTING) {
    // Note: CancelConnectionAttempt() disconnects the active host.
    if (tether_connector_->CancelConnectionAttempt(tether_network_guid)) {
      PA_LOG(INFO) << "Disconnect requested for Tether network with GUID "
                   << tether_network_guid << ", which had not yet connected. "
                   << "Canceled in-progress connection attempt.";
      success_callback.Run();
      return;
    }

    PA_LOG(ERROR) << "Disconnect requested for Tether network with GUID "
                  << tether_network_guid << " (not yet connected), but "
                  << "canceling connection attempt failed.";
    error_callback.Run(NetworkConnectionHandler::kErrorDisconnectFailed);
    return;
  }

  DCHECK(!active_wifi_network_guid.empty());
  DisconnectActiveWifiConnection(tether_network_guid, active_wifi_network_guid,
                                 success_callback, error_callback);

  tether_session_completion_logger_->RecordTetherSessionCompletion(
      session_completion_reason);
}

void TetherDisconnectorImpl::DisconnectActiveWifiConnection(
    const std::string& tether_network_guid,
    const std::string& wifi_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback) {
  // First, disconnect the active host so that the user gets visual indication
  // that the disconnection is in progress as quickly as possible.
  active_host_->SetActiveHostDisconnected();

  // Disconnect from the Wi-Fi hotspot. This transfers responsibility for
  // invoking the success or error callbacks to |wifi_hotspot_disconnector_|.
  wifi_hotspot_disconnector_->DisconnectFromWifiHotspot(
      wifi_network_guid, success_callback, error_callback);

  // In addition to disconnecting from the Wi-Fi network, this device must also
  // send a DisconnectTetheringRequest to the tether host so that it can shut
  // down its Wi-Fi hotspot if it is no longer in use.
  const std::string device_id =
      device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
          tether_network_guid);
  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      device_id);
}

}  // namespace tether

}  // namespace chromeos