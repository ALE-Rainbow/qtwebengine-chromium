// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreeBuilder.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutView.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

// The context for layout tree walk.
// The walk will be done in the primary tree order (= DOM order), thus the context will also be
// responsible for bookkeeping tree state in other order, for example, the most recent position
// container seen.
struct PaintPropertyTreeBuilderContext {
    PaintPropertyTreeBuilderContext()
        : currentTransform(nullptr)
        , currentClip(nullptr)
        , transformForAbsolutePosition(nullptr)
        , clipForAbsolutePosition(nullptr)
        , transformForFixedPosition(nullptr)
        , clipForFixedPosition(nullptr)
        , currentEffect(nullptr) { }

    // The combination of a transform and paint offset describes a linear space.
    // When a layout object recur to its children, the main context is expected to refer
    // the object's border box, then the callee will derive its own border box by translating
    // the space with its own layout location.
    TransformPaintPropertyNode* currentTransform;
    LayoutPoint paintOffset;
    // The clip node describes the accumulated raster clip for the current subtree.
    // Note that the computed raster region in canvas space for a clip node is independent from
    // the transform and paint offset above. Also the actual raster region may be affected
    // by layerization and occlusion tracking.
    ClipPaintPropertyNode* currentClip;

    // Separate context for out-of-flow positioned and fixed positioned elements are needed
    // because they don't use DOM parent as their containing block.
    // These additional contexts normally pass through untouched, and are only copied from
    // the main context when the current element serves as the containing block of corresponding
    // positioned descendants.
    // Overflow clips are also inherited by containing block tree instead of DOM tree, thus they
    // are included in the additional context too.
    TransformPaintPropertyNode* transformForAbsolutePosition;
    LayoutPoint paintOffsetForAbsolutePosition;
    ClipPaintPropertyNode* clipForAbsolutePosition;

    TransformPaintPropertyNode* transformForFixedPosition;
    LayoutPoint paintOffsetForFixedPosition;
    ClipPaintPropertyNode* clipForFixedPosition;

    // The effect hierarchy is applied by the stacking context tree. It is guaranteed that every
    // DOM descendant is also a stacking context descendant. Therefore, we don't need extra
    // bookkeeping for effect nodes and can generate the effect tree from a DOM-order traversal.
    EffectPaintPropertyNode* currentEffect;
};

void PaintPropertyTreeBuilder::buildPropertyTrees(FrameView& rootFrame)
{
    walk(rootFrame, PaintPropertyTreeBuilderContext());
}

void PaintPropertyTreeBuilder::walk(FrameView& frameView, const PaintPropertyTreeBuilderContext& context)
{
    PaintPropertyTreeBuilderContext localContext(context);

    // TODO(pdr): Creating paint properties for FrameView here will not be
    // needed once settings()->rootLayerScrolls() is enabled.
    // TODO(pdr): Make this conditional on the rootLayerScrolls setting.

    TransformationMatrix frameTranslate;
    frameTranslate.translate(frameView.x() + context.paintOffset.x(), frameView.y() + context.paintOffset.y());
    RefPtr<TransformPaintPropertyNode> newTransformNodeForPreTranslation = TransformPaintPropertyNode::create(frameTranslate, FloatPoint3D(), context.currentTransform);
    localContext.transformForFixedPosition = newTransformNodeForPreTranslation.get();
    localContext.paintOffsetForFixedPosition = LayoutPoint();

    FloatRoundedRect contentClip(IntRect(IntPoint(), frameView.visibleContentSize()));
    RefPtr<ClipPaintPropertyNode> newClipNodeForContentClip = ClipPaintPropertyNode::create(newTransformNodeForPreTranslation.get(), contentClip, localContext.currentClip);
    localContext.currentClip = localContext.clipForAbsolutePosition = localContext.clipForFixedPosition = newClipNodeForContentClip.get();

    DoubleSize scrollOffset = frameView.scrollOffsetDouble();
    TransformationMatrix frameScroll;
    frameScroll.translate(-scrollOffset.width(), -scrollOffset.height());
    RefPtr<TransformPaintPropertyNode> newTransformNodeForScrollTranslation = TransformPaintPropertyNode::create(frameScroll, FloatPoint3D(), newTransformNodeForPreTranslation);
    localContext.currentTransform = localContext.transformForAbsolutePosition = newTransformNodeForScrollTranslation.get();
    localContext.paintOffset = localContext.paintOffsetForAbsolutePosition = LayoutPoint();

    frameView.setPreTranslation(newTransformNodeForPreTranslation.release());
    frameView.setScrollTranslation(newTransformNodeForScrollTranslation.release());
    frameView.setContentClip(newClipNodeForContentClip.release());

    if (LayoutView* layoutView = frameView.layoutView())
        walk(*layoutView, localContext);
}

