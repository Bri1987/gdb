include(GoogleTest)
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/deps/fmt/include)
add_subdirectory(my_concurrency_test)
add_subdirectory(generate_data)
#set(test_dependency
#        gtest_main
#        )
file(GLOB_RECURSE RMDB_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*test.cpp")
message("start build gtest targets")
foreach(RMDB_TEST_SOURCE ${RMDB_TEST_SOURCES})
    get_filename_component(rmdb_test_filename ${RMDB_TEST_SOURCE} NAME)
    string(REPLACE ".cpp" "" rmdb_test_name ${rmdb_test_filename})
    message("detect test: ${rmdb_test_name}")
    # Add the test target separately and as part of "make check-tests".
    add_executable(${rmdb_test_name} EXCLUDE_FROM_ALL ${RMDB_TEST_SOURCE})
    gtest_discover_tests(${rmdb_test_name}
            EXTRA_ARGS
            --gtest_color=auto
            --gtest_catch_exceptions=0
            --verbose
            DISCOVERY_TIMEOUT 120
            PROPERTIES
            TIMEOUT 120
            )

    target_link_libraries(${rmdb_test_name} gtest gmock_main fmt)

    # Set test target properties and dependencies.
    set_target_properties(${rmdb_test_name}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
            COMMAND ${rmdb_test_name}
            )
    message("runtime output dir:-> ${CMAKE_BINARY_DIR}")
endforeach()
#################################
##      在这里添加你自己需要的库    ##
#################################
target_link_libraries(storage2_test
        lru_replacer
        storage)

target_link_libraries(executor_test
        execution)
target_link_libraries(index_test
        index)
target_link_libraries(crab_test
        index)