// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TimedCanvasDrawListener_h
#define TimedCanvasDrawListener_h

#include "core/html/canvas/CanvasDrawListener.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCanvasCaptureHandler.h"

namespace blink {

class TimedCanvasDrawListener final : public GarbageCollectedFinalized<TimedCanvasDrawListener>, public CanvasDrawListener {
    USING_GARBAGE_COLLECTED_MIXIN(TimedCanvasDrawListener);
public:
    ~TimedCanvasDrawListener();
    static TimedCanvasDrawListener* create(const PassOwnPtr<WebCanvasCaptureHandler>&, double frameRate);
    void sendNewFrame(const WTF::PassRefPtr<SkImage>&) override;

    DEFINE_INLINE_TRACE() {}
private:
    TimedCanvasDrawListener(const PassOwnPtr<WebCanvasCaptureHandler>&, double frameRate);
    // Implementation of TimerFiredFunction.
    void requestFrameTimerFired(Timer<TimedCanvasDrawListener>*);

    double m_frameInterval;
    UnthrottledTimer<TimedCanvasDrawListener> m_requestFrameTimer;
};

} // namespace blink

#endif