static void deriveBorderBoxFromContainerContext(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBoxModelObject())
        return;

    const LayoutBoxModelObject& boxModelObject = toLayoutBoxModelObject(object);

    switch (object.styleRef().position()) {
    case StaticPosition:
        break;
    case RelativePosition:
        context.paintOffset += boxModelObject.offsetForInFlowPosition();
        break;
    case AbsolutePosition:
        context.currentTransform = context.transformForAbsolutePosition;
        context.paintOffset = context.paintOffsetForAbsolutePosition;
        context.currentClip = context.clipForAbsolutePosition;
        break;
    case StickyPosition:
        context.paintOffset += boxModelObject.offsetForInFlowPosition();
        break;
    case FixedPosition:
        context.currentTransform = context.transformForFixedPosition;
        context.paintOffset = context.paintOffsetForFixedPosition;
        context.currentClip = context.clipForFixedPosition;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    if (boxModelObject.isBox()) {
        context.paintOffset += toLayoutBox(boxModelObject).locationOffset();
        // This is a weird quirk that table cells paint as children of table rows,
        // but their location have the row's location baked-in.
        // Similar adjustment is done in LayoutTableCell::offsetFromContainer().
        if (boxModelObject.isTableCell()) {
            LayoutObject* parentRow = boxModelObject.parent();
            ASSERT(parentRow && parentRow->isTableRow());
            context.paintOffset -= toLayoutBox(parentRow)->locationOffset();
        }
    }
}

static PassRefPtr<TransformPaintPropertyNode> createPaintOffsetTranslationIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    bool shouldCreatePaintOffsetTranslationNode = false;
    if (object.isSVGRoot()) {
        // SVG doesn't use paint offset internally so emit a paint offset at the html->svg boundary.
        shouldCreatePaintOffsetTranslationNode = true;
    } else if (object.isBoxModelObject()) {
        // TODO(trchen): Eliminate PaintLayer dependency.
        PaintLayer* layer = toLayoutBoxModelObject(object).layer();
        shouldCreatePaintOffsetTranslationNode = layer && layer->paintsWithTransform(GlobalPaintNormalPhase);
    }

    if (context.paintOffset == LayoutPoint() || !shouldCreatePaintOffsetTranslationNode)
        return nullptr;

    RefPtr<TransformPaintPropertyNode> newTransformNodeForPaintOffsetTranslation = TransformPaintPropertyNode::create(
        TransformationMatrix().translate(context.paintOffset.x(), context.paintOffset.y()),
        FloatPoint3D(), context.currentTransform);
    context.currentTransform = newTransformNodeForPaintOffsetTranslation.get();
    context.paintOffset = LayoutPoint();
    return newTransformNodeForPaintOffsetTranslation.release();
}

static FloatPoint3D transformOrigin(const LayoutBox& box)
{
    const ComputedStyle& style = box.styleRef();
    FloatSize borderBoxSize(box.size());
    return FloatPoint3D(
        floatValueForLength(style.transformOriginX(), borderBoxSize.width()),
        floatValueForLength(style.transformOriginY(), borderBoxSize.height()),
        style.transformOriginZ());
}

static PassRefPtr<TransformPaintPropertyNode> createTransformIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    const ComputedStyle& style = object.styleRef();
    if (!object.isBox() || !style.hasTransform())
        return nullptr;

    ASSERT(context.paintOffset == LayoutPoint());

    TransformationMatrix matrix;
    style.applyTransform(matrix, toLayoutBox(object).size(), ComputedStyle::ExcludeTransformOrigin,
        ComputedStyle::IncludeMotionPath, ComputedStyle::IncludeIndependentTransformProperties);
    RefPtr<TransformPaintPropertyNode> newTransformNodeForTransform = TransformPaintPropertyNode::create(
        matrix, transformOrigin(toLayoutBox(object)), context.currentTransform);
    context.currentTransform = newTransformNodeForTransform.get();
    return newTransformNodeForTransform.release();
}

