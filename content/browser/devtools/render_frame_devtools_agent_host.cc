// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/render_frame_devtools_agent_host.h"

#include <tuple>
#include <utility>

#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_frame_trace_recorder.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/browser_handler.h"
#include "content/browser/devtools/protocol/dom_handler.h"
#include "content/browser/devtools/protocol/emulation_handler.h"
#include "content/browser/devtools/protocol/input_handler.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/schema_handler.h"
#include "content/browser/devtools/protocol/security_handler.h"
#include "content/browser/devtools/protocol/service_worker_handler.h"
#include "content/browser/devtools/protocol/storage_handler.h"
#include "content/browser/devtools/protocol/target_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "third_party/WebKit/common/associated_interfaces/associated_interface_provider.h"

#if defined(OS_ANDROID)
#include "content/public/browser/render_widget_host_view.h"
#include "services/device/public/interfaces/wake_lock_context.mojom.h"
#endif

namespace content {

namespace {
using RenderFrameDevToolsMap =
    std::map<FrameTreeNode*, RenderFrameDevToolsAgentHost*>;
base::LazyInstance<RenderFrameDevToolsMap>::Leaky g_agent_host_instances =
    LAZY_INSTANCE_INITIALIZER;

RenderFrameDevToolsAgentHost* FindAgentHost(FrameTreeNode* frame_tree_node) {
  if (!g_agent_host_instances.IsCreated())
    return nullptr;
  auto it = g_agent_host_instances.Get().find(frame_tree_node);
  return it == g_agent_host_instances.Get().end() ? nullptr : it->second;
}

bool ShouldCreateDevToolsForHost(RenderFrameHost* rfh) {
  return rfh->IsCrossProcessSubframe() || !rfh->GetParent();
}

bool ShouldCreateDevToolsForNode(FrameTreeNode* ftn) {
  return !ftn->parent() || ftn->current_frame_host()->IsCrossProcessSubframe();
}

FrameTreeNode* GetFrameTreeNodeAncestor(FrameTreeNode* frame_tree_node) {
  while (frame_tree_node && !ShouldCreateDevToolsForNode(frame_tree_node))
    frame_tree_node = frame_tree_node->parent();
  DCHECK(frame_tree_node);
  return frame_tree_node;
}

}  // namespace

// static
scoped_refptr<DevToolsAgentHost>
DevToolsAgentHost::GetOrCreateFor(WebContents* web_contents) {
  FrameTreeNode* node =
      static_cast<WebContentsImpl*>(web_contents)->GetFrameTree()->root();
  // TODO(dgozman): this check should not be necessary. See
  // http://crbug.com/489664.
  if (!node)
    return nullptr;
  return RenderFrameDevToolsAgentHost::GetOrCreateFor(node);
}

// static
scoped_refptr<DevToolsAgentHost> RenderFrameDevToolsAgentHost::GetOrCreateFor(
    FrameTreeNode* frame_tree_node) {
  frame_tree_node = GetFrameTreeNodeAncestor(frame_tree_node);
  RenderFrameDevToolsAgentHost* result = FindAgentHost(frame_tree_node);
  if (!result)
    result = new RenderFrameDevToolsAgentHost(frame_tree_node);
  return result;
}

// static
scoped_refptr<DevToolsAgentHost>
RenderFrameDevToolsAgentHost::GetOrCreateForDangling(
    FrameTreeNode* frame_tree_node) {
  // Note that this method does not use FrameTreeNode::current_frame_host(),
  // since it is used while the frame host may not be set as current yet,
  // for example right before commit time.
  // So the caller must be sure that passed frame will indeed be a correct
  // devtools target (see ShouldCreateDevToolsForNode above).
  RenderFrameDevToolsAgentHost* result = FindAgentHost(frame_tree_node);
  if (!result)
    result = new RenderFrameDevToolsAgentHost(frame_tree_node);
  return result;
}

// static
scoped_refptr<DevToolsAgentHost> RenderFrameDevToolsAgentHost::FindForDangling(
    FrameTreeNode* frame_tree_node) {
  return FindAgentHost(frame_tree_node);
}

// static
bool DevToolsAgentHost::HasFor(WebContents* web_contents) {
  FrameTreeNode* node =
      static_cast<WebContentsImpl*>(web_contents)->GetFrameTree()->root();
  return node ? FindAgentHost(node) != nullptr : false;
}

// static
bool DevToolsAgentHost::IsDebuggerAttached(WebContents* web_contents) {
  FrameTreeNode* node =
      static_cast<WebContentsImpl*>(web_contents)->GetFrameTree()->root();
  RenderFrameDevToolsAgentHost* host = node ? FindAgentHost(node) : nullptr;
  return host && host->IsAttached();
}

// static
void RenderFrameDevToolsAgentHost::AddAllAgentHosts(
    DevToolsAgentHost::List* result) {
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    for (FrameTreeNode* node : wc->GetFrameTree()->Nodes()) {
      if (!node->current_frame_host() || !ShouldCreateDevToolsForNode(node))
        continue;
      if (!node->current_frame_host()->IsRenderFrameLive())
        continue;
      result->push_back(RenderFrameDevToolsAgentHost::GetOrCreateFor(node));
    }
  }
}

