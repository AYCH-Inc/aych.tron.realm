/*************************************************************************
 *
 * TIGHTDB CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2012] TightDB Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of TightDB Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to TightDB Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from TightDB Incorporated.
 *
 **************************************************************************/

#include <tightdb/column_backlink.hpp>
#include <tightdb/column_link.hpp>

using namespace tightdb;

void ColumnBackLink::add_backlink(size_t row_ndx, size_t source_row_ndx)
{
    size_t ref = Column::get(row_ndx);

    // If there is only a single backlink, it can be stored as
    // a tagged value
    if (ref == 0) {
        size_t tagged_ndx = (source_row_ndx << 1) + 1;
        Column::set(row_ndx, tagged_ndx);
        return;
    }

    // if there already is a tagged value stored
    // we have to convert it to a list with room for
    // new backlink
    if (ref & 1) {
        // Create new column to hold backlinks
        ref_type col_ref = Column::create(Array::type_Normal, 0, 0, get_alloc());
        Column col(col_ref, this, row_ndx, get_alloc());

        size_t existing_source_ndx = (ref >> 1);
        col.add(existing_source_ndx);
        col.add(source_row_ndx);

        Column::set(row_ndx, col_ref);
        return;
    }

    // Otherwise just add to existing list
    Column col(ref, this, row_ndx, get_alloc());
    col.add(source_row_ndx);
}

std::size_t ColumnBackLink::get_backlink_count(size_t row_ndx) const TIGHTDB_NOEXCEPT
{
    size_t ref = Column::get(row_ndx);

    if (ref == 0)
        return 0;

    if (ref & 1)
        return 1;

    // get list size
    return ColumnBase::get_size_from_ref(ref, get_alloc());
}

std::size_t ColumnBackLink::get_backlink(size_t row_ndx, size_t backlink_ndx) const TIGHTDB_NOEXCEPT
{
    size_t ref = Column::get(row_ndx);
    TIGHTDB_ASSERT(ref != 0);

    if (ref & 1) {
        TIGHTDB_ASSERT(backlink_ndx == 0);

        size_t row_ref = (ref >> 1);
        return row_ref;
    }

    // return ref from list
    //TODO: optimize with direct access
    TIGHTDB_ASSERT(backlink_ndx < ColumnBase::get_size_from_ref(ref, get_alloc()));
    Column col(ref, null_ptr, 0, get_alloc());
    return col.get(backlink_ndx);
}

void ColumnBackLink::remove_backlink(size_t row_ndx, size_t source_row_ndx)
{
    size_t ref = Column::get(row_ndx);
    TIGHTDB_ASSERT(ref != 0);

    // If there is only a single backlink, it can be stored as
    // a tagged value
    if (ref & 1) {
        TIGHTDB_ASSERT(get_backlink(row_ndx, 0) == source_row_ndx);
        Column::set(row_ndx, 0);
        return;
    }

    // if there is a list of backlinks we have to find
    // the right one and remove it.
    Column col(ref, this, row_ndx, get_alloc());
    size_t ref_pos = col.find_first(source_row_ndx);
    TIGHTDB_ASSERT(ref_pos != not_found);
    bool is_last = (ref_pos+1 == size());
    col.erase(ref_pos, is_last);

    // if there is only one ref left we can inline it as tagged value
    if (col.size() == 1) {
        size_t last_value = col.get(0);
        col.destroy();

        size_t tagged_ndx = (last_value << 1) + 1;
        Column::set(row_ndx, tagged_ndx);
    }
}

void ColumnBackLink::update_backlink(std::size_t row_ndx, std::size_t old_row_ndx, std::size_t new_row_ndx) {
    size_t ref = Column::get(row_ndx);
    TIGHTDB_ASSERT(ref != 0);

    if (ref & 1) {
        TIGHTDB_ASSERT((ref >> 1) == old_row_ndx);
        size_t tagged_ndx = (new_row_ndx << 1) + 1;
        Column::set(row_ndx, tagged_ndx);
        return;
    }

    // find match in list and replace
    Column col(ref, this, row_ndx, get_alloc());
    size_t ref_pos = col.find_first(old_row_ndx);
    TIGHTDB_ASSERT(ref_pos != not_found);
    col.set(ref_pos, new_row_ndx);
}

void ColumnBackLink::nullify_links(std::size_t row_ndx, bool do_destroy)
{
    // Nullify all links pointing to the row being deleted
    size_t ref = Column::get(row_ndx);
    if (ref != 0) {
        if (ref & 1) {
            size_t row_ref = (ref >> 1);
            m_source_column->do_nullify_link(row_ref, row_ndx);
        }
        else {
            // nullify entire list of links
            Column col(ref, null_ptr, 0, get_alloc());
            size_t count = col.size();

            for (size_t i = 0; i < count; ++i) {
                size_t source_row_ref = col.get(i);
                m_source_column->do_nullify_link(source_row_ref, row_ndx);
            }

            if (do_destroy) {
                col.destroy();
            }
        }
    }
}

void ColumnBackLink::move_last_over(std::size_t row_ndx)
{
    TIGHTDB_ASSERT(row_ndx+1 < size());

    // Nullify all links pointing to the row being deleted
    nullify_links(row_ndx, true);

    // Update all links to the last row to point to the new row instead
    size_t last_row_ndx = size()-1;
    size_t ref = Column::get(last_row_ndx);
    if (ref != 0) {
        if (ref & 1) {
            size_t row_ref = (ref >> 1);
            m_source_column->do_update_link(row_ref, last_row_ndx, row_ndx);
        }
        else {
            // update entire list of links
            Column col(ref, null_ptr, 0, get_alloc());
            size_t count = col.size();

            for (size_t i = 0; i < count; ++i) {
                size_t source_row_ref = col.get(i);
                m_source_column->do_update_link(source_row_ref, last_row_ndx, row_ndx);
            }
        }
    }

    // Do the actual move
    Column::set(row_ndx, ref);
    Column::erase(last_row_ndx, true);
}

void ColumnBackLink::erase(std::size_t row_ndx, bool is_last)
{
    TIGHTDB_ASSERT(is_last);

    nullify_links(row_ndx, false);
    Column::erase(row_ndx, is_last);
}

void ColumnBackLink::clear()
{
    size_t count = size();
    for (size_t i = 0; i < count; ++i) {
        nullify_links(i, false);
    }
    Column::clear();
    // FIXME: This one is needed because Column::clear() forgets about
    // the leaf type. A better solution should probably be found.
    m_array->set_type(Array::type_HasRefs);
}

void ColumnBackLink::update_child_ref(size_t child_ndx, ref_type new_ref)
{
    Column::set(child_ndx, new_ref);
}

ref_type ColumnBackLink::get_child_ref(size_t child_ndx) const TIGHTDB_NOEXCEPT
{
    return Column::get(child_ndx);
}

#ifdef TIGHTDB_DEBUG

std::pair<ref_type, size_t> ColumnBackLink::get_to_dot_parent(size_t ndx_in_parent) const
{
    std::pair<MemRef, size_t> p = m_array->get_bptree_leaf(ndx_in_parent);
    return std::make_pair(p.first.m_ref, p.second);
}

#endif //TIGHTDB_DEBUG

