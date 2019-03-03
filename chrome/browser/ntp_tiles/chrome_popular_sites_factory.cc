// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_tiles/chrome_popular_sites_factory.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"

std::unique_ptr<ntp_tiles::PopularSites>
ChromePopularSitesFactory::NewForProfile(Profile* profile) {
  return base::MakeUnique<ntp_tiles::PopularSitesImpl>(
      profile->GetPrefs(), TemplateURLServiceFactory::GetForProfile(profile),
      g_browser_process->variations_service(), profile->GetRequestContext(),
      base::Bind(
          data_decoder::SafeJsonParser::Parse,
          content::ServiceManagerConnection::GetForProcess()->GetConnector()));
}