// static
void RenderFrameDevToolsAgentHost::OnResetNavigationRequest(
    NavigationRequest* navigation_request) {
  RenderFrameDevToolsAgentHost* agent_host =
      FindAgentHost(navigation_request->frame_tree_node());
  if (!agent_host)
    return;
  if (navigation_request->net_error() != net::OK) {
    for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host))
      network->NavigationFailed(navigation_request);
  }
  for (auto* page : protocol::PageHandler::ForAgentHost(agent_host))
    page->NavigationReset(navigation_request);
}

// static
std::vector<std::unique_ptr<NavigationThrottle>>
RenderFrameDevToolsAgentHost::CreateNavigationThrottles(
    NavigationHandle* navigation_handle) {
  std::vector<std::unique_ptr<NavigationThrottle>> result;
  FrameTreeNode* frame_tree_node =
      static_cast<NavigationHandleImpl*>(navigation_handle)->frame_tree_node();

  // Interception might throttle navigations in inspected frames.
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(frame_tree_node);
  if (agent_host) {
    for (auto* network_handler :
         protocol::NetworkHandler::ForAgentHost(agent_host)) {
      std::unique_ptr<NavigationThrottle> throttle =
          network_handler->CreateThrottleForNavigation(navigation_handle);
      if (throttle)
        result.push_back(std::move(throttle));
    }
  }

  agent_host = nullptr;
  if (frame_tree_node->parent()) {
    // Target domain of the parent frame's DevTools may want to pause
    // this frame to do some setup.
    agent_host =
        FindAgentHost(GetFrameTreeNodeAncestor(frame_tree_node->parent()));
  }
  if (agent_host) {
    for (auto* target_handler :
         protocol::TargetHandler::ForAgentHost(agent_host)) {
      std::unique_ptr<NavigationThrottle> throttle =
          target_handler->CreateThrottleForNavigation(navigation_handle);
      if (throttle)
        result.push_back(std::move(throttle));
    }
  }

  return result;
}

// static
void RenderFrameDevToolsAgentHost::OnWillSendNavigationRequest(
    FrameTreeNode* frame_tree_node,
    mojom::BeginNavigationParams* begin_params,
    bool* report_raw_headers) {
  bool disable_cache = false;
  frame_tree_node = GetFrameTreeNodeAncestor(frame_tree_node);
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(frame_tree_node);
  if (!agent_host)
    return;
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params->headers);
  for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host)) {
    if (!network->enabled())
      continue;
    *report_raw_headers = true;
    network->WillSendNavigationRequest(
        &headers, &begin_params->skip_service_worker, &disable_cache);
  }
  if (disable_cache) {
    begin_params->load_flags &=
        ~(net::LOAD_VALIDATE_CACHE | net::LOAD_SKIP_CACHE_VALIDATION |
          net::LOAD_ONLY_FROM_CACHE | net::LOAD_DISABLE_CACHE);
    begin_params->load_flags |= net::LOAD_BYPASS_CACHE;
  }

  begin_params->headers = headers.ToString();
}

