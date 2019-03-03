// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/url_loader_throttle_provider_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/message_loop/message_loop.h"
#include "chrome/common/prerender.mojom.h"
#include "chrome/common/prerender_url_loader_throttle.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/prerender/prerender_dispatcher.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "components/safe_browsing/renderer/renderer_url_loader_throttle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {

chrome::mojom::PrerenderCanceler* GetPrerenderCanceller(int render_frame_id) {
  content::RenderFrame* render_frame =
      content::RenderFrame::FromRoutingID(render_frame_id);
  if (!render_frame)
    return nullptr;
  prerender::PrerenderHelper* helper =
      prerender::PrerenderHelper::Get(render_frame);
  if (!helper)
    return nullptr;

  auto* canceler = new chrome::mojom::PrerenderCancelerPtr;
  render_frame->GetRemoteInterfaces()->GetInterface(canceler);
  base::MessageLoop::current()->task_runner()->DeleteSoon(FROM_HERE, canceler);
  return canceler->get();
}

}  // namespace

URLLoaderThrottleProviderImpl::URLLoaderThrottleProviderImpl(
    content::URLLoaderThrottleProviderType type,
    ChromeContentRendererClient* chrome_content_renderer_client)
    : type_(type),
      chrome_content_renderer_client_(chrome_content_renderer_client) {
  DETACH_FROM_THREAD(thread_checker_);

  if (base::FeatureList::IsEnabled(features::kNetworkService)) {
    content::RenderThread::Get()->GetConnector()->BindInterface(
        content::mojom::kBrowserServiceName,
        mojo::MakeRequest(&safe_browsing_info_));
  }
}

URLLoaderThrottleProviderImpl::~URLLoaderThrottleProviderImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

std::vector<std::unique_ptr<content::URLLoaderThrottle>>
URLLoaderThrottleProviderImpl::CreateThrottles(
    int render_frame_id,
    const blink::WebURL& url,
    content::ResourceType resource_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<std::unique_ptr<content::URLLoaderThrottle>> throttles;

  bool network_service_enabled =
      base::FeatureList::IsEnabled(features::kNetworkService);
  // Some throttles have already been added in the browser for frame resources.
  // Don't add them for frame requests.
  bool is_frame_resource = content::IsResourceTypeFrame(resource_type);

  DCHECK(!is_frame_resource ||
         type_ == content::URLLoaderThrottleProviderType::kFrame);

  if (network_service_enabled && !is_frame_resource) {
    if (safe_browsing_info_)
      safe_browsing_.Bind(std::move(safe_browsing_info_));
    throttles.push_back(
        std::make_unique<safe_browsing::RendererURLLoaderThrottle>(
            safe_browsing_.get(), render_frame_id));
  }

  if (type_ == content::URLLoaderThrottleProviderType::kFrame &&
      !is_frame_resource) {
    content::RenderFrame* render_frame =
        content::RenderFrame::FromRoutingID(render_frame_id);
    auto* prerender_helper =
        render_frame ? prerender::PrerenderHelper::Get(
                           render_frame->GetRenderView()->GetMainRenderFrame())
                     : nullptr;
    if (prerender_helper) {
      auto throttle = std::make_unique<prerender::PrerenderURLLoaderThrottle>(
          prerender_helper->prerender_mode(),
          prerender_helper->histogram_prefix(),
          base::BindOnce(GetPrerenderCanceller, render_frame_id),
          base::MessageLoop::current()->task_runner());
      prerender_helper->AddThrottle(throttle->AsWeakPtr());
      if (prerender_helper->prerender_mode() == prerender::PREFETCH_ONLY) {
        auto* prerender_dispatcher =
            chrome_content_renderer_client_->prerender_dispatcher();
        prerender_dispatcher->IncrementPrefetchCount();
        throttle->set_destruction_closure(base::BindOnce(
            &prerender::PrerenderDispatcher::DecrementPrefetchCount,
            base::Unretained(prerender_dispatcher)));
      }
      throttles.push_back(std::move(throttle));
    }
  }

  return throttles;
}
