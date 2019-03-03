// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_ITEM_NOTIFICATION_H_
#define CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_ITEM_NOTIFICATION_H_

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/image_decoder.h"
#include "content/public/browser/download_item.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/native_theme/native_theme.h"

class SkBitmap;

namespace test {
class DownloadItemNotificationTest;
}

namespace gfx {
struct VectorIcon;
}

namespace message_center {
class Notification;
}

class DownloadNotificationManagerForProfile;

class DownloadItemNotification : public ImageDecoder::ImageRequest {
 public:
  DownloadItemNotification(content::DownloadItem* item,
                           DownloadNotificationManagerForProfile* manager);

  ~DownloadItemNotification() override;

  void OnDownloadUpdated(content::DownloadItem* item);
  void OnDownloadRemoved(content::DownloadItem* item);

  // Disables popup by setting low priority.
  void DisablePopup();

  // Called back from the NotificationHandler.
  void OnNotificationClose();
  void OnNotificationClick();
  void OnNotificationButtonClick(int button_index);

 private:
  class DownloadItemNotificationDelegate;
  friend class test::DownloadItemNotificationTest;

  enum ImageDecodeStatus { NOT_STARTED, IN_PROGRESS, DONE, FAILED, NOT_IMAGE };

  enum NotificationUpdateType {
    ADD,
    UPDATE,
    UPDATE_AND_POPUP
  };

  std::string GetNotificationId() const;

  void CloseNotification();
  void Update();
  void UpdateNotificationData(bool display, bool bump_priority);
  void UpdateNotificationIcon();

  // Set icon of the notification.
  void SetNotificationIcon(const gfx::VectorIcon& icon, SkColor color);

  // Set preview image of the notification. Must be called on IO thread.
  void OnImageLoaded(const std::string& image_data);
  void OnImageCropped(const SkBitmap& image);

  // ImageDecoder::ImageRequest overrides:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

  // Returns a short one-line status string for the download.
  base::string16 GetTitle() const;

  // Returns a short one-line status string for a download command.
  base::string16 GetCommandLabel(DownloadCommands::Command command) const;

  // Get the warning text to notify a dangerous download. Should only be called
  // if IsDangerous() is true.
  base::string16 GetWarningStatusString() const;

  // Get the sub status text of the current in-progress download status. Should
  // be called only for downloads in progress.
  base::string16 GetInProgressSubStatusString() const;

  // Get the sub status text. Can be called for downloads in all states.
  // If the state does not have sub status string, it returns empty string.
  base::string16 GetSubStatusString() const;

  // Get the status text.
  base::string16 GetStatusString() const;

  bool IsNotificationVisible() const;

  Browser* GetBrowser() const;
  Profile* profile() const;

  // Returns the list of possible extra (all except the default) actions.
  std::unique_ptr<std::vector<DownloadCommands::Command>> GetExtraActions()
      const;

  // Flag to show the notification on next update. If true, the notification
  // goes visible. The initial value is true so it gets shown on initial update.
  bool show_next_ = true;

  // Flag if the notification has been closed or not. Setting this flag
  // prevents updates after close.
  bool closed_ = false;

  content::DownloadItem::DownloadState previous_download_state_ =
      content::DownloadItem::MAX_DOWNLOAD_STATE;  // As uninitialized state
  bool previous_dangerous_state_ = false;
  std::unique_ptr<message_center::Notification> notification_;
  content::DownloadItem* item_;
  std::unique_ptr<std::vector<DownloadCommands::Command>> button_actions_;

  // Status of the preview image decode.
  ImageDecodeStatus image_decode_status_ = NOT_STARTED;

  base::WeakPtrFactory<DownloadItemNotification> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemNotification);
};

#endif  // CHROME_BROWSER_DOWNLOAD_NOTIFICATION_DOWNLOAD_ITEM_NOTIFICATION_H_