// static
bool RenderFrameDevToolsAgentHost::IsNetworkHandlerEnabled(
    FrameTreeNode* frame_tree_node) {
  frame_tree_node = GetFrameTreeNodeAncestor(frame_tree_node);
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(frame_tree_node);
  if (!agent_host)
    return false;
  for (auto* network : protocol::NetworkHandler::ForAgentHost(agent_host)) {
    if (network->enabled())
      return true;
  }
  return false;
}

// static
void RenderFrameDevToolsAgentHost::WebContentsCreated(
    WebContents* web_contents) {
  if (ShouldForceCreation()) {
    // Force agent host.
    DevToolsAgentHost::GetOrCreateFor(web_contents);
  }
}

RenderFrameDevToolsAgentHost::RenderFrameDevToolsAgentHost(
    FrameTreeNode* frame_tree_node)
    : DevToolsAgentHostImpl(frame_tree_node->devtools_frame_token().ToString()),
      frame_tree_node_(nullptr) {
  SetFrameTreeNode(frame_tree_node);
  frame_host_ = frame_tree_node->current_frame_host();
  render_frame_alive_ = frame_host_ && frame_host_->IsRenderFrameLive();
  AddRef();  // Balanced in DestroyOnRenderFrameGone.
  NotifyCreated();
}

void RenderFrameDevToolsAgentHost::SetFrameTreeNode(
    FrameTreeNode* frame_tree_node) {
  if (frame_tree_node_)
    g_agent_host_instances.Get().erase(frame_tree_node_);
  frame_tree_node_ = frame_tree_node;
  if (frame_tree_node_) {
    // TODO(dgozman): with ConnectWebContents/DisconnectWebContents,
    // we may get two agent hosts for the same FrameTreeNode.
    // That is definitely a bug, and we should fix that, and DCHECK
    // here that there is no other agent host.
    g_agent_host_instances.Get()[frame_tree_node] = this;
  }
  WebContentsObserver::Observe(
      frame_tree_node_ ? WebContentsImpl::FromFrameTreeNode(frame_tree_node_)
                       : nullptr);
}

BrowserContext* RenderFrameDevToolsAgentHost::GetBrowserContext() {
  WebContents* contents = web_contents();
  return contents ? contents->GetBrowserContext() : nullptr;
}

WebContents* RenderFrameDevToolsAgentHost::GetWebContents() {
  return web_contents();
}

void RenderFrameDevToolsAgentHost::AttachSession(DevToolsSession* session) {
  session->SetFallThroughForNotFound(true);
  session->SetRenderer(frame_host_ ? frame_host_->GetProcess() : nullptr,
                       frame_host_);

  protocol::EmulationHandler* emulation_handler =
      new protocol::EmulationHandler();
  session->AddHandler(base::WrapUnique(new protocol::BrowserHandler()));
  session->AddHandler(base::WrapUnique(new protocol::DOMHandler()));
  session->AddHandler(base::WrapUnique(emulation_handler));
  session->AddHandler(base::WrapUnique(new protocol::InputHandler()));
  session->AddHandler(base::WrapUnique(new protocol::InspectorHandler()));
  session->AddHandler(base::WrapUnique(new protocol::IOHandler(
      GetIOContext())));
  session->AddHandler(base::WrapUnique(new protocol::NetworkHandler(GetId())));
  session->AddHandler(base::WrapUnique(new protocol::SchemaHandler()));
  session->AddHandler(base::WrapUnique(new protocol::ServiceWorkerHandler()));
  session->AddHandler(base::WrapUnique(new protocol::StorageHandler()));
  session->AddHandler(base::WrapUnique(new protocol::TargetHandler()));
  session->AddHandler(base::WrapUnique(new protocol::TracingHandler(
      protocol::TracingHandler::Renderer,
      frame_tree_node_ ? frame_tree_node_->frame_tree_node_id() : 0,
      GetIOContext())));
  if (frame_tree_node_ && !frame_tree_node_->parent()) {
    session->AddHandler(
        base::WrapUnique(new protocol::PageHandler(emulation_handler)));
    session->AddHandler(base::WrapUnique(new protocol::SecurityHandler()));
  }

  if (EnsureAgent())
    session->AttachToAgent(agent_ptr_);

  if (sessions().size() == 1) {
    frame_trace_recorder_.reset(new DevToolsFrameTraceRecorder());
    GrantPolicy();
#if defined(OS_ANDROID)
    GetWakeLock()->RequestWakeLock();
#endif
  }
}

