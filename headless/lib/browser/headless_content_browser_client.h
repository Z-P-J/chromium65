// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_CONTENT_BROWSER_CLIENT_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_CONTENT_BROWSER_CLIENT_H_

#include "content/public/browser/content_browser_client.h"
#include "headless/lib/browser/headless_resource_dispatcher_host_delegate.h"
#include "headless/public/headless_browser.h"

namespace headless {

class HeadlessBrowserImpl;

class HeadlessContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit HeadlessContentBrowserClient(HeadlessBrowserImpl* browser);
  ~HeadlessContentBrowserClient() override;

  // content::ContentBrowserClient implementation:
  content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams&) override;
  void OverrideWebkitPrefs(content::RenderViewHost* render_view_host,
                           content::WebPreferences* prefs) override;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;
  std::unique_ptr<base::Value> GetServiceManifestOverlay(
      base::StringPiece name) override;
  void RegisterOutOfProcessServices(OutOfProcessServiceMap* services) override;
  content::QuotaPermissionContext* CreateQuotaPermissionContext() override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      storage::OptionalQuotaSettingsCallback callback) override;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;

  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      content::ResourceType resource_type,
      bool strict_enforcement,
      bool expired_previous_decision,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback) override;

  void ResourceDispatcherHostCreated() override;

  net::NetLog* GetNetLog() override;

  bool AllowGetCookie(const GURL& url,
                      const GURL& first_party,
                      const net::CookieList& cookie_list,
                      content::ResourceContext* context,
                      int render_process_id,
                      int render_frame_id) override;

  bool AllowSetCookie(const GURL& url,
                      const GURL& first_party,
                      const net::CanonicalCookie& cookie,
                      content::ResourceContext* context,
                      int render_process_id,
                      int render_frame_id,
                      const net::CookieOptions& options) override;

 private:
  std::unique_ptr<base::Value> GetBrowserServiceManifestOverlay();
  std::unique_ptr<base::Value> GetRendererServiceManifestOverlay();
  std::unique_ptr<base::Value> GetPackagedServicesServiceManifestOverlay();

  HeadlessBrowserImpl* browser_;  // Not owned.

  // We store the callback here because we may call it from the I/O thread.
  HeadlessBrowser::Options::AppendCommandLineFlagsCallback
      append_command_line_flags_callback_;

  std::unique_ptr<HeadlessResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessContentBrowserClient);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_CONTENT_BROWSER_CLIENT_H_
