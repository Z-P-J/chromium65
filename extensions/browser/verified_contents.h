// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VERIFIED_CONTENTS_H_
#define EXTENSIONS_BROWSER_VERIFIED_CONTENTS_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/version.h"

namespace extensions {

// This class encapsulates the data in a "verified_contents.json" file
// generated by the webstore for a .crx file. That data includes a set of
// signed expected hashes of file content which can be used to check for
// corruption of extension files on local disk.
class VerifiedContents {
 public:
  // Note: the public_key must remain valid for the lifetime of this object.
  VerifiedContents(const uint8_t* public_key, size_t public_key_size);
  ~VerifiedContents();

  // Returns true if we successfully parsed the verified_contents.json file at
  // |path| and validated the enclosed signature. The
  bool InitFrom(const base::FilePath& path);

  int block_size() const { return block_size_; }
  const std::string& extension_id() const { return extension_id_; }
  const base::Version& version() const { return version_; }

  bool HasTreeHashRoot(const base::FilePath& relative_path) const;

  bool TreeHashRootEquals(const base::FilePath& relative_path,
                          const std::string& expected) const;

  // If InitFrom has not been called yet, or was used in "ignore invalid
  // signature" mode, this can return false.
  bool valid_signature() { return valid_signature_; }

 private:
  // Returns the base64url-decoded "payload" field from the json at |path|, if
  // the signature was valid.
  bool GetPayload(const base::FilePath& path, std::string* payload);

  // The |protected_value| and |payload| arguments should be base64url encoded
  // strings, and |signature_bytes| should be a byte array. See comments in the
  // .cc file on GetPayload for where these come from in the overall input
  // file.
  bool VerifySignature(const std::string& protected_value,
                       const std::string& payload,
                       const std::string& signature_bytes);

  // The public key we should use for signature verification.
  const uint8_t* public_key_;
  const size_t public_key_size_;

  // Indicates whether the signature was successfully validated or not.
  bool valid_signature_;

  // The block size used for computing the treehash root hashes.
  int block_size_;

  // Information about which extension these signed hashes are for.
  std::string extension_id_;
  base::Version version_;

  // The expected treehash root hashes for each file, lower cased so we can do
  // case-insensitive lookups.
  //
  // We use a multi-map here so that we can do fast lookups of paths from
  // requests on case-insensitive systems (windows, mac) where the request path
  // might not have the exact right capitalization, but not break
  // case-sensitive systems (linux, chromeos). TODO(asargent) - we should give
  // developers client-side warnings in each of those cases, and have the
  // webstore reject the cases they can statically detect. See crbug.com/29941
  typedef std::multimap<base::FilePath::StringType, std::string> RootHashes;
  RootHashes root_hashes_;

  DISALLOW_COPY_AND_ASSIGN(VerifiedContents);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_VERIFIED_CONTENTS_H_
