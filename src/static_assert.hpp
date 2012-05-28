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
#ifndef TIGHTDB_STATIC_ASSERT_HPP
#define TIGHTDB_STATIC_ASSERT_HPP

#include "config.h"


#ifdef TIGHTDB_HAVE_CXX11_STATIC_ASSERT

#define TIGHTDB_STATIC_ASSERT(assertion, message) static_assert(assertion, message)

#else // !TIGHTDB_HAVE_CXX11_STATIC_ASSERT

#define TIGHTDB_STATIC_ASSERT(assertion, message) typedef \
    tightdb::static_assert_dummy<sizeof(tightdb::STATIC_ASSERTION_FAILURE<bool(assertion)>)> \
    _tightdb_static_assert_##__LINE__

namespace tightdb {
    template<bool> struct STATIC_ASSERTION_FAILURE;
    template<> struct STATIC_ASSERTION_FAILURE<true> {};
    template<int> struct static_assert_dummy {};
} // namespace tightdb

#endif // !TIGHTDB_HAVE_CXX11_STATIC_ASSERT


#endif // TIGHTDB_STATIC_ASSERT_HPP