void RenderFrameDevToolsAgentHost::DetachSession(DevToolsSession* session) {
  // Destroying session automatically detaches in renderer.
  suspended_messages_by_session_.erase(session);
  if (sessions().empty()) {
    frame_trace_recorder_.reset();
    RevokePolicy();
#if defined(OS_ANDROID)
    GetWakeLock()->CancelWakeLock();
#endif
  }
}

bool RenderFrameDevToolsAgentHost::DispatchProtocolMessage(
    DevToolsSession* session,
    const std::string& message) {
  int call_id = 0;
  std::string method;
  if (session->Dispatch(message, &call_id, &method) !=
      protocol::Response::kFallThrough) {
    return true;
  }

  if (!navigation_handles_.empty()) {
    suspended_messages_by_session_[session].push_back(
        {call_id, method, message});
    return true;
  }

  session->DispatchProtocolMessageToAgent(call_id, method, message);
  session->waiting_messages()[call_id] = {method, message};
  return true;
}

void RenderFrameDevToolsAgentHost::InspectElement(
    DevToolsSession* session,
    int x,
    int y) {
  // When there are nested WebContents, the renderer expects coordinates
  // relative to the main frame, so we need to transform the coordinates from
  // the root space to the current WebContents' main frame space.

  if (frame_tree_node_) {
    if (auto* main_view =
            frame_tree_node_->frame_tree()->GetMainFrame()->GetView()) {
      gfx::Point transformed_point = gfx::ToRoundedPoint(
          main_view->TransformRootPointToViewCoordSpace(gfx::PointF(x, y)));
      x = transformed_point.x();
      y = transformed_point.y();
    }
  }

  session->InspectElement(gfx::Point(x, y));
}

RenderFrameDevToolsAgentHost::~RenderFrameDevToolsAgentHost() {
  SetFrameTreeNode(nullptr);
}

void RenderFrameDevToolsAgentHost::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  NavigationHandleImpl* handle =
      static_cast<NavigationHandleImpl*>(navigation_handle);
  if (handle->frame_tree_node() != frame_tree_node_)
    return;

  UpdateFrameHost(handle->GetRenderFrameHost());
  // UpdateFrameHost may destruct |this|.
}

void RenderFrameDevToolsAgentHost::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  NavigationHandleImpl* handle =
      static_cast<NavigationHandleImpl*>(navigation_handle);
  if (handle->frame_tree_node() != frame_tree_node_)
    return;
  navigation_handles_.erase(handle);
  NotifyNavigated();

  // UpdateFrameHost may destruct |this|.
  scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);
  UpdateFrameHost(frame_tree_node_->current_frame_host());

  if (navigation_handles_.empty()) {
    for (auto& pair : suspended_messages_by_session_) {
      DevToolsSession* session = pair.first;
      for (const Message& message : pair.second) {
        session->DispatchProtocolMessageToAgent(message.call_id, message.method,
                                                message.message);
        session->waiting_messages()[message.call_id] = {message.method,
                                                        message.message};
      }
    }
    suspended_messages_by_session_.clear();
  }
  if (handle->HasCommitted()) {
    for (auto* target : protocol::TargetHandler::ForAgentHost(this))
      target->DidCommitNavigation();
  }
}

