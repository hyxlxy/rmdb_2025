
# index test
add_executable(b_plus_tree_insert_test index/b_plus_tree_insert_test.cpp)
target_link_libraries(b_plus_tree_insert_test system index transaction gtest_main)

add_executable(b_plus_tree_delete_test index/b_plus_tree_delete_test.cpp)
target_link_libraries(b_plus_tree_delete_test system index transaction gtest_main)

add_executable(b_plus_tree_concurrent_test index/b_plus_tree_concurrent_test.cpp)
target_link_libraries(b_plus_tree_concurrent_test system index transaction gtest_main)

# # MVCC tests
# # add_executable(mvcc_test mvcc_test.cpp)
# # target_link_libraries(mvcc_test system transaction records gtest_main)

# # add_executable(mvcc_demo mvcc_demo.cpp)
# # target_link_libraries(mvcc_demo system transaction records)

# add_executable(simple_mvcc_test simple_mvcc_test.cpp)
# target_link_libraries(simple_mvcc_test system transaction records)

# add_executable(mvcc_concurrent_test mvcc_concurrent_test.cpp)
# target_link_libraries(mvcc_concurrent_test system transaction records)

# add_executable(mvcc_basic_test mvcc_basic_test.cpp)
# target_link_libraries(mvcc_basic_test system transaction records)

# add_executable(mvcc_sql_test mvcc_sql_test.cpp)
# target_link_libraries(mvcc_sql_test system transaction records)

# add_executable(mvcc_output_test mvcc_output_test.cpp)
# target_link_libraries(mvcc_output_test system transaction records)

# add_executable(mvcc_final_test mvcc_final_test.cpp)
# target_link_libraries(mvcc_final_test system transaction records)

# add_executable(mvcc_comprehensive_test mvcc_comprehensive_test.cpp)
# target_link_libraries(mvcc_comprehensive_test system transaction records)

# # MVCC脏读测试
# add_executable(mvcc_dirty_read_test mvcc_dirty_read_test.cpp)
# target_link_libraries(mvcc_dirty_read_test system transaction records)

# add_executable(mvcc_concurrent_executor mvcc_concurrent_executor.cpp)
# target_link_libraries(mvcc_concurrent_executor system transaction records)

# # MVCC标准格式测试
# add_executable(mvcc_standard_format_test mvcc_standard_format_test.cpp)
# target_link_libraries(mvcc_standard_format_test system transaction records)

# # MVCC快照隔离测试
# add_executable(mvcc_snapshot_isolation_test mvcc_snapshot_isolation_test.cpp)
# target_link_libraries(mvcc_snapshot_isolation_test system transaction records)

