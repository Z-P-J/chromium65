// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_FORM_ANALYSER_LOGGER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_FORM_ANALYSER_LOGGER_H_

#include <string>
#include <vector>

#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"

namespace autofill {

using ConsoleLevel = blink::WebConsoleMessage::Level;

// ConsoleLogger provides a convenient interface for logging messages to the
// DevTools console, both in terms of wrapping and formatting console messages
// along with their parameters, and in ordering messages so that higher-priority
// warnings are displayed first.
class PageFormAnalyserLogger {
 public:
  static const ConsoleLevel kError = blink::WebConsoleMessage::kLevelError;
  static const ConsoleLevel kWarning = blink::WebConsoleMessage::kLevelWarning;
  static const ConsoleLevel kVerbose = blink::WebConsoleMessage::kLevelVerbose;

  explicit PageFormAnalyserLogger(blink::WebLocalFrame* frame);
  ~PageFormAnalyserLogger();

  // Virtual for testing.
  virtual void Send(std::string message,
                    ConsoleLevel level,
                    blink::WebNode node);
  virtual void Send(std::string message,
                    ConsoleLevel level,
                    std::vector<blink::WebNode> nodes);
  virtual void Flush();

 private:
  blink::WebLocalFrame* frame_;

  // Though PageFormAnalyserLogger provides buffering, it is intended to be
  // simply over the course of a single analysis, for ordering purposes.
  struct LogEntry;
  std::map<ConsoleLevel, std::vector<LogEntry>> node_buffer_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_FORM_ANALYSER_LOGGER_H_
