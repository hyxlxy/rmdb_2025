set(SOURCES execution_manager.cpp mvcc_executor_base.cpp tpcc_runtime_coordinator.cpp)
add_library(execution STATIC ${SOURCES})

target_link_libraries(execution system record transaction planner common_extra)