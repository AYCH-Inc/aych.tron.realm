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

#ifndef __TREE_HPP__
#define __TREE_HPP__

#include "refs.hpp"
#include "memory.hpp"

struct _TreeTop {
    uint64_t mask;
    uint64_t count;
    char levels;
    Ref<DynType> top_level;

    struct LeafCommitter {
        virtual Ref<DynType> commit(Ref<DynType> from) = 0;
    };

    void copied_to_file(Memory& mem, LeafCommitter& lc);
    void cow_path(Memory& mem, uint64_t index, Ref<DynType> leaf);

    // get leaf at index
    Ref<DynType> lookup(const Memory& mem, uint64_t index) const;

    void init(uint64_t capacity);

    void free(Memory& mem);

    static Ref<DynType> dispatch_commit(Memory& mem, Ref<DynType> from, int levels, LeafCommitter& lc);
};

#endif
