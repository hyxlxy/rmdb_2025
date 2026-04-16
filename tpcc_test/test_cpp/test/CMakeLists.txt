#basi send sql
add_executable(basic_send basic_send.cpp)

# gnerate data
add_executable(generate_sql generate_sql.cpp)

#tpcc_first_phase.cpp
add_executable(tpcc_first_phase tpcc_first_phase.cpp)
target_link_libraries(tpcc_first_phase pthread)
