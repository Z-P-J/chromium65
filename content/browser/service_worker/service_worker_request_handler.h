// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/service_worker_modes.h"
#include "net/url_request/url_request_job_factory.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "services/network/public/interfaces/request_context_frame_type.mojom.h"

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestInterceptor;
}

namespace network {
class ResourceRequestBody;
}

namespace storage {
class BlobStorageContext;
}

namespace content {

class ResourceContext;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class ServiceWorkerNavigationHandleCore;
class ServiceWorkerProviderHost;
class WebContents;

// Abstract base class for routing network requests to ServiceWorkers.
// Created one per URLRequest and attached to each request.
class CONTENT_EXPORT ServiceWorkerRequestHandler
    : public base::SupportsUserData::Data,
      public URLLoaderRequestHandler {
 public:
  // PlzNavigate
  // Attaches a newly created handler if the given |request| needs to be handled
  // by ServiceWorker.
  static void InitializeForNavigation(
      net::URLRequest* request,
      ServiceWorkerNavigationHandleCore* navigation_handle_core,
      storage::BlobStorageContext* blob_storage_context,
      bool skip_service_worker,
      ResourceType resource_type,
      RequestContextType request_context_type,
      network::mojom::RequestContextFrameType frame_type,
      bool is_parent_frame_secure,
      scoped_refptr<network::ResourceRequestBody> body,
      const base::Callback<WebContents*(void)>& web_contents_getter);

  // S13nServiceWorker:
  // Same as InitializeForNavigation() but instead of attaching to a URLRequest,
  // just creates a URLLoaderRequestHandler and returns it.
  static std::unique_ptr<URLLoaderRequestHandler>
  InitializeForNavigationNetworkService(
      const network::ResourceRequest& resource_request,
      ResourceContext* resource_context,
      ServiceWorkerNavigationHandleCore* navigation_handle_core,
      storage::BlobStorageContext* blob_storage_context,
      bool skip_service_worker,
      ResourceType resource_type,
      RequestContextType request_context_type,
      network::mojom::RequestContextFrameType frame_type,
      bool is_parent_frame_secure,
      scoped_refptr<network::ResourceRequestBody> body,
      const base::Callback<WebContents*(void)>& web_contents_getter);

  // Attaches a newly created handler if the given |request| needs to
  // be handled by ServiceWorker.
  // TODO(kinuko): While utilizing UserData to attach data to URLRequest
  // has some precedence, it might be better to attach this handler in a more
  // explicit way within content layer, e.g. have ResourceRequestInfoImpl
  // own it.
  static void InitializeHandler(
      net::URLRequest* request,
      ServiceWorkerContextWrapper* context_wrapper,
      storage::BlobStorageContext* blob_storage_context,
      int process_id,
      int provider_id,
      bool skip_service_worker,
      network::mojom::FetchRequestMode request_mode,
      network::mojom::FetchCredentialsMode credentials_mode,
      network::mojom::FetchRedirectMode redirect_mode,
      const std::string& integrity,
      bool keepalive,
      ResourceType resource_type,
      RequestContextType request_context_type,
      network::mojom::RequestContextFrameType frame_type,
      scoped_refptr<network::ResourceRequestBody> body);

  // Returns the handler attached to |request|. This may return NULL
  // if no handler is attached.
  static ServiceWorkerRequestHandler* GetHandler(
      const net::URLRequest* request);

  // Creates a protocol interceptor for ServiceWorker.
  static std::unique_ptr<net::URLRequestInterceptor> CreateInterceptor(
      ResourceContext* resource_context);

  // Returns true if the request falls into the scope of a ServiceWorker.
  // It's only reliable after the ServiceWorkerRequestHandler MaybeCreateJob
  // method runs to completion for this request. The AppCache handler uses
  // this to avoid colliding with ServiceWorkers.
  static bool IsControlledByServiceWorker(const net::URLRequest* request);

  // Returns the ServiceWorkerProviderHost the request is associated with.
  // Only valid after InitializeHandler has been called. Can return null.
  static ServiceWorkerProviderHost* GetProviderHost(
      const net::URLRequest* request);

  ~ServiceWorkerRequestHandler() override;

  // Called via custom URLRequestJobFactory.
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      ResourceContext* context) = 0;

  // URLLoaderRequestHandler overrides.
  void MaybeCreateLoader(const network::ResourceRequest& request,
                         ResourceContext* resource_context,
                         LoaderCallback callback) override;

  // These are obsolete, needed for non-PlzNavigate.
  // TODO(falken): Remove these completely.
  void PrepareForCrossSiteTransfer(int old_process_id);
  void CompleteCrossSiteTransfer(int new_process_id,
                                 int new_provider_id);
  void MaybeCompleteCrossSiteTransferInOldProcess(
      int old_process_id);

  // Useful for detecting storage partition mismatches in the context of cross
  // site transfer navigations.
  bool SanityCheckIsSameContext(ServiceWorkerContextWrapper* wrapper);

 protected:
  ServiceWorkerRequestHandler(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      ResourceType resource_type);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  ResourceType resource_type_;

 private:
  static int user_data_key_;  // Only address is used.

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
