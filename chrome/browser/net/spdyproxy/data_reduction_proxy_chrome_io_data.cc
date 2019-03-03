// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_io_data.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/previews/previews_infobar_delegate.h"
#include "chrome/browser/previews/previews_infobar_tab_helper.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"
#include "components/data_reduction_proxy/content/browser/content_lofi_ui_service.h"
#include "components/data_reduction_proxy/content/browser/content_resource_type_provider.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/prefs/pref_service.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_experiments.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

using data_reduction_proxy::DataReductionProxyParams;

namespace {

// Adds the preview navigation to the black list.
void AddPreviewNavigationToBlackListCallback(
    content::BrowserContext* browser_context,
    const GURL& url,
    previews::PreviewsType type,
    uint64_t page_id,
    bool opt_out) {
  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (previews_service && previews_service->previews_ui_service()) {
    previews_service->previews_ui_service()->AddPreviewNavigation(
        url, type, opt_out, page_id);
  }
}

// If this is the first Lo-Fi response for a page load, a
// PreviewsInfoBarDelegate is created, which handles showing Lo-Fi UI.
void OnLoFiResponseReceivedOnUI(content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Retrieve PreviewsUIService* from |web_contents| if available.
  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  previews::PreviewsUIService* previews_ui_service =
      previews_service ? previews_service->previews_ui_service() : nullptr;

  PreviewsInfoBarTabHelper* infobar_tab_helper =
      PreviewsInfoBarTabHelper::FromWebContents(web_contents);

  uint64_t page_id = 0;
  if (infobar_tab_helper && infobar_tab_helper->previews_user_data()) {
    page_id = infobar_tab_helper->previews_user_data()->page_id();
  }

  PreviewsInfoBarDelegate::Create(
      web_contents, previews::PreviewsType::LOFI,
      base::Time() /* previews_freshness */, true /* is_data_saver_user */,
      false /* is_reload */,
      base::BindOnce(&AddPreviewNavigationToBlackListCallback,
                     web_contents->GetBrowserContext(),
                     web_contents->GetController()
                         .GetLastCommittedEntry()
                         ->GetRedirectChain()[0],
                     previews::PreviewsType::LOFI, page_id),
      previews_ui_service);
}

}  // namespace

std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
CreateDataReductionProxyChromeIOData(
    net::NetLog* net_log,
    PrefService* prefs,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner) {
  DCHECK(net_log);
  DCHECK(prefs);

  bool enabled =
      prefs->GetBoolean(prefs::kDataSaverEnabled) ||
      data_reduction_proxy::params::ShouldForceEnableDataReductionProxy();
  std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
      data_reduction_proxy_io_data(
          new data_reduction_proxy::DataReductionProxyIOData(
              DataReductionProxyChromeSettings::GetClient(), prefs, net_log,
              io_task_runner, ui_task_runner, enabled, GetUserAgent(),
              version_info::GetChannelString(chrome::GetChannel())));

  data_reduction_proxy_io_data->set_lofi_decider(
      base::MakeUnique<data_reduction_proxy::ContentLoFiDecider>());
  data_reduction_proxy_io_data->set_resource_type_provider(
      base::MakeUnique<data_reduction_proxy::ContentResourceTypeProvider>());
  data_reduction_proxy_io_data->set_lofi_ui_service(
      base::MakeUnique<data_reduction_proxy::ContentLoFiUIService>(
          ui_task_runner, base::Bind(&OnLoFiResponseReceivedOnUI)));

  return data_reduction_proxy_io_data;
}
