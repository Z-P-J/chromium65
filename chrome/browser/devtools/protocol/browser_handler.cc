// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/browser_handler.h"

#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"

namespace {

BrowserWindow* GetBrowserWindow(int window_id) {
  for (auto* b : *BrowserList::GetInstance()) {
    if (b->session_id().id() == window_id)
      return b->window();
  }
  return nullptr;
}

std::unique_ptr<protocol::Browser::Bounds> GetBrowserWindowBounds(
    BrowserWindow* window) {
  std::string window_state = "normal";
  if (window->IsMinimized())
    window_state = "minimized";
  if (window->IsMaximized())
    window_state = "maximized";
  if (window->IsFullscreen())
    window_state = "fullscreen";

  gfx::Rect bounds;
  if (window->IsMinimized())
    bounds = window->GetRestoredBounds();
  else
    bounds = window->GetBounds();
  return protocol::Browser::Bounds::Create()
      .SetLeft(bounds.x())
      .SetTop(bounds.y())
      .SetWidth(bounds.width())
      .SetHeight(bounds.height())
      .SetWindowState(window_state)
      .Build();
}

}  // namespace

BrowserHandler::BrowserHandler(protocol::UberDispatcher* dispatcher) {
  // Dispatcher can be null in tests.
  if (dispatcher)
    protocol::Browser::Dispatcher::wire(dispatcher, this);
}

BrowserHandler::~BrowserHandler() = default;

protocol::Response BrowserHandler::GetWindowForTarget(
    const std::string& target_id,
    int* out_window_id,
    std::unique_ptr<protocol::Browser::Bounds>* out_bounds) {
  auto host = content::DevToolsAgentHost::GetForId(target_id);
  if (!host)
    return protocol::Response::Error("No target with given id");
  content::WebContents* web_contents = host->GetWebContents();
  if (!web_contents)
    return protocol::Response::Error("No web contents in the target");

  Browser* browser = nullptr;
  for (auto* b : *BrowserList::GetInstance()) {
    int tab_index = b->tab_strip_model()->GetIndexOfWebContents(web_contents);
    if (tab_index != TabStripModel::kNoTab)
      browser = b;
  }
  if (!browser)
    return protocol::Response::Error("Browser window not found");

  BrowserWindow* window = browser->window();
  *out_window_id = browser->session_id().id();
  *out_bounds = GetBrowserWindowBounds(window);
  return protocol::Response::OK();
}

protocol::Response BrowserHandler::GetWindowBounds(
    int window_id,
    std::unique_ptr<protocol::Browser::Bounds>* out_bounds) {
  BrowserWindow* window = GetBrowserWindow(window_id);
  if (!window)
    return protocol::Response::Error("Browser window not found");

  *out_bounds = GetBrowserWindowBounds(window);
  return protocol::Response::OK();
}

protocol::Response BrowserHandler::Close() {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce([]() { chrome::AttemptExit(); }));
  return protocol::Response::OK();
}

protocol::Response BrowserHandler::SetWindowBounds(
    int window_id,
    std::unique_ptr<protocol::Browser::Bounds> window_bounds) {
  BrowserWindow* window = GetBrowserWindow(window_id);
  if (!window)
    return protocol::Response::Error("Browser window not found");
  gfx::Rect bounds = window->GetBounds();
  const bool set_bounds = window_bounds->HasLeft() || window_bounds->HasTop() ||
                          window_bounds->HasWidth() ||
                          window_bounds->HasHeight();
  if (set_bounds) {
    bounds.set_x(window_bounds->GetLeft(bounds.x()));
    bounds.set_y(window_bounds->GetTop(bounds.y()));
    bounds.set_width(window_bounds->GetWidth(bounds.width()));
    bounds.set_height(window_bounds->GetHeight(bounds.height()));
  }

  const std::string& window_state = window_bounds->GetWindowState("normal");
  if (set_bounds && window_state != "normal") {
    return protocol::Response::Error(
        "The 'minimized', 'maximized' and 'fullscreen' states cannot be "
        "combined with 'left', 'top', 'width' or 'height'");
  }

  if (window_state == "fullscreen") {
    if (window->IsMinimized()) {
      return protocol::Response::Error(
          "To make minimized window fullscreen, "
          "restore it to normal state first.");
    }
    window->GetExclusiveAccessContext()->EnterFullscreen(
        GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE);
  } else if (window_state == "maximized") {
    if (window->IsMinimized() || window->IsFullscreen()) {
      return protocol::Response::Error(
          "To maximize a minimized or fullscreen "
          "window, restore it to normal state first.");
    }
    window->Maximize();
  } else if (window_state == "minimized") {
    if (window->IsFullscreen()) {
      return protocol::Response::Error(
          "To minimize a fullscreen window, restore it to normal "
          "state first.");
    }
    window->Minimize();
  } else if (window_state == "normal") {
    if (window->IsFullscreen())
      window->GetExclusiveAccessContext()->ExitFullscreen();
    else if (window->IsMinimized())
      window->Show();
    else if (window->IsMaximized())
      window->Restore();
    else if (set_bounds)
      window->SetBounds(bounds);
  } else {
    NOTREACHED();
  }

  return protocol::Response::OK();
}