static PassRefPtr<TransformPaintPropertyNode> createSvgLocalTransformIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isSVG())
        return nullptr;

    const AffineTransform& transform = object.localToSVGParentTransform();
    if (transform.isIdentity())
        return nullptr;

    // SVG's transform origin is baked into the localToSVGParentTransform so we use 0's for the origin.
    RefPtr<TransformPaintPropertyNode> newTransformNodeForTransform = TransformPaintPropertyNode::create(
        transform, FloatPoint3D(0, 0, 0), context.currentTransform);
    context.currentTransform = newTransformNodeForTransform.get();
    return newTransformNodeForTransform.release();
}

static PassRefPtr<EffectPaintPropertyNode> createEffectIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    const ComputedStyle& style = object.styleRef();
    if (!style.hasOpacity())
        return nullptr;
    RefPtr<EffectPaintPropertyNode> newEffectNode = EffectPaintPropertyNode::create(style.opacity(), context.currentEffect);
    context.currentEffect = newEffectNode.get();
    return newEffectNode.release();
}

static PassRefPtr<ClipPaintPropertyNode> createCSSClipIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.hasClip())
        return nullptr;
    ASSERT(object.canContainAbsolutePositionObjects());

    // Create clip node for descendants that are not fixed position.
    // We don't have to setup context.clipForAbsolutePosition here because this object must be
    // a container for absolute position descendants, and will copy from in-flow context later
    // at updateOutOfFlowContext() step.
    LayoutRect clipRect = toLayoutBox(object).clipRect(context.paintOffset);
    RefPtr<ClipPaintPropertyNode> newClipNodeForCSSClip = ClipPaintPropertyNode::create(
        context.currentTransform,
        FloatRoundedRect(FloatRect(clipRect)),
        context.currentClip);
    context.currentClip = newClipNodeForCSSClip.get();

    return newClipNodeForCSSClip.release();
}

// TODO(trchen): Remove this once we bake the paint offset into frameRect.
static PassRefPtr<TransformPaintPropertyNode> createScrollbarPaintOffsetIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    IntPoint roundedPaintOffset = roundedIntPoint(context.paintOffset);
    if (roundedPaintOffset == IntPoint())
        return nullptr;

    if (!object.isBoxModelObject())
        return nullptr;
    PaintLayerScrollableArea* scrollableArea = toLayoutBoxModelObject(object).getScrollableArea();
    if (!scrollableArea)
        return nullptr;
    if (!scrollableArea->horizontalScrollbar() && !scrollableArea->verticalScrollbar())
        return nullptr;

    return TransformPaintPropertyNode::create(
        TransformationMatrix().translate(roundedPaintOffset.x(), roundedPaintOffset.y()),
        FloatPoint3D(), context.currentTransform);
}

static PassRefPtr<ClipPaintPropertyNode> createOverflowClipIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBox())
        return nullptr;
    const LayoutBox& box = toLayoutBox(object);

    // The <input> elements can't have contents thus CSS overflow property doesn't apply.
    // However for layout purposes we do generate child layout objects for them, e.g. button label.
    // We should clip the overflow from those children. This is called control clip and we
    // technically treat them like overflow clip.
    LayoutRect clipRect;
    if (box.hasControlClip())
        clipRect = box.controlClipRect(context.paintOffset);
    else if (box.hasOverflowClip())
        clipRect = box.overflowClipRect(context.paintOffset);
    else
        return nullptr;

    RefPtr<ClipPaintPropertyNode> newClipNodeForBorderRadiusClip;
    const ComputedStyle& style = box.styleRef();
    if (style.hasBorderRadius()) {
        newClipNodeForBorderRadiusClip = ClipPaintPropertyNode::create(
            context.currentTransform,
            style.getRoundedInnerBorderFor(LayoutRect(context.paintOffset, box.size())),
            context.currentClip);
    }

    RefPtr<ClipPaintPropertyNode> newClipNodeForOverflowClip = ClipPaintPropertyNode::create(
        context.currentTransform,
        FloatRoundedRect(FloatRect(clipRect)),
        newClipNodeForBorderRadiusClip ? newClipNodeForBorderRadiusClip.release() : context.currentClip);
    context.currentClip = newClipNodeForOverflowClip.get();
    return newClipNodeForOverflowClip.release();
}

