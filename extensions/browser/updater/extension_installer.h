// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSIONS_INSTALLER_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSIONS_INSTALLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/update_client/update_client.h"

namespace extensions {

// This class is used as a shim between the components::update_client and
// extensions code, to help the generic update_client code prepare and then
// install an updated version of an extension. Because the update_client code
// doesn't have the notion of extension ids, we use instances of this class to
// map an install request back to the original update check for a given
// extension.
class ExtensionInstaller : public update_client::CrxInstaller {
 public:
  using UpdateClientCallback = update_client::CrxInstaller::Callback;
  // A callback to implement the install of a new version of the extension.
  // Takes ownership of the directory at |unpacked_dir|.
  using ExtensionInstallerCallback =
      base::OnceCallback<void(const std::string& extension_id,
                              const std::string& public_key,
                              const base::FilePath& unpacked_dir,
                              UpdateClientCallback update_client_callback)>;

  // This method takes the id and root directory for an extension we're doing
  // an update check for, as well as a callback to call if we get a new version
  // of it to install.
  ExtensionInstaller(std::string extension_id,
                     const base::FilePath& extension_root,
                     ExtensionInstallerCallback extension_installer_callback);

  // update_client::CrxInstaller::
  void OnUpdateError(int error) override;

  // This function is executed by the component update client, which runs on a
  // blocking thread with background priority.
  // |update_client_callback| is expected to be called on a UI thread.
  void Install(const base::FilePath& unpack_path,
               const std::string& public_key,
               UpdateClientCallback update_client_callback) override;
  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;
  bool Uninstall() override;

 private:
  friend class base::RefCountedThreadSafe<ExtensionInstaller>;
  ~ExtensionInstaller() override;

  std::string extension_id_;
  base::FilePath extension_root_;
  ExtensionInstallerCallback extension_installer_callback_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstaller);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSIONS_INSTALLER_H_
