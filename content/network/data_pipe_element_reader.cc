// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/data_pipe_element_reader.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "mojo/public/c/system/types.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace content {

DataPipeElementReader::DataPipeElementReader(
    scoped_refptr<network::ResourceRequestBody> resource_request_body,
    network::mojom::DataPipeGetterPtr data_pipe_getter_)
    : resource_request_body_(std::move(resource_request_body)),
      data_pipe_getter_(std::move(data_pipe_getter_)),
      handle_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      weak_factory_(this) {}

DataPipeElementReader::~DataPipeElementReader() {}

int DataPipeElementReader::Init(const net::CompletionCallback& callback) {
  DCHECK(callback);

  // Init rewinds the stream. Throw away current state.
  read_callback_.Reset();
  buf_ = nullptr;
  buf_length_ = 0;
  handle_watcher_.Cancel();
  size_ = 0;
  bytes_read_ = 0;
  // Need to do this to prevent any previously pending ReadCallback() invocation
  // from running.
  weak_factory_.InvalidateWeakPtrs();

  // Get a new data pipe and start.
  mojo::DataPipe data_pipe;
  data_pipe_getter_->Read(std::move(data_pipe.producer_handle),
                          base::BindOnce(&DataPipeElementReader::ReadCallback,
                                         weak_factory_.GetWeakPtr()));
  data_pipe_ = std::move(data_pipe.consumer_handle);
  handle_watcher_.Watch(
      data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(&DataPipeElementReader::OnHandleReadable,
                          base::Unretained(this)));

  init_callback_ = std::move(callback);
  return net::ERR_IO_PENDING;
}

uint64_t DataPipeElementReader::GetContentLength() const {
  return size_;
}

uint64_t DataPipeElementReader::BytesRemaining() const {
  return size_ - bytes_read_;
}

int DataPipeElementReader::Read(net::IOBuffer* buf,
                                int buf_length,
                                const net::CompletionCallback& callback) {
  DCHECK(callback);
  DCHECK(!read_callback_);
  DCHECK(!init_callback_);
  DCHECK(!buf_);

  int result = ReadInternal(buf, buf_length);
  if (result == net::ERR_IO_PENDING) {
    buf_ = buf;
    buf_length_ = buf_length;
    read_callback_ = std::move(callback);
  }
  return result;
}

void DataPipeElementReader::ReadCallback(int32_t status, uint64_t size) {
  if (status == net::OK)
    size_ = size;
  if (init_callback_)
    std::move(init_callback_).Run(status);
}

void DataPipeElementReader::OnHandleReadable(MojoResult result) {
  DCHECK(read_callback_);
  DCHECK(buf_);

  // Final result of the Read() call, to be passed to the consumer.
  int read_result;
  if (result == MOJO_RESULT_OK) {
    read_result = ReadInternal(buf_.get(), buf_length_);
  } else {
    read_result = net::ERR_FAILED;
  }

  buf_ = nullptr;
  buf_length_ = 0;

  if (read_result != net::ERR_IO_PENDING)
    std::move(read_callback_).Run(read_result);
}

int DataPipeElementReader::ReadInternal(net::IOBuffer* buf, int buf_length) {
  DCHECK(buf);
  DCHECK_GT(buf_length, 0);

  if (BytesRemaining() == 0)
    return net::OK;

  uint32_t num_bytes = buf_length;
  MojoResult rv =
      data_pipe_->ReadData(buf->data(), &num_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (rv == MOJO_RESULT_OK) {
    bytes_read_ += num_bytes;
    return num_bytes;
  }

  if (rv == MOJO_RESULT_SHOULD_WAIT) {
    handle_watcher_.ArmOrNotify();
    return net::ERR_IO_PENDING;
  }

  return net::ERR_FAILED;
}

}  // namespace content
