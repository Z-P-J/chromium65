// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/untrusted_authority_certs_cache.h"

#include <cert.h>
#include <certdb.h>
#include <secitem.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/pem_tokenizer.h"
#include "net/der/input.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

class UntrustedAuthorityCertsCacheTest : public testing::Test {
 public:
  UntrustedAuthorityCertsCacheTest() {}
  ~UntrustedAuthorityCertsCacheTest() override {}

 protected:
  // Reads the certificates from |pem_cert_files|, assuming that each file
  // contains one CERTIFICATE block. Returns all certificates.
  // Note: This funcion uses ASSERT_ macros, so the caller must verify for
  // failures after it returns.
  void GetAuthoritiesFromFiles(
      std::vector<base::FilePath> pem_cert_files,
      std::vector<std::string>* out_x509_authority_certs) {
    for (const auto& pem_cert_file : pem_cert_files) {
      std::string x509_authority_cert;
      ASSERT_TRUE(base::ReadFileToString(pem_cert_file, &x509_authority_cert));
      out_x509_authority_certs->push_back(std::move(x509_authority_cert));
    }
  }

  // Checks if the certificate stored in |pem_cert_file| can be found in the
  // default NSS certificate database using CERT_FindCertByName.
  // Stores the result in *|out_available|.
  // Note: This funcion uses ASSERT_ macros, so the caller must verify for
  // failures after it returns.
  void CheckIsCertificateAvailable(const base::FilePath& pem_cert_file,
                                   bool* out_available) {
    std::string cert_contents_buffer;
    net::der::Input subject;
    ASSERT_NO_FATAL_FAILURE(GetCertificateSubjectDN(
        pem_cert_file, &cert_contents_buffer, &subject));

    SECItem subject_item;
    subject_item.len = subject.Length();
    subject_item.data = const_cast<unsigned char*>(subject.UnsafeData());

    net::ScopedCERTCertificate found_cert(
        CERT_FindCertByName(CERT_GetDefaultCertDB(), &subject_item));
    *out_available = static_cast<bool>(found_cert);
  }

  // Determines the subject DN of the certificate stored in
  // |pem_cert_file|. Stores the result in *|out_subject|.
  // The der::Input data structure contains unowned pointers into the
  // certificate data buffer. The caller must pass a buffer in
  // |cert_contents_buffer| and ensure to only use *|out_subject| while
  // *|cert_contents_buffer| is in scope.
  // Note: This funcion uses ASSERT_ macros, so the caller must verify for
  // failures after it returns.
  void GetCertificateSubjectDN(const base::FilePath& pem_cert_file,
                               std::string* cert_contents_buffer,
                               net::der::Input* out_subject) {
    std::string file_data;
    ASSERT_TRUE(base::ReadFileToString(pem_cert_file, &file_data));

    std::vector<std::string> pem_headers;
    pem_headers.push_back("CERTIFICATE");
    net::PEMTokenizer pem_tokenizer(file_data, pem_headers);
    ASSERT_TRUE(pem_tokenizer.GetNext());
    *cert_contents_buffer = pem_tokenizer.data();

    // Parsing the certificate.
    net::der::Input tbs_certificate_tlv;
    net::der::Input signature_algorithm_tlv;
    net::der::BitString signature_value;
    net::CertErrors errors;
    ASSERT_TRUE(net::ParseCertificate(
        net::der::Input(cert_contents_buffer), &tbs_certificate_tlv,
        &signature_algorithm_tlv, &signature_value, &errors));

    net::ParsedTbsCertificate tbs;
    net::ParseCertificateOptions options;
    options.allow_invalid_serial_numbers = true;
    ASSERT_TRUE(
        net::ParseTbsCertificate(tbs_certificate_tlv, options, &tbs, nullptr));
    *out_subject = tbs.subject_tlv;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UntrustedAuthorityCertsCacheTest);
};

// Checks that a certificate made available through the
// UntrustedAuthorityCertsCache can be found by NSS. We specifically check for
// lookup through the CERT_FindCertByName function, as this is what is used in
// client certificate matching (see MatchClientCertificateIssuers in
// net/third_party/nss/ssl/cmpcert.cc). Additionally, checks that the
// certificate is not available after the UntrustedAuthorityCache goes out of
// scope.
TEST_F(UntrustedAuthorityCertsCacheTest, CertMadeAvailable) {
  base::FilePath cert_file_path =
      net::GetTestCertsDirectory().AppendASCII("client_1_ca.pem");
  {
    std::vector<std::string> x509_authority_certs;
    ASSERT_NO_FATAL_FAILURE(
        GetAuthoritiesFromFiles({cert_file_path}, &x509_authority_certs));
    UntrustedAuthorityCertsCache cache(x509_authority_certs);

    bool cert_available = false;
    ASSERT_NO_FATAL_FAILURE(
        CheckIsCertificateAvailable(cert_file_path, &cert_available));
    EXPECT_TRUE(cert_available);
  }

  bool cert_available_no_cache = true;
  ASSERT_NO_FATAL_FAILURE(
      CheckIsCertificateAvailable(cert_file_path, &cert_available_no_cache));
  EXPECT_FALSE(cert_available_no_cache);
}

}  // namespace
}  // namespace policy
