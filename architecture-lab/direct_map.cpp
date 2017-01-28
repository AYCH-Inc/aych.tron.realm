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

#include "direct_map.hpp"
#include "table.hpp"

template<typename _TEntry>
struct _DirectMapLeaf {
    uint16_t num_entries;
    uint8_t condenser_array[256];
    struct _Entry {
        uint64_t key;
        _TEntry entry;
    };
    // must come last: (holds not 1, but num_entries)
    _Entry entries[1];

    // helper functions
    static size_t get_size(int sz) {
        size_t res = sizeof(_DirectMapLeaf<_TEntry>) + (sz - 1) * sizeof(_Entry);
        return res;
    }

    int find(uint64_t key);
    bool is_empty(uint64_t key);
    void insert(Memory& mem, uint64_t key);
    static Ref<DynType> grow(Memory& mem, Ref<DynType> from);
    static Ref<DynType> commit(Memory& mem, Ref<DynType> from);
};

template<typename _TEntry>
Ref<DynType> _DirectMapLeaf<_TEntry>::grow(Memory& mem, Ref<DynType> from) {
    _DirectMapLeaf<_TEntry>* from_ptr = mem.txl(from.as<_DirectMapLeaf<_TEntry>>());
    int entries = from_ptr->num_entries;
    size_t cap = get_size(entries+1);
    _DirectMapLeaf<_TEntry>* to_ptr;
    Ref<_DirectMapLeaf<_TEntry>> to = mem.alloc<_DirectMapLeaf<_TEntry>>(to_ptr, cap);
    to_ptr->num_entries = from_ptr->num_entries;
    for (int j = 0; j < 256; ++j) to_ptr->condenser_array[j] = from_ptr->condenser_array[j];
    for (int j = 0; j < from_ptr->num_entries; ++j) {
        to_ptr->entries[j] = from_ptr->entries[j];
        to_ptr->entries[j].entry.copied_from_file(mem);
    }
    mem.free(from, get_size(entries));
    return to;
}

template<typename _TEntry>
Ref<DynType> _DirectMapLeaf<_TEntry>::commit(Memory& mem, Ref<DynType> from) {
    if (is_null(from)) return from;
    if (mem.is_writable(from)) {
        _DirectMapLeaf<_TEntry>* from_ptr = mem.txl(from.as<_DirectMapLeaf<_TEntry>>());
        _DirectMapLeaf<_TEntry>* to_ptr;
        size_t sz = get_size(from_ptr->num_entries);
        Ref<_DirectMapLeaf<_TEntry>> to = mem.alloc_in_file<_DirectMapLeaf<_TEntry>>(to_ptr, sz);
        to_ptr->num_entries = from_ptr->num_entries;
        for (int j = 0; j < 256; ++j) to_ptr->condenser_array[j] = from_ptr->condenser_array[j];
        for (int j = 0; j < from_ptr->num_entries; ++j) {
            to_ptr->entries[j] = from_ptr->entries[j];
            to_ptr->entries[j].entry.copied_to_file(mem);
        }
        mem.free(from, sz);
        return to;
    }
    return from;
}

template<typename _TEntry>
bool _DirectMapLeaf<_TEntry>::is_empty(uint64_t key) {
    uint8_t subhash = key;
    uint8_t idx = condenser_array[subhash];
    return idx >= num_entries;
}

template<typename _TEntry>
int _DirectMapLeaf<_TEntry>::find(uint64_t key) {
    uint8_t subhash = key;
    uint8_t idx = condenser_array[subhash];
    --idx; //
    if (idx < num_entries && entries[idx].key == key) {
        return idx;
    }
    return -1; // not found!
}

template<typename _TEntry>
void _DirectMapLeaf<_TEntry>::insert(Memory& mem, uint64_t key) {
    uint8_t subhash = key;
    int idx = num_entries;
    entries[idx].key = key;
    // actual element (entries[idx].entry) must be initialized by caller
    num_entries++;
    ++idx;
    condenser_array[subhash] = idx;
}

template<typename _TEntry>
_TEntry* _DirectMap<_TEntry>::get_ref(Memory& mem, uint64_t key) {
    using _TLeaf = _DirectMapLeaf<_TEntry>;
    Ref<_TLeaf> leaf = tree.lookup(mem, key).as<_TLeaf>();
    _TLeaf* leaf_ptr = mem.txl(leaf);
    int in_leaf_idx = leaf_ptr->find(key);
    if (in_leaf_idx >= 0)
        return &leaf_ptr->entries[in_leaf_idx].entry;
    throw NotFound();
}

template<typename _TEntry>
_TEntry _DirectMap<_TEntry>::get(Memory& mem, uint64_t key) const {
    using _TLeaf = _DirectMapLeaf<_TEntry>;
    Ref<_TLeaf> leaf = tree.lookup(mem, key).as<_TLeaf>();
    _TLeaf* leaf_ptr = mem.txl(leaf);
    int in_leaf_idx = leaf_ptr->find(key);
    if (in_leaf_idx >= 0)
        return leaf_ptr->entries[in_leaf_idx].entry;
    throw NotFound();
}

template<typename _TEntry>
uint64_t _DirectMap<_TEntry>::insert(Memory& mem) {
    using _TLeaf = _DirectMapLeaf<_TEntry>;
    for (;;) {
        uint64_t key;
        key = rand(); //replace with something better
        Ref<DynType> leaf = tree.lookup(mem, key);
        _TLeaf* leaf_ptr = mem.txl(leaf.as<_TLeaf>());
        bool found = leaf_ptr->is_empty(key);
        if (!found)
            continue;
        leaf = _TLeaf::grow(mem, leaf);
        leaf_ptr = mem.txl(leaf.as<_TLeaf>());
        tree.cow_path(mem, key, leaf);
        leaf_ptr->insert(mem, key);
        ++tree.count;
        return key;
    }
}

template<typename _TEntry>
void _DirectMap<_TEntry>::cow_path(Memory& mem, uint64_t key) {
    using _TLeaf = _DirectMapLeaf<_TEntry>;
    Ref<DynType> leaf = tree.lookup(mem, key);
    _TLeaf* leaf_ptr = mem.txl(leaf.as<_TLeaf>());
    bool found = leaf_ptr->is_empty(key);
    if (!found)
        throw NotFound();
    if (!mem.is_writable(leaf)) {
        leaf = _TLeaf::grow(mem, leaf);
        leaf_ptr = mem.txl(leaf.as<_TLeaf>());
        tree.cow_path(mem, key, leaf);
    }
    // why was this here: leaf_ptr->insert(mem, key);
}

template<typename _TEntry>
struct LeafCommitter : public _TreeTop::LeafCommitter {
    virtual Ref<DynType> commit(Ref<DynType> from) { return _DirectMapLeaf<_TEntry>::commit(mem, from); }
    Memory& mem;
    LeafCommitter(Memory& mem) : mem(mem) {};
};

template<typename _TEntry>
void _DirectMap<_TEntry>::copied_to_file(Memory& mem) {
    LeafCommitter<_TEntry> cmt(mem);
    tree.copied_to_file(mem, cmt);
}

template<typename _TEntry>
void _DirectMap<_TEntry>::init(size_t initial_size) {
    tree.init(initial_size);
}

// Explicit instantiations:
template class _DirectMap<Ref<_Table>>;
