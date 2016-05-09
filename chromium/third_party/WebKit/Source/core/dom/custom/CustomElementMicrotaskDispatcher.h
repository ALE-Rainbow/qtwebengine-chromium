// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementMicrotaskDispatcher_h
#define CustomElementMicrotaskDispatcher_h

#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace blink {

class CustomElementCallbackQueue;

class CustomElementMicrotaskDispatcher final : public GarbageCollected<CustomElementMicrotaskDispatcher> {
    WTF_MAKE_NONCOPYABLE(CustomElementMicrotaskDispatcher);
public:
    static CustomElementMicrotaskDispatcher& instance();

    void enqueue(CustomElementCallbackQueue*);

    bool elementQueueIsEmpty() { return m_elements.isEmpty(); }

    DECLARE_TRACE();

private:
    CustomElementMicrotaskDispatcher();

    void ensureMicrotaskScheduledForElementQueue();
    void ensureMicrotaskScheduled();

    static void dispatch();
    void doDispatch();

    bool m_hasScheduledMicrotask;
    enum {
        Quiescent,
        Resolving,
        DispatchingCallbacks
    } m_phase;

    HeapVector<Member<CustomElementCallbackQueue>> m_elements;
};

} // namespace blink

#endif // CustomElementMicrotaskDispatcher_h
