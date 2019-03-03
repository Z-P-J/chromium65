// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/content_watcher.h"

#include <stddef.h>

#include <set>

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace extensions {

namespace {

class FrameContentWatcher
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<FrameContentWatcher> {
 public:
  FrameContentWatcher(content::RenderFrame* render_frame,
                      const blink::WebVector<blink::WebString>& css_selectors);
  ~FrameContentWatcher() override;

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidCreateDocumentElement() override;
  void DidMatchCSS(
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors)
      override;

  void UpdateCSSSelectors(const blink::WebVector<blink::WebString>& selectors);

 private:
  // Given that we saw a change in the CSS selectors that the associated frame
  // matched, tells the browser about the new set of matching selectors in its
  // top-level page. We filter this so that if an extension were to be granted
  // activeTab permission on that top-level page, we only send CSS selectors for
  // frames that it could run on.
  // Note: Currently, this works with OOPIFs because, since we only send this
  // for a matching selector found in a frame that the top frame can access,
  // that frame is guaranteed to be local. If we ever isolate frames regardless
  // of whether the top frame could access them, or if we notify of matches for
  // frames the top frame cannot access, we may have to rethink this.
  void NotifyBrowserOfChange();

  blink::WebVector<blink::WebString> css_selectors_;
  std::set<std::string> matching_selectors_;
  bool document_created_ = false;

  DISALLOW_COPY_AND_ASSIGN(FrameContentWatcher);
};

FrameContentWatcher::FrameContentWatcher(
    content::RenderFrame* render_frame,
    const blink::WebVector<blink::WebString>& css_selectors)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<FrameContentWatcher>(render_frame),
      css_selectors_(css_selectors) {}

FrameContentWatcher::~FrameContentWatcher() {}

void FrameContentWatcher::OnDestruct() {
  delete this;
}

void FrameContentWatcher::DidCreateDocumentElement() {
  document_created_ = true;
  render_frame()->GetWebFrame()->GetDocument().WatchCSSSelectors(
      css_selectors_);
}

void FrameContentWatcher::DidMatchCSS(
    const blink::WebVector<blink::WebString>& newly_matching_selectors,
    const blink::WebVector<blink::WebString>& stopped_matching_selectors) {
  for (size_t i = 0; i < stopped_matching_selectors.size(); ++i)
    matching_selectors_.erase(stopped_matching_selectors[i].Utf8());
  for (size_t i = 0; i < newly_matching_selectors.size(); ++i)
    matching_selectors_.insert(newly_matching_selectors[i].Utf8());

  NotifyBrowserOfChange();
}

void FrameContentWatcher::UpdateCSSSelectors(
    const blink::WebVector<blink::WebString>& selectors) {
  css_selectors_ = selectors;
  if (document_created_) {
    render_frame()->GetWebFrame()->GetDocument().WatchCSSSelectors(
        css_selectors_);
  }
}

void FrameContentWatcher::NotifyBrowserOfChange() {
  blink::WebLocalFrame* changed_frame = render_frame()->GetWebFrame();
  blink::WebFrame* const top_frame = changed_frame->Top();
  const blink::WebSecurityOrigin top_origin = top_frame->GetSecurityOrigin();
  // Want to aggregate matched selectors from all frames where an
  // extension with access to top_origin could run on the frame.
  if (!top_origin.CanAccess(changed_frame->GetSecurityOrigin())) {
    // If the changed frame can't be accessed by the top frame, then
    // no change in it could affect the set of selectors we'd send back.
    return;
  }

  std::set<base::StringPiece> transitive_selectors;
  for (blink::WebFrame* frame = top_frame; frame;
       frame = frame->TraverseNext()) {
    if (frame->IsWebLocalFrame() &&
        top_origin.CanAccess(frame->GetSecurityOrigin())) {
      FrameContentWatcher* watcher = FrameContentWatcher::Get(
          content::RenderFrame::FromWebFrame(frame->ToWebLocalFrame()));
      if (watcher && !watcher->matching_selectors_.empty()) {
        transitive_selectors.insert(watcher->matching_selectors_.begin(),
                                    watcher->matching_selectors_.end());
      }
    }
  }

  std::vector<std::string> selector_strings;
  for (const base::StringPiece& selector : transitive_selectors)
    selector_strings.push_back(selector.as_string());

  // TODO(devlin): Frame-ify this message.
  content::RenderView* view =
      content::RenderView::FromWebView(top_frame->View());
  view->Send(new ExtensionHostMsg_OnWatchedPageChange(view->GetRoutingID(),
                                                      selector_strings));
}

}  // namespace

ContentWatcher::ContentWatcher() {}
ContentWatcher::~ContentWatcher() {}

void ContentWatcher::OnWatchPages(
    const std::vector<std::string>& new_css_selectors_utf8) {
  blink::WebVector<blink::WebString> new_css_selectors(
      new_css_selectors_utf8.size());
  bool changed = new_css_selectors.size() != css_selectors_.size();
  for (size_t i = 0; i < new_css_selectors.size(); ++i) {
    new_css_selectors[i] =
        blink::WebString::FromUTF8(new_css_selectors_utf8[i]);
    if (!changed && new_css_selectors[i] != css_selectors_[i])
      changed = true;
  }

  if (!changed)
    return;

  css_selectors_.Swap(new_css_selectors);

  // Tell each frame's document about the new set of watched selectors. These
  // will trigger calls to DidMatchCSS after Blink has a chance to apply the new
  // style, which will in turn notify the browser about the changes.
  struct WatchSelectors : public content::RenderFrameVisitor {
    explicit WatchSelectors(
        const blink::WebVector<blink::WebString>& css_selectors)
        : css_selectors(css_selectors) {}

    bool Visit(content::RenderFrame* frame) override {
      FrameContentWatcher::Get(frame)->UpdateCSSSelectors(css_selectors);
      return true;  // Continue visiting.
    }

    const blink::WebVector<blink::WebString>& css_selectors;
  };
  WatchSelectors visitor(css_selectors_);
  content::RenderFrame::ForEach(&visitor);
}

void ContentWatcher::OnRenderFrameCreated(content::RenderFrame* render_frame) {
  // Manages its own lifetime.
  new FrameContentWatcher(render_frame, css_selectors_);
}

}  // namespace extensions
