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
#ifndef TIGHTDB_TEST_UTIL_TEST_ONLY_HPP
#define TIGHTDB_TEST_UTIL_TEST_ONLY_HPP

#include "unit_test.hpp"

#define ONLY(name) \
    tightdb::test_util::SetTestOnly tightdb_set_test_only__##name(#name); \
    TEST(name)

namespace tightdb {
namespace test_util {

struct SetTestOnly {
    SetTestOnly(const char* test_name);
};

const char* get_test_only();

} // namespace test_util
} // namespace tightdb

#endif // TIGHTDB_TEST_UTIL_TEST_ONLY_HPP
