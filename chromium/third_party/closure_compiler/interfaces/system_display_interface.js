// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated by:
//   ./tools/json_schema_compiler/compiler.py.

// This was modified to replace System.display with SystemDisplay.

/** @fileoverview Interface for system.display that can be overriden. */

assertNotReached('Interface file for Closure Compiler should not be executed.');

/** @interface */
function SystemDisplay() {}

SystemDisplay.prototype = {
  /**
   * Get the information of all attached display devices.
   * @param {function(!Array<!chrome.system.display.DisplayUnitInfo>):void}
   *     callback
   * @see https://developer.chrome.com/extensions/system.display#method-getInfo
   */
  getInfo: assertNotReached,

  /**
   * Updates the properties for the display specified by |id|, according to the
   * information provided in |info|. On failure, $(ref:runtime.lastError) will
   * be set.
   * @param {string} id The display's unique identifier.
   * @param {!chrome.system.display.DisplayProperties} info The information
   *     about display properties that should be changed.     A property will be
   *     changed only if a new value for it is specified in     |info|.
   * @param {function():void=} callback Empty function called when the function
   *     finishes. To find out     whether the function succeeded,
   *     $(ref:runtime.lastError) should be     queried.
   * @see https://developer.chrome.com/extensions/system.display#method-setDisplayProperties
   */
  setDisplayProperties: assertNotReached,

  /**
   * Enables/disables the unified desktop feature. Note that this simply enables
   * the feature, but will not change the actual desktop mode. (That is, if the
   * desktop is in mirror mode, it will stay in mirror mode)
   * @param {boolean} enabled
   * @see https://developer.chrome.com/extensions/system.display#method-enableUnifiedDesktop
   */
  enableUnifiedDesktop: assertNotReached,
};

/**
 * Fired when anything changes to the display configuration.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/system.display#event-onDisplayChanged
 */
SystemDisplay.prototype.onDisplayChanged;
