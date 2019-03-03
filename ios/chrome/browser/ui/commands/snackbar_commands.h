// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_SNACKBAR_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_SNACKBAR_COMMANDS_H_

#import <Foundation/Foundation.h>

@class MDCSnackbarMessage;

// Commands related to Snackbar.
@protocol SnackbarCommands

// Shows a snackbar with |message|.
- (void)showSnackbarMessage:(MDCSnackbarMessage*)message;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SNACKBAR_COMMANDS_H_