void RenderFrameDevToolsAgentHost::UpdateFrameHost(
    RenderFrameHostImpl* frame_host) {
  if (frame_host == frame_host_) {
    if (frame_host && !render_frame_alive_) {
      render_frame_alive_ = true;
      MaybeReattachToRenderFrame();
    }
    return;
  }

  if (frame_host && !ShouldCreateDevToolsForHost(frame_host)) {
    DestroyOnRenderFrameGone();
    // |this| may be deleted at this point.
    return;
  }

  if (IsAttached())
    RevokePolicy();
  frame_host_ = frame_host;
  agent_ptr_.reset();
  render_frame_alive_ = true;
  if (IsAttached()) {
    GrantPolicy();
    for (DevToolsSession* session : sessions()) {
      session->SetRenderer(frame_host ? frame_host->GetProcess() : nullptr,
                           frame_host);
    }
    MaybeReattachToRenderFrame();
  }
}

void RenderFrameDevToolsAgentHost::MaybeReattachToRenderFrame() {
  if (!EnsureAgent())
    return;
  for (DevToolsSession* session : sessions()) {
    session->ReattachToAgent(agent_ptr_);
    for (const auto& pair : session->waiting_messages()) {
      int call_id = pair.first;
      const DevToolsSession::Message& message = pair.second;
      session->DispatchProtocolMessageToAgent(call_id, message.method,
                                              message.message);
    }
  }
}

void RenderFrameDevToolsAgentHost::GrantPolicy() {
  if (!frame_host_)
    return;
  uint32_t process_id = frame_host_->GetProcess()->GetID();
  if (base::FeatureList::IsEnabled(features::kNetworkService))
    GetNetworkService()->SetRawHeadersAccess(process_id, true);
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadRawCookies(
      process_id);
}

void RenderFrameDevToolsAgentHost::RevokePolicy() {
  if (!frame_host_)
    return;

  bool process_has_agents = false;
  RenderProcessHost* process_host = frame_host_->GetProcess();
  for (const auto& ftn_agent : g_agent_host_instances.Get()) {
    RenderFrameDevToolsAgentHost* agent = ftn_agent.second;
    if (!agent->IsAttached())
      continue;
    if (agent->frame_host_ && agent->frame_host_ != frame_host_ &&
        agent->frame_host_->GetProcess() == process_host) {
      process_has_agents = true;
    }
  }

  // We are the last to disconnect from the renderer -> revoke permissions.
  if (!process_has_agents) {
    if (base::FeatureList::IsEnabled(features::kNetworkService))
      GetNetworkService()->SetRawHeadersAccess(process_host->GetID(), false);
    ChildProcessSecurityPolicyImpl::GetInstance()->RevokeReadRawCookies(
        process_host->GetID());
  }
}

void RenderFrameDevToolsAgentHost::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  NavigationHandleImpl* handle =
      static_cast<NavigationHandleImpl*>(navigation_handle);
  if (handle->frame_tree_node() != frame_tree_node_)
    return;
  navigation_handles_.insert(handle);
}

void RenderFrameDevToolsAgentHost::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  if (old_host != frame_host_)
    return;

  UpdateFrameHost(nullptr);
  // UpdateFrameHost may destruct |this|.
}

void RenderFrameDevToolsAgentHost::FrameDeleted(RenderFrameHost* rfh) {
  if (static_cast<RenderFrameHostImpl*>(rfh)->frame_tree_node() ==
      frame_tree_node_) {
    DestroyOnRenderFrameGone();  // |this| may be deleted at this point.
  }
}

void RenderFrameDevToolsAgentHost::RenderFrameDeleted(RenderFrameHost* rfh) {
  if (rfh == frame_host_) {
    render_frame_alive_ = false;
    agent_ptr_.reset();
  }
}

void RenderFrameDevToolsAgentHost::DestroyOnRenderFrameGone() {
  scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);
  if (IsAttached())
    RevokePolicy();
  ForceDetachAllClients();
  frame_host_ = nullptr;
  agent_ptr_.reset();
  SetFrameTreeNode(nullptr);
  Release();
}