static FloatPoint perspectiveOrigin(const LayoutBox& box)
{
    const ComputedStyle& style = box.styleRef();
    FloatSize borderBoxSize(box.size());
    return FloatPoint(
        floatValueForLength(style.perspectiveOriginX(), borderBoxSize.width()),
        floatValueForLength(style.perspectiveOriginY(), borderBoxSize.height()));
}

static PassRefPtr<TransformPaintPropertyNode> createPerspectiveIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    const ComputedStyle& style = object.styleRef();
    if (!object.isBox() || !style.hasPerspective())
        return nullptr;

    RefPtr<TransformPaintPropertyNode> newTransformNodeForPerspective = TransformPaintPropertyNode::create(
        TransformationMatrix().applyPerspective(style.perspective()),
        perspectiveOrigin(toLayoutBox(object)) + toLayoutSize(context.paintOffset), context.currentTransform);
    context.currentTransform = newTransformNodeForPerspective.get();
    return newTransformNodeForPerspective.release();
}

static PassRefPtr<TransformPaintPropertyNode> createScrollTranslationIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBoxModelObject() || !object.hasOverflowClip())
        return nullptr;

    PaintLayer* layer = toLayoutBoxModelObject(object).layer();
    ASSERT(layer);
    DoubleSize scrollOffset = layer->getScrollableArea()->scrollOffset();
    if (scrollOffset.isZero() && !layer->scrollsOverflow())
        return nullptr;

    RefPtr<TransformPaintPropertyNode> newTransformNodeForScrollTranslation = TransformPaintPropertyNode::create(
        TransformationMatrix().translate(-scrollOffset.width(), -scrollOffset.height()),
        FloatPoint3D(), context.currentTransform);
    context.currentTransform = newTransformNodeForScrollTranslation.get();
    return newTransformNodeForScrollTranslation.release();
}

static void updateOutOfFlowContext(const LayoutObject& object, PaintPropertyTreeBuilderContext& context, ClipPaintPropertyNode* newClipNodeForCSSClip, RefPtr<ClipPaintPropertyNode>& newClipNodeForCSSClipFixedPosition)
{
    // TODO(pdr): Always create an SVG transform for the root and remove this paint offset quirk.
    // At the html->svg boundary (see: createPaintOffsetTranslationIfNeeded) the currentTransform is
    // up-to-date for all children of the svg root element. Additionally, inside SVG, all positioning
    // uses transforms. Therefore, we only need to check createdNewTransform and isSVGRoot() to
    // ensure out-of-flow and fixed positioning is correct at the svg->html boundary.

    if (object.canContainAbsolutePositionObjects()) {
        context.transformForAbsolutePosition = context.currentTransform;
        context.paintOffsetForAbsolutePosition = context.paintOffset;
        context.clipForAbsolutePosition = context.currentClip;
    }

    // TODO(pdr): Remove the !object.isLayoutView() condition when removing FrameView
    // paint properties for rootLayerScrolls.
    if (!object.isLayoutView() && object.canContainFixedPositionObjects()) {
        context.transformForFixedPosition = context.currentTransform;
        context.paintOffsetForFixedPosition = context.paintOffset;
        context.clipForFixedPosition = context.currentClip;
    } else if (newClipNodeForCSSClip) {
        // CSS clip applies to all descendants, even if this object is not a containing block
        // ancestor of the descendant. It is okay for absolute-position descendants because
        // having CSS clip implies being absolute position container. However for fixed-position
        // descendants we need to insert the clip here if we are not a containing block ancestor
        // of them.

        // Before we actually create anything, check whether in-flow context and fixed-position
        // context has exactly the same clip. Reuse if possible.
        if (context.clipForFixedPosition == newClipNodeForCSSClip->parent()) {
            context.clipForFixedPosition = newClipNodeForCSSClip;
            return;
        }

        newClipNodeForCSSClipFixedPosition = ClipPaintPropertyNode::create(
            const_cast<TransformPaintPropertyNode*>(newClipNodeForCSSClip->localTransformSpace()),
            newClipNodeForCSSClip->clipRect(),
            context.clipForFixedPosition);
        context.clipForFixedPosition = newClipNodeForCSSClipFixedPosition.get();
    }
}

