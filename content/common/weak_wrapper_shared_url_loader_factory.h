// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WEAK_WRAPPER_SHARED_URL_LOADER_FACTORY_H_
#define CONTENT_COMMON_WEAK_WRAPPER_SHARED_URL_LOADER_FACTORY_H_

#include "content/common/content_export.h"
#include "content/public/common/shared_url_loader_factory.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

namespace content {

// A SharedURLLoaderFactory implementation that wraps a raw
// mojom::URLLoaderFactory pointer.
class CONTENT_EXPORT WeakWrapperSharedURLLoaderFactory
    : public SharedURLLoaderFactory {
 public:
  explicit WeakWrapperSharedURLLoaderFactory(
      network::mojom::URLLoaderFactory* factory_ptr);

  // Detaches from the raw mojom::URLLoaderFactory pointer. All subsequent calls
  // to CreateLoaderAndStart() will fail silently.
  void Detach();

  // SharedURLLoaderFactory implementation.
  void CreateLoaderAndStart(
      network::mojom::URLLoaderRequest loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const Constraints& constraints) override;

  std::unique_ptr<SharedURLLoaderFactoryInfo> Clone() override;

 private:
  ~WeakWrapperSharedURLLoaderFactory() override;

  // Not owned.
  network::mojom::URLLoaderFactory* factory_ptr_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_COMMON_WEAK_WRAPPER_URL_LOADER_FACTORY_H_
