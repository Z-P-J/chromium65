// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific constants and install modes for Google Chrome.

#include <stdlib.h>

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/install_static/install_modes.h"

namespace install_static {

const wchar_t kCompanyPathName[] = L"Google";

const wchar_t kProductPathName[] = L"Chrome";

const size_t kProductPathNameLength = _countof(kProductPathName) - 1;

const wchar_t kBinariesAppGuid[] = L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

// Google Chrome integrates with Google Update, so the app GUID above is used.
const wchar_t kBinariesPathName[] = L"";

const InstallConstants kInstallModes[] = {
    // The primary install mode for stable Google Chrome.
    {
        sizeof(kInstallModes[0]),
        STABLE_INDEX,  // The first mode is for stable/beta/dev.
        "",            // No install switch for the primary install mode.
        L"",           // Empty install_suffix for the primary install mode.
        L"",           // No logo suffix for the primary install mode.
        L"{8A69D345-D564-463c-AFF1-A69D9E530F96}",
        L"Google Chrome",                           // A distinct base_app_name.
        L"Chrome",                                  // A distinct base_app_id.
        L"ChromeHTML",                              // ProgID prefix.
        L"Chrome HTML Document",                    // ProgID description.
        L"{8A69D345-D564-463c-AFF1-A69D9E530F96}",  // Active Setup GUID.
        L"{5C65F4B0-3651-4514-B207-D10CB699B14B}",  // CommandExecuteImpl CLSID.
        {0xA2C6CB58,
         0xC076,
         0x425C,
         {0xAC, 0xB7, 0x6D, 0x19, 0xD6, 0x44, 0x28,
          0xCD}},  // Toast Activator CLSID.
        L"",       // The empty string means "stable".
        ChannelStrategy::ADDITIONAL_PARAMETERS,
        true,  // Supports system-level installs.
        true,  // Supports in-product set as default browser UX.
        true,  // Supports retention experiments.
        true,  // Supported multi-install.
        icon_resources::kApplicationIndex,  // App icon resource index.
        IDR_MAINFRAME,                      // App icon resource id.
    },
    // A secondary install mode for Google Chrome Beta
    {
        sizeof(kInstallModes[0]),
        BETA_INDEX,     // The mode for the side-by-side beta channel.
        "chrome-beta",  // Install switch.
        L" Beta",       // Install suffix.
        L"Beta",        // Logo suffix.
        L"{8237E44A-0054-442C-B6B6-EA0509993955}",  // A distinct app GUID.
        L"Google Chrome Beta",                      // A distinct base_app_name.
        L"ChromeBeta",                              // A distinct base_app_id.
        L"ChromeBHTML",                             // ProgID prefix.
        L"Chrome Beta HTML Document",               // ProgID description.
        L"{8237E44A-0054-442C-B6B6-EA0509993955}",  // Active Setup GUID.
        L"",                                        // CommandExecuteImpl CLSID.
        {0xB89B137F,
         0x96AA,
         0x4AE2,
         {0x98, 0xC4, 0x63, 0x73, 0xEA, 0xA1, 0xEA,
          0x4D}},  // Toast Activator CLSID.
        L"beta",   // Forced channel name.
        ChannelStrategy::FIXED,
        true,   // Supports system-level installs.
        true,   // Supports in-product set as default browser UX.
        true,   // Supports retention experiments.
        false,  // Did not support multi-install.
        icon_resources::kBetaApplicationIndex,  // App icon resource index.
        IDR_X005_BETA,                          // App icon resource id.
    },
    // A secondary install mode for Google Chrome Dev
    {
        sizeof(kInstallModes[0]),
        DEV_INDEX,     // The mode for the side-by-side dev channel.
        "chrome-dev",  // Install switch.
        L" Dev",       // Install suffix.
        L"Dev",        // Logo suffix.
        L"{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}",  // A distinct app GUID.
        L"Google Chrome Dev",                       // A distinct base_app_name.
        L"ChromeDev",                               // A distinct base_app_id.
        L"ChromeDHTML",                             // ProgID prefix.
        L"Chrome Dev HTML Document",                // ProgID description.
        L"{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}",  // Active Setup GUID.
        L"",                                        // CommandExecuteImpl CLSID.
        {0xF01C03EB,
         0xD431,
         0x4C83,
         {0x8D, 0x7A, 0x90, 0x27, 0x71, 0xE7, 0x32,
          0xFA}},  // Toast Activator CLSID.
        L"dev",    // Forced channel name.
        ChannelStrategy::FIXED,
        true,   // Supports system-level installs.
        true,   // Supports in-product set as default browser UX.
        true,   // Supports retention experiments.
        false,  // Did not support multi-install.
        icon_resources::kDevApplicationIndex,  // App icon resource index.
        IDR_X004_DEV,                          // App icon resource id.
    },
    // A secondary install mode for Google Chrome SxS (canary).
    {
        sizeof(kInstallModes[0]),
        CANARY_INDEX,  // The mode for the side-by-side canary channel.
        "chrome-sxs",  // Install switch.
        L" SxS",       // Install suffix.
        L"Canary",     // Logo suffix.
        L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}",  // A distinct app GUID.
        L"Google Chrome Canary",                    // A distinct base_app_name.
        L"ChromeCanary",                            // A distinct base_app_id.
        L"ChromeSSHTM",                             // ProgID prefix.
        L"Chrome Canary HTML Document",             // ProgID description.
        L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}",  // Active Setup GUID.
        L"{1BEAC3E3-B852-44F4-B468-8906C062422E}",  // CommandExecuteImpl CLSID.
        {0xFA372A6E,
         0x149F,
         0x4E95,
         {0x83, 0x2D, 0x8F, 0x69, 0x8D, 0x40, 0xAD,
          0x7F}},   // Toast Activator CLSID.
        L"canary",  // Forced channel name.
        ChannelStrategy::FIXED,
        false,  // Does not support system-level installs.
        false,  // Does not support in-product set as default browser UX.
        true,   // Supports retention experiments.
        false,  // Did not support multi-install.
        icon_resources::kSxSApplicationIndex,  // App icon resource index.
        IDR_SXS,                               // App icon resource id.
    },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
