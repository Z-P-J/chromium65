// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_OBSERVER_H_
#define ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_OBSERVER_H_

#include "ash/app_list/model/app_list_model_export.h"

namespace app_list {

class APP_LIST_MODEL_EXPORT SearchResultObserver {
 public:
  // Invoked when the SearchResult's icon has changed.
  virtual void OnIconChanged() {}

  // Invoked when the SearchResult's badge icon has changed.
  virtual void OnBadgeIconChanged() {}

  // Invoked when the SearchResult's app rating has changed.
  virtual void OnRatingChanged() {}

  // Invoked when the SearchResult's app formatted price has changed.
  virtual void OnFormattedPriceChanged() {}

  // Invoked when the SearchResult's actions have changed.
  virtual void OnActionsChanged() {}

  // Invoked when the SearchResult's is_installing flag has changed.
  virtual void OnIsInstallingChanged() {}

  // Invoked when the download percentage has changed.
  virtual void OnPercentDownloadedChanged() {}

  // Invoked when the item represented by the SearchResult is installed.
  virtual void OnItemInstalled() {}

  // Invoked just before the SearchResult is destroyed.
  virtual void OnResultDestroying() {}

 protected:
  virtual ~SearchResultObserver() {}
};

}  // namespace app_list

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_RESULT_OBSERVER_H_
