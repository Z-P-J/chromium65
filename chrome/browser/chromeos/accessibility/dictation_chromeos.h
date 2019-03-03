// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_DICTATION_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_DICTATION_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "content/public/browser/speech_recognition_session_preamble.h"

class Profile;
class SpeechRecognizer;

// Provides global dictation (type what you speak) on Chrome OS.
class DictationChromeos : public SpeechRecognizerDelegate {
 public:
  explicit DictationChromeos(Profile* profile);
  ~DictationChromeos() override;

  // User-initiated dictation.
  void OnToggleDictation();

 private:
  // SpeechRecognizerDelegate:
  void OnSpeechResult(const base::string16& query, bool is_final) override;
  void OnSpeechSoundLevelChanged(int16_t level) override;
  void OnSpeechRecognitionStateChanged(
      SpeechRecognizerState new_state) override;
  void GetSpeechAuthParameters(std::string* auth_scope,
                               std::string* auth_token) override;

  std::unique_ptr<SpeechRecognizer> speech_recognizer_;

  Profile* profile_;

  base::WeakPtrFactory<DictationChromeos> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DictationChromeos);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_DICTATION_CHROMEOS_H_
