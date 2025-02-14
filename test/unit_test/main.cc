// Copyright (c) 2023 dingodb.com, Inc. All Rights Reserved
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include <iostream>

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);

  if (testing::FLAGS_gtest_filter == "*") {
    std::string default_run_case;
    default_run_case += "ConfigTest.*";
    default_run_case += ":HelperTest.*";
    default_run_case += ":DingoLatchTest.*";
    default_run_case += ":DingoLoggerTest.*";
    default_run_case += ":StorePbTest.*";
    default_run_case += ":WriteDataBuilderTest.*";
    default_run_case += ":FailPointManagerTest.*";
    default_run_case += ":FailPointTest.*";
    default_run_case += ":CoprocessorTest.*";
    default_run_case += ":CoprocessorUtilsTest.*";
    default_run_case += ":CoprocessorAggregationManagerTest.*";
    default_run_case += ":DingoSafeMapTest.*";
    default_run_case += ":SegmentLogStorageTest.*";
    default_run_case += ":DingoSerialListTypeTest.*";
    default_run_case += ":DingoSerialTest.*";
    default_run_case += ":ServiceHelperTest.*";
    default_run_case += ":SplitCheckerTest.*";

    // default_run_case += ":StoreRegionMetaTest.*";
    // default_run_case += ":StoreRegionMetricsTest.*";

    default_run_case += ":VectorIndexWrapperTest.*";
    default_run_case += ":VectorIndexUtilsTest.*";
    default_run_case += ":VectorIndexSnapshotTest.*";
    default_run_case += ":VectorIndexRawIvfPqTest.*";
    default_run_case += ":VectorIndexRawIvfPqBoundaryTest.*";
    default_run_case += ":VectorIndexMemoryTest.*";
    default_run_case += ":VectorIndexMemoryHnswTest.*";
    default_run_case += ":VectorIndexMemoryFlatTest.*";
    default_run_case += ":VectorIndexIvfPqTest.*";
    default_run_case += ":VectorIndexIvfFlatTest.*";
    default_run_case += ":VectorIndexHnswTest.*";
    default_run_case += ":VectorIndexHnswSearchParamTest.*";
    default_run_case += ":VectorIndexFlatTest.*";
    default_run_case += ":VectorIndexFlatSearchParamTest.*";
    default_run_case += ":VectorIndexFlatSearchParamLimitTest.*";
    default_run_case += ":TxnGcTest.*";

    testing::GTEST_FLAG(filter) = default_run_case;
  }

  return RUN_ALL_TESTS();
}