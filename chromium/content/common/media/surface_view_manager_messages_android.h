// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for surface view manager.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/geometry/size.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START SurfaceViewManagerMsgStart

// Message sent from the renderer to the browser

IPC_MESSAGE_ROUTED1(SurfaceViewManagerHostMsg_CreateFullscreenSurface,
                    gfx::Size /* video_natural_size */)
IPC_MESSAGE_ROUTED1(SurfaceViewManagerHostMsg_NaturalSizeChanged,
                    gfx::Size /* size */)

// Message sent from the browser to the renderer

IPC_MESSAGE_ROUTED1(SurfaceViewManagerMsg_FullscreenSurfaceCreated,
                    int /* surface_id */)
