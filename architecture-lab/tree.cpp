/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#include <cstdint>
#include "tree.hpp"

// interior node of radix tree
struct _TreeNode {
    Ref<DynType> next_level[256];

    // Fixme: put cow operation here!
    static Ref<DynType> commit(Memory& mem, Ref<DynType> from, int levels, _TreeTop::LeafCommitter& lc);
};


void _TreeTop::copied_to_file(Memory& mem, LeafCommitter& lc) {
    // this is sort the inverse of cow_path
    top_level = dispatch_commit(mem, top_level, levels, lc);
}

Ref<DynType> _TreeTop::dispatch_commit(Memory& mem, Ref<DynType> from, int levels, 
                                       _TreeTop::LeafCommitter& lc) {
    if (is_null(from)) return from;
    if (levels == 1) {
        return lc.commit(from);
    } else {
        return _TreeNode::commit(mem, from, levels, lc);
    }
}

Ref<DynType> _TreeNode::commit(Memory& mem, Ref<DynType> from, int levels, _TreeTop::LeafCommitter& lc) {
    if (mem.is_writable(from)) {
        _TreeNode* to_ptr;
        Ref<_TreeNode> to = mem.alloc_in_file<_TreeNode>(to_ptr);
        _TreeNode* from_ptr = mem.txl(from.as<_TreeNode>());
        for (int i=0; i<256; ++i) {
            to_ptr->next_level[i] = _TreeTop::dispatch_commit(mem, from_ptr->next_level[i], levels-1, lc);
        }
        mem.free(from);
        return to;
    }
    return from;
}

void _TreeTop::init(uint64_t capacity) {
    int bits = 4; //minimal size of tree is 16
    while ((1ULL<<bits) < capacity) ++bits;
    mask = (1ULL << bits) - 1;
    count = 0;
    levels = 1 + ((bits-1)/8);
    top_level = Ref<DynType>();
}

inline Ref<DynType> step(const Memory& mem, Ref<DynType> ref, uint64_t masked_index, int shift) {
    Ref<_TreeNode> r = ref.as<_TreeNode>();
    unsigned char c = masked_index >> shift;
    ref = mem.txl(r)->next_level[c];
    return ref;
}

inline Ref<DynType> step_with_trace(const Memory& mem, Ref<DynType> ref, uint64_t masked_index,
				    int shift, Ref<DynType>*& tracking) {
    Ref<_TreeNode> r = ref.as<_TreeNode>();
    unsigned char c = masked_index >> shift;
    _TreeNode* ptr = mem.txl(r);
    ref = ptr->next_level[c];
    tracking = &ptr->next_level[c];
    return ref;
}

// Get leaf at index:
Ref<DynType> _TreeTop::lookup(const Memory& mem, uint64_t index) const {
    Ref<DynType> ref = top_level;
    uint64_t masked_index = index & mask;
    switch (levels) {
        case 8:
            ref = step(mem, ref, masked_index, 56);
        case 7:
            ref = step(mem, ref, masked_index, 48);
        case 6:
            ref = step(mem, ref, masked_index, 40);
        case 5:
            ref = step(mem, ref, masked_index, 32);
        case 4:
            ref = step(mem, ref, masked_index, 24);
        case 3:
            ref = step(mem, ref, masked_index, 16);
        case 2:
            ref = step(mem, ref, masked_index, 8);
        case 1:
            return ref;
        case 0:
            return Ref<DynType>(); // empty tree <-- this should assert!
    }
    return Ref<DynType>(); // not possible
}

// Set leaf at index:
// copy-on-write the path from the tree top to the leaf, but not the top or leaf themselves.
// caller is responsible for copy-on-writing the leaf PRIOR to the call, and for
// copy-on-writing the top PRIOR to the call so that it can be updated.
void _TreeTop::cow_path(Memory& mem, uint64_t index, Ref<DynType> leaf) {
    Ref<DynType> ref = top_level;
    Ref<DynType>* tracking_ref = &top_level;
    uint64_t masked_index = index & mask;
    int _levels = levels;
    int shifts = _levels * 8 - 8;
    while (_levels > 1) {
        // copy on write each interior node
        if (!mem.is_writable(ref)) {
            Ref<_TreeNode> old_ref = ref.as<_TreeNode>();
            _TreeNode* new_node;
            ref = mem.alloc<_TreeNode>(new_node);
            _TreeNode* old_node = mem.txl(old_ref);
            *new_node = *old_node;
            *tracking_ref = ref;
            mem.free(old_ref);
        }
        ref = step_with_trace(mem, ref, masked_index, shifts, tracking_ref);
        shifts -= 8;
        --_levels;
    }
    // leaf node:
    *tracking_ref = leaf;
}

// release all interior nodes of tree - leafs should have been
// removed/released before calling free_tree. The tree must
// be made writable before calling free_tree.
void free_tree_internal(int level, Memory& mem, Ref<DynType> ref) {
    Ref<_TreeNode> tree_node = ref.as<_TreeNode>();
    _TreeNode* node_ptr = mem.txl(tree_node);
    if (level > 2) {
        for (int j=0; j<256; ++j) {
            if (node_ptr)
                free_tree_internal(level - 1, mem, node_ptr->next_level[j]);
        }
    }
    mem.free(ref);
}

void _TreeTop::free(Memory& mem) {
    if (levels > 1)
        free_tree_internal(levels, mem, top_level);
    top_level = Ref<DynType>();
}

