// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_REFERENCE_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_REFERENCE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/common/service_worker/service_worker_object.mojom.h"

namespace content {

class ThreadSafeSender;

// Represents an interprocess reference to ServiceWorkerHandle managed in the
// browser process. The constructor and destructor sends a message to increment
// or decrement the reference count to the browser process.
class CONTENT_EXPORT ServiceWorkerHandleReference {
 public:
  // Creates a new ServiceWorkerHandleReference by adopting a ref-count. If
  // the handle id is kInvalidServiceWorkerHandleId, returns null instead.
  static std::unique_ptr<ServiceWorkerHandleReference> Adopt(
      blink::mojom::ServiceWorkerObjectInfoPtr info,
      scoped_refptr<ThreadSafeSender> sender);

  ~ServiceWorkerHandleReference();

  int handle_id() const { return info_->handle_id; }
  const GURL& url() const { return info_->url; }
  blink::mojom::ServiceWorkerState state() const { return info_->state; }
  int64_t version_id() const { return info_->version_id; }

 private:
  ServiceWorkerHandleReference(blink::mojom::ServiceWorkerObjectInfoPtr info,
                               scoped_refptr<ThreadSafeSender> sender);
  blink::mojom::ServiceWorkerObjectInfoPtr info_;
  scoped_refptr<ThreadSafeSender> sender_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandleReference);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_REFERENCE_H_
