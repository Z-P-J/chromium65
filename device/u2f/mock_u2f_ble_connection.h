// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_MOCK_U2F_BLE_CONNECTION_H_
#define DEVICE_U2F_MOCK_U2F_BLE_CONNECTION_H_

#include <string>
#include <vector>

#include "device/u2f/u2f_ble_connection.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockU2fBleConnection : public U2fBleConnection {
 public:
  explicit MockU2fBleConnection(std::string device_address);
  ~MockU2fBleConnection() override;

  MOCK_METHOD0(Connect, void());
  // GMock cannot mock a method taking a move-only type.
  // TODO(https://crbug.com/729950): Remove these workarounds once support for
  // move-only types is added to GMock.
  MOCK_METHOD1(ReadControlPointLengthPtr, void(ControlPointLengthCallback* cb));
  MOCK_METHOD1(ReadServiceRevisionsPtr, void(ServiceRevisionsCallback* cb));
  MOCK_METHOD2(WriteControlPointPtr,
               void(const std::vector<uint8_t>& data, WriteCallback* cb));
  MOCK_METHOD2(WriteServiceRevisionPtr,
               void(ServiceRevision service_revision, WriteCallback* cb));

  void ReadControlPointLength(ControlPointLengthCallback cb) override;
  void ReadServiceRevisions(ServiceRevisionsCallback cb) override;
  void WriteControlPoint(const std::vector<uint8_t>& data,
                         WriteCallback cb) override;
  void WriteServiceRevision(ServiceRevision service_revision,
                            WriteCallback cb) override;

  ConnectionStatusCallback& connection_status_callback() {
    return connection_status_callback_;
  }

  ReadCallback& read_callback() { return read_callback_; }

 private:
  ConnectionStatusCallback connection_status_callback_;
  ReadCallback read_callback_;
};

}  // namespace device

#endif  // DEVICE_U2F_MOCK_U2F_BLE_CONNECTION_H_
