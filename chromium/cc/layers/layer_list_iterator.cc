// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_list_iterator.h"

#include "cc/layers/layer_impl.h"

namespace cc {

LayerListIterator::LayerListIterator(LayerImpl* root_layer)
    : current_layer_(root_layer) {
  DCHECK(!root_layer || !root_layer->parent());
  list_indices_.push_back(0);
}

LayerListIterator::LayerListIterator(const LayerListIterator& other) = default;

LayerListIterator::~LayerListIterator() {}

LayerListIterator& LayerListIterator::operator++() {
  // case 0: done
  if (!current_layer_)
    return *this;

  // case 1: descend.
  const LayerImplList& current_list = current_layer_->children();
  if (!current_list.empty()) {
    current_layer_ = current_list[0];
    list_indices_.push_back(0);
    return *this;
  }

  for (LayerImpl* parent = current_layer_->parent(); parent;
       parent = parent->parent()) {
    // We now try and advance in some list of siblings.
    const LayerImplList& sibling_list = parent->children();

    // case 2: Advance to a sibling.
    if (list_indices_.back() + 1 < sibling_list.size()) {
      ++list_indices_.back();
      current_layer_ = sibling_list[list_indices_.back()];
      return *this;
    }

    // We need to ascend. We will pop an index off the stack.
    list_indices_.pop_back();
  }

  current_layer_ = nullptr;
  return *this;
}

LayerListReverseIterator::LayerListReverseIterator(LayerImpl* root_layer)
    : LayerListIterator(root_layer) {
  DescendToRightmostInSubtree();
}

LayerListReverseIterator::~LayerListReverseIterator() {}

// We will only support prefix increment.
LayerListIterator& LayerListReverseIterator::operator++() {
  // case 0: done
  if (!current_layer_)
    return *this;

  // case 1: we're the leftmost sibling.
  if (!list_indices_.back()) {
    list_indices_.pop_back();
    current_layer_ = current_layer_->parent();
    return *this;
  }

  // case 2: we're not the leftmost sibling. In this case, we want to move one
  // sibling over, and then descend to the rightmost descendant in that subtree.
  CHECK(current_layer_->parent());
  --list_indices_.back();
  const LayerImplList& parent_list = current_layer_->parent()->children();
  current_layer_ = parent_list[list_indices_.back()];
  DescendToRightmostInSubtree();
  return *this;
}

void LayerListReverseIterator::DescendToRightmostInSubtree() {
  if (!current_layer_)
    return;

  const LayerImplList& current_list = current_layer_->children();
  if (current_list.empty())
    return;

  size_t last_index = current_list.size() - 1;
  current_layer_ = current_list[last_index];
  list_indices_.push_back(last_index);
  DescendToRightmostInSubtree();
}

}  // namespace cc