#if defined(OS_ANDROID)
device::mojom::WakeLock* RenderFrameDevToolsAgentHost::GetWakeLock() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!wake_lock_) {
    device::mojom::WakeLockRequest request = mojo::MakeRequest(&wake_lock_);
    device::mojom::WakeLockContext* wake_lock_context =
        web_contents()->GetWakeLockContext();
    if (wake_lock_context) {
      wake_lock_context->GetWakeLock(
          device::mojom::WakeLockType::kPreventDisplaySleep,
          device::mojom::WakeLockReason::kOther, "DevTools",
          std::move(request));
    }
  }
  return wake_lock_.get();
}
#endif

void RenderFrameDevToolsAgentHost::RenderProcessGone(
    base::TerminationStatus status) {
  switch(status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
        inspector->TargetCrashed();
      break;
    default:
      for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
        inspector->TargetDetached("Render process gone.");
      break;
  }
}

void RenderFrameDevToolsAgentHost::DidAttachInterstitialPage() {
  for (auto* page : protocol::PageHandler::ForAgentHost(this))
    page->DidAttachInterstitialPage();
}

void RenderFrameDevToolsAgentHost::DidDetachInterstitialPage() {
  for (auto* page : protocol::PageHandler::ForAgentHost(this))
    page->DidDetachInterstitialPage();
}

void RenderFrameDevToolsAgentHost::WasShown() {
#if defined(OS_ANDROID)
  GetWakeLock()->RequestWakeLock();
#endif
}

void RenderFrameDevToolsAgentHost::WasHidden() {
#if defined(OS_ANDROID)
  GetWakeLock()->CancelWakeLock();
#endif
}

void RenderFrameDevToolsAgentHost::DidReceiveCompositorFrame() {
  const viz::CompositorFrameMetadata& metadata =
      RenderWidgetHostImpl::From(
          web_contents()->GetRenderViewHost()->GetWidget())
          ->last_frame_metadata();
  for (auto* page : protocol::PageHandler::ForAgentHost(this))
    page->OnSwapCompositorFrame(metadata.Clone());
  for (auto* input : protocol::InputHandler::ForAgentHost(this))
    input->OnSwapCompositorFrame(metadata);

  if (!frame_trace_recorder_)
    return;
  bool did_initiate_recording = false;
  for (auto* tracing : protocol::TracingHandler::ForAgentHost(this))
    did_initiate_recording |= tracing->did_initiate_recording();
  if (did_initiate_recording)
    frame_trace_recorder_->OnSwapCompositorFrame(frame_host_, metadata);
}

void RenderFrameDevToolsAgentHost::DisconnectWebContents() {
  navigation_handles_.clear();
  SetFrameTreeNode(nullptr);
  UpdateFrameHost(nullptr);
  // UpdateFrameHost may destruct |this|.
}

void RenderFrameDevToolsAgentHost::ConnectWebContents(WebContents* wc) {
  RenderFrameHostImpl* host =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
  DCHECK(host);
  SetFrameTreeNode(host->frame_tree_node());
  UpdateFrameHost(host);
  // UpdateFrameHost may destruct |this|.
}

std::string RenderFrameDevToolsAgentHost::GetParentId() {
  if (IsChildFrame()) {
    FrameTreeNode* frame_tree_node =
        GetFrameTreeNodeAncestor(frame_tree_node_->parent());
    return RenderFrameDevToolsAgentHost::GetOrCreateFor(frame_tree_node)
        ->GetId();
  }

  WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents());
  if (!contents)
    return "";
  contents = contents->GetOuterWebContents();
  if (contents)
    return DevToolsAgentHost::GetOrCreateFor(contents)->GetId();
  return "";
}

std::string RenderFrameDevToolsAgentHost::GetOpenerId() {
  if (!frame_tree_node_)
    return std::string();
  FrameTreeNode* opener = frame_tree_node_->original_opener();
  return opener ? opener->devtools_frame_token().ToString() : std::string();
}

