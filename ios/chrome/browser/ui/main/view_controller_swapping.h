// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_VIEW_CONTROLLER_SWAPPING_H_
#define IOS_CHROME_BROWSER_UI_MAIN_VIEW_CONTROLLER_SWAPPING_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@protocol TabSwitcher;

// ViewControllerSwapping defines a set of methods that allow an object to
// display TabSwitchers and view controllers that display tabs.
@protocol ViewControllerSwapping

// The view controller, if any, that is active.
@property(nonatomic, readonly, strong) UIViewController* activeViewController;

// Displays the given TabSwitcher, replacing any TabSwitchers or view
// controllers that may currently be visible.  Runs the given |completion| block
// after the view controller is visible.
- (void)showTabSwitcher:(UIViewController<TabSwitcher>*)tabSwitcher
             completion:(ProceduralBlock)completion;

// Displays the given view controller, replacing any TabSwitchers or other view
// controllers that may currently be visible.  Runs the given |completion| block
// after the view controller is visible.
- (void)showTabViewController:(UIViewController*)viewController
                   completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_VIEW_CONTROLLER_SWAPPING_H_
