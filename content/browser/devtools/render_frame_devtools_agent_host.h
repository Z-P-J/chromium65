// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/web/devtools_agent.mojom.h"

#if defined(OS_ANDROID)
#include "services/device/public/interfaces/wake_lock.mojom.h"
#include "ui/android/view_android.h"
#endif  // OS_ANDROID

namespace viz {
class CompositorFrameMetadata;
}

namespace content {

class BrowserContext;
class DevToolsFrameTraceRecorder;
class FrameTreeNode;
class NavigationHandle;
class NavigationHandleImpl;
class NavigationRequest;
class NavigationThrottle;
class RenderFrameHostImpl;

class CONTENT_EXPORT RenderFrameDevToolsAgentHost
    : public DevToolsAgentHostImpl,
      private WebContentsObserver {
 public:
  static void AddAllAgentHosts(DevToolsAgentHost::List* result);
  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(
      FrameTreeNode* frame_tree_node);

  // This method does not climb up to the suitable parent frame,
  // so only use it when we are sure the frame will be a local root.
  // Prefer GetOrCreateFor instead.
  static scoped_refptr<DevToolsAgentHost> GetOrCreateForDangling(
      FrameTreeNode* frame_tree_node);
  static scoped_refptr<DevToolsAgentHost> FindForDangling(
      FrameTreeNode* frame_tree_node);

  static void OnWillSendNavigationRequest(
      FrameTreeNode* frame_tree_node,
      mojom::BeginNavigationParams* begin_params,
      bool* report_raw_headers);

  static void OnResetNavigationRequest(NavigationRequest* navigation_request);

  static std::vector<std::unique_ptr<NavigationThrottle>>
  CreateNavigationThrottles(NavigationHandle* navigation_handle);
  static bool IsNetworkHandlerEnabled(FrameTreeNode* frame_tree_node);
  static void WebContentsCreated(WebContents* web_contents);

  static void SignalSynchronousSwapCompositorFrame(
      RenderFrameHost* frame_host,
      viz::CompositorFrameMetadata frame_metadata);

  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  // DevToolsAgentHost overrides.
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* web_contents) override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  std::string GetParentId() override;
  std::string GetOpenerId() override;
  std::string GetType() override;
  std::string GetTitle() override;
  std::string GetDescription() override;
  GURL GetURL() override;
  GURL GetFaviconURL() override;
  bool Activate() override;
  void Reload() override;

  bool Close() override;
  base::TimeTicks GetLastActivityTime() override;

  RenderFrameHostImpl* GetFrameHostForTesting() { return frame_host_; }

 private:
  friend class DevToolsAgentHost;
  explicit RenderFrameDevToolsAgentHost(FrameTreeNode*);
  ~RenderFrameDevToolsAgentHost() override;

  // DevToolsAgentHostImpl overrides.
  void AttachSession(DevToolsSession* session) override;
  void DetachSession(DevToolsSession* session) override;
  void InspectElement(DevToolsSession* session, int x, int y) override;
  bool DispatchProtocolMessage(
      DevToolsSession* session,
      const std::string& message) override;

  // WebContentsObserver overrides.
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(RenderFrameHost* rfh) override;
  void RenderFrameDeleted(RenderFrameHost* rfh) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;
  void WasShown() override;
  void WasHidden() override;
  void DidReceiveCompositorFrame() override;

  bool IsChildFrame();

  void OnSwapCompositorFrame(const IPC::Message& message);
  void DestroyOnRenderFrameGone();
  void UpdateFrameHost(RenderFrameHostImpl* frame_host);
  void MaybeReattachToRenderFrame();
  void GrantPolicy();
  void RevokePolicy();
  void SetFrameTreeNode(FrameTreeNode* frame_tree_node);

#if defined(OS_ANDROID)
  device::mojom::WakeLock* GetWakeLock();
#endif

  void SynchronousSwapCompositorFrame(
      viz::CompositorFrameMetadata frame_metadata);
  bool EnsureAgent();

  std::unique_ptr<DevToolsFrameTraceRecorder> frame_trace_recorder_;
#if defined(OS_ANDROID)
  device::mojom::WakeLockPtr wake_lock_;
#endif

  // The active host we are talking to.
  RenderFrameHostImpl* frame_host_ = nullptr;
  blink::mojom::DevToolsAgentAssociatedPtr agent_ptr_;
  base::flat_set<NavigationHandleImpl*> navigation_handles_;
  bool render_frame_alive_ = false;

  // These messages were queued after suspending, not sent to the agent,
  // and will be sent after resuming.
  struct Message {
    int call_id;
    std::string method;
    std::string message;
  };
  std::map<DevToolsSession*, std::vector<Message>>
      suspended_messages_by_session_;

  // The FrameTreeNode associated with this agent.
  FrameTreeNode* frame_tree_node_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_RENDER_FRAME_DEVTOOLS_AGENT_HOST_H_