std::string RenderFrameDevToolsAgentHost::GetType() {
  if (web_contents() &&
      static_cast<WebContentsImpl*>(web_contents())->GetOuterWebContents()) {
    return kTypeGuest;
  }
  if (IsChildFrame())
    return kTypeFrame;
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate() && web_contents()) {
    std::string type = manager->delegate()->GetTargetType(web_contents());
    if (!type.empty())
      return type;
  }
  return kTypePage;
}

std::string RenderFrameDevToolsAgentHost::GetTitle() {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate() && web_contents()) {
    std::string title = manager->delegate()->GetTargetTitle(web_contents());
    if (!title.empty())
      return title;
  }
  if (IsChildFrame() && frame_host_)
    return frame_host_->GetLastCommittedURL().spec();
  if (web_contents())
    return base::UTF16ToUTF8(web_contents()->GetTitle());
  return GetURL().spec();
}

std::string RenderFrameDevToolsAgentHost::GetDescription() {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate() && web_contents())
    return manager->delegate()->GetTargetDescription(web_contents());
  return std::string();
}

GURL RenderFrameDevToolsAgentHost::GetURL() {
  // Order is important here.
  WebContents* web_contents = GetWebContents();
  if (web_contents && !IsChildFrame())
    return web_contents->GetVisibleURL();
  if (frame_host_)
    return frame_host_->GetLastCommittedURL();
  return GURL();
}

GURL RenderFrameDevToolsAgentHost::GetFaviconURL() {
  WebContents* wc = web_contents();
  if (!wc)
    return GURL();
  NavigationEntry* entry = wc->GetController().GetLastCommittedEntry();
  if (entry)
    return entry->GetFavicon().url;
  return GURL();
}

bool RenderFrameDevToolsAgentHost::Activate() {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(web_contents());
  if (wc) {
    wc->Activate();
    return true;
  }
  return false;
}

void RenderFrameDevToolsAgentHost::Reload() {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(web_contents());
  if (wc)
    wc->GetController().Reload(ReloadType::NORMAL, true);
}

bool RenderFrameDevToolsAgentHost::Close() {
  if (web_contents()) {
    web_contents()->ClosePage();
    return true;
  }
  return false;
}

base::TimeTicks RenderFrameDevToolsAgentHost::GetLastActivityTime() {
  if (WebContents* contents = web_contents())
    return contents->GetLastActiveTime();
  return base::TimeTicks();
}

void RenderFrameDevToolsAgentHost::SignalSynchronousSwapCompositorFrame(
    RenderFrameHost* frame_host,
    viz::CompositorFrameMetadata frame_metadata) {
  scoped_refptr<RenderFrameDevToolsAgentHost> dtah(FindAgentHost(
      static_cast<RenderFrameHostImpl*>(frame_host)->frame_tree_node()));
  if (dtah) {
    // Unblock the compositor.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &RenderFrameDevToolsAgentHost::SynchronousSwapCompositorFrame,
            dtah.get(), base::Passed(std::move(frame_metadata))));
  }
}

void RenderFrameDevToolsAgentHost::SynchronousSwapCompositorFrame(
    viz::CompositorFrameMetadata frame_metadata) {
  for (auto* page : protocol::PageHandler::ForAgentHost(this))
    page->OnSynchronousSwapCompositorFrame(frame_metadata.Clone());
  for (auto* input : protocol::InputHandler::ForAgentHost(this))
    input->OnSwapCompositorFrame(frame_metadata);

  if (!frame_trace_recorder_)
    return;
  bool did_initiate_recording = false;
  for (auto* tracing : protocol::TracingHandler::ForAgentHost(this))
    did_initiate_recording |= tracing->did_initiate_recording();
  if (did_initiate_recording) {
    frame_trace_recorder_->OnSynchronousSwapCompositorFrame(frame_host_,
                                                            frame_metadata);
  }
}

bool RenderFrameDevToolsAgentHost::EnsureAgent() {
  if (!frame_host_ || !render_frame_alive_)
    return false;
  if (!agent_ptr_)
    frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(&agent_ptr_);
  return true;
}

bool RenderFrameDevToolsAgentHost::IsChildFrame() {
  return frame_tree_node_ && frame_tree_node_->parent();
}

}  // namespace content