static PassOwnPtr<ObjectPaintProperties::LocalBorderBoxProperties> recordTreeContextIfNeeded(LayoutObject& object, const PaintPropertyTreeBuilderContext& context)
{
    // Note: Currently only layer painter makes use of the pre-computed context.
    // This condition may be loosened with no adverse effects beside memory use.
    if (!object.hasLayer())
        return nullptr;

    OwnPtr<ObjectPaintProperties::LocalBorderBoxProperties> recordedContext = adoptPtr(new ObjectPaintProperties::LocalBorderBoxProperties);
    recordedContext->paintOffset = context.paintOffset;
    recordedContext->transform = context.currentTransform;
    recordedContext->clip = context.currentClip;
    recordedContext->effect = context.currentEffect;
    return recordedContext.release();
}

void PaintPropertyTreeBuilder::walk(LayoutObject& object, const PaintPropertyTreeBuilderContext& context)
{
    PaintPropertyTreeBuilderContext localContext(context);

    deriveBorderBoxFromContainerContext(object, localContext);
    RefPtr<TransformPaintPropertyNode> newTransformNodeForPaintOffsetTranslation = createPaintOffsetTranslationIfNeeded(object, localContext);
    RefPtr<TransformPaintPropertyNode> newTransformNodeForTransform = createTransformIfNeeded(object, localContext);
    RefPtr<EffectPaintPropertyNode> newEffectNode = createEffectIfNeeded(object, localContext);
    RefPtr<ClipPaintPropertyNode> newClipNodeForCSSClip = createCSSClipIfNeeded(object, localContext);
    OwnPtr<ObjectPaintProperties::LocalBorderBoxProperties> newRecordedContext = recordTreeContextIfNeeded(object, localContext);
    RefPtr<TransformPaintPropertyNode> newTransformNodeForScrollbarPaintOffset = createScrollbarPaintOffsetIfNeeded(object, localContext);
    RefPtr<ClipPaintPropertyNode> newClipNodeForOverflowClip = createOverflowClipIfNeeded(object, localContext);
    // TODO(trchen): Insert flattening transform here, as specified by
    // http://www.w3.org/TR/css3-transforms/#transform-style-property
    RefPtr<TransformPaintPropertyNode> newTransformNodeForPerspective = createPerspectiveIfNeeded(object, localContext);
    RefPtr<TransformPaintPropertyNode> newTransformNodeForSvgLocalTransform = createSvgLocalTransformIfNeeded(object, localContext);
    RefPtr<TransformPaintPropertyNode> newTransformNodeForScrollTranslation = createScrollTranslationIfNeeded(object, localContext);
    RefPtr<ClipPaintPropertyNode> newClipNodeForCSSClipFixedPosition;
    updateOutOfFlowContext(object, localContext, newClipNodeForCSSClip.get(), newClipNodeForCSSClipFixedPosition);

    DCHECK(!newTransformNodeForSvgLocalTransform || !newTransformNodeForScrollTranslation)
        << "SVG elements cannot have scroll translation";

    // TODO(pdr): Refactor this to use a less massive if statement.
    if (newTransformNodeForPaintOffsetTranslation || newTransformNodeForTransform || newEffectNode || newClipNodeForCSSClip || newClipNodeForCSSClipFixedPosition || newClipNodeForOverflowClip || newTransformNodeForPerspective || newTransformNodeForSvgLocalTransform || newTransformNodeForScrollTranslation || newTransformNodeForScrollbarPaintOffset || newRecordedContext) {
        OwnPtr<ObjectPaintProperties> updatedPaintProperties = ObjectPaintProperties::create(
            newTransformNodeForPaintOffsetTranslation.release(),
            newTransformNodeForTransform.release(),
            newEffectNode.release(),
            newClipNodeForCSSClip.release(),
            newClipNodeForCSSClipFixedPosition.release(),
            newClipNodeForOverflowClip.release(),
            newTransformNodeForPerspective.release(),
            newTransformNodeForSvgLocalTransform.release(),
            newTransformNodeForScrollTranslation.release(),
            newTransformNodeForScrollbarPaintOffset.release(),
            newRecordedContext.release());
        object.setObjectPaintProperties(updatedPaintProperties.release());
    } else {
        object.clearObjectPaintProperties();
    }

    for (LayoutObject* child = object.slowFirstChild(); child; child = child->nextSibling()) {
        if (child->isBoxModelObject() || child->isSVG())
            walk(*child, localContext);
    }

    if (object.isLayoutPart()) {
        Widget* widget = toLayoutPart(object).widget();
        if (widget && widget->isFrameView())
            walk(*toFrameView(widget), localContext);
        // TODO(pdr): Investigate RemoteFrameView (crbug.com/579281).
    }
}

} // namespace blink
