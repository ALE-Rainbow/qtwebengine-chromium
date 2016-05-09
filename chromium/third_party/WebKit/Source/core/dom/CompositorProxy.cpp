// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/CompositorProxy.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"
#include <algorithm>

namespace blink {

static const struct {
    const char* name;
    uint32_t property;
} allowedProperties[] = {
    { "opacity", CompositorMutableProperty::kOpacity },
    { "scrollleft", CompositorMutableProperty::kScrollLeft },
    { "scrolltop", CompositorMutableProperty::kScrollTop },
    { "transform", CompositorMutableProperty::kTransform },
};

static uint32_t compositorMutablePropertyForName(const String& attributeName)
{
    for (const auto& mapping : allowedProperties) {
        if (equalIgnoringCase(mapping.name, attributeName))
            return mapping.property;
    }
    return CompositorMutableProperty::kNone;
}

static bool isControlThread()
{
    return !isMainThread();
}

static bool isCallingCompositorFrameCallback()
{
    // TODO(sad): Check that the requestCompositorFrame callbacks are currently being called.
    return true;
}

static void decrementCompositorProxiedPropertiesForElement(uint64_t elementId, uint32_t compositorMutableProperties)
{
    DCHECK(isMainThread());
    Node* node = DOMNodeIds::nodeForId(elementId);
    if (!node)
        return;
    Element* element = toElement(node);
    element->decrementCompositorProxiedProperties(compositorMutableProperties);
}

static void incrementCompositorProxiedPropertiesForElement(uint64_t elementId, uint32_t compositorMutableProperties)
{
    DCHECK(isMainThread());
    Node* node = DOMNodeIds::nodeForId(elementId);
    if (!node)
        return;
    Element* element = toElement(node);
    element->incrementCompositorProxiedProperties(compositorMutableProperties);
}

static bool raiseExceptionIfMutationNotAllowed(ExceptionState& exceptionState)
{
    if (!isControlThread()) {
        exceptionState.throwDOMException(NoModificationAllowedError, "Cannot mutate a proxy attribute from the main page.");
        return true;
    }
    if (!isCallingCompositorFrameCallback()) {
        exceptionState.throwDOMException(NoModificationAllowedError, "Cannot mutate a proxy attribute outside of a requestCompositorFrame callback.");
        return true;
    }
    return false;
}

static uint32_t compositorMutablePropertiesFromNames(const Vector<String>& attributeArray)
{
    uint32_t properties = 0;
    for (const auto& attribute : attributeArray) {
        properties |= compositorMutablePropertyForName(attribute);
    }
    return properties;
}

#if DCHECK_IS_ON()
static bool sanityCheckMutableProperties(uint32_t properties)
{
    // Ensures that we only have bits set for valid mutable properties.
    uint32_t sanityCheckProperties = properties;
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(allowedProperties); ++i) {
        sanityCheckProperties &= ~static_cast<uint32_t>(allowedProperties[i].property);
    }
    return !sanityCheckProperties;
}
#endif

CompositorProxy* CompositorProxy::create(ExecutionContext* context, Element* element, const Vector<String>& attributeArray, ExceptionState& exceptionState)
{
    if (!context->isDocument()) {
        exceptionState.throwTypeError(ExceptionMessages::failedToConstruct("CompositorProxy", "Can only be created from the main context."));
        exceptionState.throwIfNeeded();
        return nullptr;
    }

    return new CompositorProxy(*element, attributeArray);
}

CompositorProxy* CompositorProxy::create(uint64_t elementId, uint32_t compositorMutableProperties)
{
    return new CompositorProxy(elementId, compositorMutableProperties);
}

CompositorProxy::CompositorProxy(Element& element, const Vector<String>& attributeArray)
    : m_elementId(DOMNodeIds::idForNode(&element))
    , m_compositorMutableProperties(compositorMutablePropertiesFromNames(attributeArray))
{
    DCHECK(isMainThread());
    DCHECK(m_compositorMutableProperties);
#if DCHECK_IS_ON()
    DCHECK(sanityCheckMutableProperties(m_compositorMutableProperties));
#endif

    incrementCompositorProxiedPropertiesForElement(m_elementId, m_compositorMutableProperties);
}

