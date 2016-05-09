// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_macros.h"

// IPC messages for page-level actions.
// Multiply-included message file, hence no include guard.

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START PageMsgStart

// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED1(PageMsg_UpdateWindowScreenRect,
                    gfx::Rect /* window_screen_rect */)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Adding a new message? Stick to the sort order above: first platform
// independent PageMsg, then ifdefs for platform specific PageMsg, then platform
// independent PageHostMsg, then ifdefs for platform specific PageHostMsg.
