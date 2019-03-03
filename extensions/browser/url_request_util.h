// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_
#define EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_

#include <string>

#include "content/public/common/resource_type.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace net {
class URLRequest;
}

namespace extensions {
class Extension;
class ExtensionSet;
class ProcessMap;

// Utilities related to URLRequest jobs for extension resources. See
// chrome/browser/extensions/extension_protocols_unittest.cc for related tests.
namespace url_request_util {

// Sets allowed=true to allow a chrome-extension:// resource request coming from
// renderer A to access a resource in an extension running in renderer B.
// Returns false when it couldn't determine if the resource is allowed or not
bool AllowCrossRendererResourceLoad(const GURL& url,
                                    content::ResourceType resource_type,
                                    ui::PageTransition page_transition,
                                    int child_id,
                                    bool is_incognito,
                                    const Extension* extension,
                                    const ExtensionSet& extensions,
                                    const ProcessMap& process_map,
                                    bool* allowed);

// Helper method that is called by both AllowCrossRendererResourceLoad and
// ExtensionNavigationThrottle to share logic.
// Sets allowed=true to allow a chrome-extension:// resource request coming from
// renderer A to access a resource in an extension running in renderer B.
// Returns false when it couldn't determine if the resource is allowed or not
bool AllowCrossRendererResourceLoadHelper(bool is_guest,
                                          const Extension* extension,
                                          const Extension* owner_extension,
                                          const std::string& partition_id,
                                          const std::string& resource_path,
                                          ui::PageTransition page_transition,
                                          bool* allowed);

}  // namespace url_request_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_
