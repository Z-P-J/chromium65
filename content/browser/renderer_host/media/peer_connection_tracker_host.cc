// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"

#include "base/power_monitor/power_monitor.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/webrtc/webrtc_event_log_manager.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/common/media/peer_connection_tracker_messages.h"

namespace content {

PeerConnectionTrackerHost::PeerConnectionTrackerHost(int render_process_id)
    : BrowserMessageFilter(PeerConnectionTrackerMsgStart),
      render_process_id_(render_process_id) {}

bool PeerConnectionTrackerHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(PeerConnectionTrackerHost, message)
    IPC_MESSAGE_HANDLER(PeerConnectionTrackerHost_AddPeerConnection,
                        OnAddPeerConnection)
    IPC_MESSAGE_HANDLER(PeerConnectionTrackerHost_RemovePeerConnection,
                        OnRemovePeerConnection)
    IPC_MESSAGE_HANDLER(PeerConnectionTrackerHost_UpdatePeerConnection,
                        OnUpdatePeerConnection)
    IPC_MESSAGE_HANDLER(PeerConnectionTrackerHost_AddStats, OnAddStats)
    IPC_MESSAGE_HANDLER(PeerConnectionTrackerHost_GetUserMedia, OnGetUserMedia)
    IPC_MESSAGE_HANDLER(PeerConnectionTrackerHost_WebRtcEventLogWrite,
                        OnWebRtcEventLogWrite)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PeerConnectionTrackerHost::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == PeerConnectionTrackerMsgStart)
    *thread = BrowserThread::UI;
}

PeerConnectionTrackerHost::~PeerConnectionTrackerHost() {
}

void PeerConnectionTrackerHost::OnChannelConnected(int32_t peer_pid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Add PowerMonitor when connected to channel rather than in constructor due
  // to thread safety concerns. Observers of PowerMonitor must be added and
  // removed on the same thread. BrowserMessageFilter is created on the UI
  // thread but can be destructed on the UI or IO thread because they are
  // referenced by RenderProcessHostImpl on the UI thread and ChannelProxy on
  // the IO thread. Using OnChannelConnected and OnChannelClosing guarantees
  // execution on the IO thread.
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor)
    power_monitor->AddObserver(this);
}

void PeerConnectionTrackerHost::OnChannelClosing() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor)
    power_monitor->RemoveObserver(this);
}

void PeerConnectionTrackerHost::OnAddPeerConnection(
    const PeerConnectionInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals) {
    webrtc_internals->OnAddPeerConnection(
        render_process_id_, peer_pid(), info.lid, info.url,
        info.rtc_configuration, info.constraints);
  }
  WebRtcEventLogManager::GetInstance()->PeerConnectionAdded(render_process_id_,
                                                            info.lid);
}

void PeerConnectionTrackerHost::OnRemovePeerConnection(int lid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals) {
    webrtc_internals->OnRemovePeerConnection(peer_pid(), lid);
  }
  WebRtcEventLogManager::GetInstance()->PeerConnectionRemoved(
      render_process_id_, lid);
}

void PeerConnectionTrackerHost::OnUpdatePeerConnection(
    int lid, const std::string& type, const std::string& value) {
  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals) {
    webrtc_internals->OnUpdatePeerConnection(peer_pid(), lid, type, value);
  }
}

void PeerConnectionTrackerHost::OnAddStats(int lid,
                                           const base::ListValue& value) {
  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals) {
    webrtc_internals->OnAddStats(peer_pid(), lid, value);
  }
}

void PeerConnectionTrackerHost::OnGetUserMedia(
    const std::string& origin,
    bool audio,
    bool video,
    const std::string& audio_constraints,
    const std::string& video_constraints) {
  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals) {
    webrtc_internals->OnGetUserMedia(render_process_id_, peer_pid(), origin,
                                     audio, video, audio_constraints,
                                     video_constraints);
  }
}

void PeerConnectionTrackerHost::OnWebRtcEventLogWrite(
    int lid,
    const std::string& output) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* manager = WebRtcEventLogManager::GetInstance();
  manager->OnWebRtcEventLogWrite(render_process_id_, lid, output);
}

void PeerConnectionTrackerHost::OnSuspend() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&PeerConnectionTrackerHost::SendOnSuspendOnUIThread,
                     this));
}

void PeerConnectionTrackerHost::SendOnSuspendOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  if (host)
    host->Send(new PeerConnectionTracker_OnSuspend());
}

}  // namespace content