CompositorProxy::CompositorProxy(uint64_t elementId, uint32_t compositorMutableProperties)
    : m_elementId(elementId)
    , m_compositorMutableProperties(compositorMutableProperties)
{
    DCHECK(isControlThread());
#if DCHECK_IS_ON()
    DCHECK(sanityCheckMutableProperties(m_compositorMutableProperties));
#endif
    Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&incrementCompositorProxiedPropertiesForElement, m_elementId, m_compositorMutableProperties));
}

CompositorProxy::~CompositorProxy()
{
    if (m_connected)
        disconnect();
}

bool CompositorProxy::supports(const String& attributeName) const
{
    return m_compositorMutableProperties & compositorMutablePropertyForName(attributeName);
}

double CompositorProxy::opacity(ExceptionState& exceptionState) const
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return 0.0;
    if (raiseExceptionIfNotMutable(CompositorMutableProperty::kOpacity, exceptionState))
        return 0.0;
    return m_opacity;
}

double CompositorProxy::scrollLeft(ExceptionState& exceptionState) const
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return 0.0;
    if (raiseExceptionIfNotMutable(CompositorMutableProperty::kScrollLeft, exceptionState))
        return 0.0;
    return m_scrollLeft;
}

double CompositorProxy::scrollTop(ExceptionState& exceptionState) const
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return 0.0;
    if (raiseExceptionIfNotMutable(CompositorMutableProperty::kScrollTop, exceptionState))
        return 0.0;
    return m_scrollTop;
}

DOMMatrix* CompositorProxy::transform(ExceptionState& exceptionState) const
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return nullptr;
    if (raiseExceptionIfNotMutable(CompositorMutableProperty::kTransform, exceptionState))
        return nullptr;
    return m_transform;
}

void CompositorProxy::setOpacity(double opacity, ExceptionState& exceptionState)
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return;
    if (raiseExceptionIfNotMutable(CompositorMutableProperty::kOpacity, exceptionState))
        return;
    m_opacity = std::min(1., std::max(0., opacity));
    m_mutatedProperties |= CompositorMutableProperty::kTransform;
}

void CompositorProxy::setScrollLeft(double scrollLeft, ExceptionState& exceptionState)
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return;
    if (raiseExceptionIfNotMutable(CompositorMutableProperty::kScrollLeft, exceptionState))
        return;
    m_scrollLeft = scrollLeft;
    m_mutatedProperties |= CompositorMutableProperty::kScrollLeft;
}

void CompositorProxy::setScrollTop(double scrollTop, ExceptionState& exceptionState)
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return;
    if (raiseExceptionIfNotMutable(CompositorMutableProperty::kScrollTop, exceptionState))
        return;
    m_scrollTop = scrollTop;
    m_mutatedProperties |= CompositorMutableProperty::kScrollTop;
}

void CompositorProxy::setTransform(DOMMatrix* transform, ExceptionState& exceptionState)
{
    if (raiseExceptionIfMutationNotAllowed(exceptionState))
        return;
    if (raiseExceptionIfNotMutable(CompositorMutableProperty::kTransform, exceptionState))
        return;
    m_transform = transform;
    m_mutatedProperties |= CompositorMutableProperty::kTransform;
}

bool CompositorProxy::raiseExceptionIfNotMutable(uint32_t property, ExceptionState& exceptionState) const
{
    if (m_connected && (m_compositorMutableProperties & property))
        return false;
    exceptionState.throwDOMException(NoModificationAllowedError,
        m_connected ? "Attempted to mutate non-mutable attribute." : "Attempted to mutate attribute on a disconnected proxy.");
    return true;
}

void CompositorProxy::disconnect()
{
    m_connected = false;
    if (isMainThread())
        decrementCompositorProxiedPropertiesForElement(m_elementId, m_compositorMutableProperties);
    else
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&decrementCompositorProxiedPropertiesForElement, m_elementId, m_compositorMutableProperties));
}

} // namespace blink
