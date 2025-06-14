#Unit tests for Minix library routines and basic system calls

#Library routine tests compile selected sources directly
add_executable(minix_test_lib test_lib.cpp "${CMAKE_SOURCE_DIR}/lib/strlen.cpp"
                                           "${CMAKE_SOURCE_DIR}/lib/strcmp.cpp"
                                           "${CMAKE_SOURCE_DIR}/lib/rand.cpp")

#Ensure lib_test can find necessary headers from the C library's public interface
    target_include_directories(
        minix_test_lib PUBLIC
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/h") target_compile_options(minix_test_lib PRIVATE - std =
                                                            c++ 23 - fno -
                                                            builtin) add_test(NAME minix_test_lib
                                                                                  COMMAND
                                                                                      minix_test_lib)

#System call tests rely on the host C library only(or direct syscalls)
        add_executable(minix_test_syscall test_syscall.cpp) target_include_directories(
            minix_test_syscall PUBLIC
            "${CMAKE_SOURCE_DIR}/include"
            "${CMAKE_SOURCE_DIR}/h") add_test(NAME minix_test_syscall COMMAND minix_test_syscall)

#Fastpath unit test
#Add Stream architecture tests
            add_executable(minix_test_streams test_streams.cpp) target_link_libraries(
                minix_test_streams PRIVATE
                    minix_libc) target_include_directories(minix_test_streams PUBLIC
                                                           "${CMAKE_SOURCE_DIR}/include"
                                                           "${CMAKE_SOURCE_DIR}/h")
                set_target_properties(minix_test_streams PROPERTIES EXCLUDE_FROM_ALL TRUE) add_test(
                    NAME minix_test_streams COMMAND
                        minix_test_streams) set_tests_properties(minix_test_streams PROPERTIES
                                                                     DISABLED TRUE)

#Wormhole unit test
                    add_executable(minix_test_fastpath test_fastpath.cpp
                                   "${CMAKE_SOURCE_DIR}/kernel/wormhole.cpp"
                                   "${CMAKE_SOURCE_DIR}/kernel/schedule.cpp")
                        target_include_directories(
                            minix_test_fastpath PUBLIC
                            "${CMAKE_SOURCE_DIR}/kernel") add_test(NAME minix_test_fastpath COMMAND
                                                                       minix_test_fastpath)

#Scheduler unit test
                            add_executable(minix_test_scheduler test_scheduler.cpp "${CMAKE_SOURCE_"
                                                                                   "DIR}/kernel/"
                                                                                   "schedule.cpp") target_include_directories(
                                minix_test_scheduler PUBLIC
                                "${CMAKE_SOURCE_DIR}/kernel"
                                "${CMAKE_SOURCE_DIR}/include") add_test(NAME minix_test_scheduler
                                                                            COMMAND
                                                                                minix_test_scheduler)

#Semantic region unit test
                                add_executable(minix_test_semantic_region test_semantic_region.cpp) target_include_directories(
                                    minix_test_semantic_region PUBLIC
                                    "${CMAKE_SOURCE_DIR}/include") add_test(NAME minix_test_semantic_region
                                                                                COMMAND
                                                                                    minix_test_semantic_region)

#Stream foundation verification test
                                    add_executable(
                                        minix_test_stream_foundation test_stream_foundation.cpp
                                        "${CMAKE_SOURCE_DIR}/lib/io/src/file_stream.cpp"
                                        "${CMAKE_SOURCE_DIR}/lib/io/src/file_operations.cpp"
                                        "${CMAKE_SOURCE_DIR}/lib/io/src/standard_streams.cpp")
                                        set_target_properties(
                                            minix_test_stream_foundation PROPERTIES
                                                CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON EXCLUDE_FROM_ALL
                                                    TRUE) target_include_directories(minix_test_stream_foundation PUBLIC
                                                                                     "${CMAKE_"
                                                                                     "SOURCE_DIR}/"
                                                                                     "include")
                                            add_test(
                                                NAME minix_test_stream_foundation COMMAND
                                                    minix_test_stream_foundation) set_tests_properties(minix_test_stream_foundation
                                                                                                           PROPERTIES DISABLED
                                                                                                               TRUE)

#MemoryStream unit test
                                                add_executable(
                                                    minix_test_memory_stream test_memory_stream.cpp
                                                    "${CMAKE_SOURCE_DIR}/lib/io/src/"
                                                    "memory_stream.cpp")
                                                    set_target_properties(
                                                        minix_test_memory_stream PROPERTIES
                                                            CXX_STANDARD 23 CXX_STANDARD_REQUIRED
                                                                ON)
                                                        target_include_directories(
                                                            minix_test_memory_stream PUBLIC
                                                            "${CMAKE_SOURCE_DIR}/include") add_test(NAME
                                                                                                        minix_test_memory_stream COMMAND minix_test_memory_stream)

#Scheduler edge case unit test
                                                            add_executable(minix_test_scheduler_edge
                                                                               test_scheduler_edge.cpp
                                                                           "${CMAKE_SOURCE_DIR}/"
                                                                           "kernel/schedule.cpp")
                                                                set_target_properties(
                                                                    minix_test_scheduler_edge
                                                                        PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED
                                                                            ON)
                                                                    target_include_directories(
                                                                        minix_test_scheduler_edge PUBLIC
                                                                        "${CMAKE_SOURCE_DIR}/kernel"
                                                                        "${CMAKE_SOURCE_DIR}/"
                                                                        "include") add_test(NAME minix_test_scheduler_edge COMMAND minix_test_scheduler_edge)

#Wait graph unit test
                                                                        add_executable(
                                                                            minix_test_wait_graph test_wait_graph
                                                                                .cpp "${CMAKE_"
                                                                                     "SOURCE_DIR}/"
                                                                                     "kernel/"
                                                                                     "wait_graph."
                                                                                     "cpp")
                                                                            set_target_properties(
                                                                                minix_test_wait_graph PROPERTIES
                                                                                    CXX_STANDARD 23 CXX_STANDARD_REQUIRED
                                                                                        ON)
                                                                                target_include_directories(
                                                                                    minix_test_wait_graph PUBLIC
                                                                                    "${CMAKE_"
                                                                                    "SOURCE_DIR}/"
                                                                                    "kernel"
                                                                                    "${CMAKE_"
                                                                                    "SOURCE_DIR}/"
                                                                                    "include") add_test(NAME minix_test_wait_graph COMMAND minix_test_wait_graph)

#Fastpath precondition unit test
                                                                                    add_executable(
                                                                                        minix_test_fastpath_preconditions test_fastpath_preconditions
                                                                                            .cpp
                                                                                        "${CMAKE_"
                                                                                        "SOURCE_"
                                                                                        "DIR}/"
                                                                                        "kernel/"
                                                                                        "wormhole."
                                                                                        "cpp"
                                                                                        "${CMAKE_"
                                                                                        "SOURCE_"
                                                                                        "DIR}/"
                                                                                        "kernel/"
                                                                                        "schedule."
                                                                                        "cpp")
                                                                                        set_target_properties(
                                                                                            minix_test_fastpath_preconditions
                                                                                                PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED
                                                                                                    ON)
                                                                                            target_include_directories(
                                                                                                minix_test_fastpath_preconditions PUBLIC
                                                                                                "${"
                                                                                                "CM"
                                                                                                "AK"
                                                                                                "E_"
                                                                                                "SO"
                                                                                                "UR"
                                                                                                "CE"
                                                                                                "_D"
                                                                                                "IR"
                                                                                                "}/"
                                                                                                "ke"
                                                                                                "rn"
                                                                                                "el"
                                                                                                "${"
                                                                                                "CM"
                                                                                                "AK"
                                                                                                "E_"
                                                                                                "SO"
                                                                                                "UR"
                                                                                                "CE"
                                                                                                "_D"
                                                                                                "IR"
                                                                                                "}/"
                                                                                                "in"
                                                                                                "cl"
                                                                                                "ud"
                                                                                                "e")
                                                                                                add_test(
                                                                                                    NAME minix_test_fastpath_preconditions
                                                                                                        COMMAND
                                                                                                            minix_test_fastpath_preconditions)

#Cryptography unit tests
                                                                                                    add_subdirectory(
                                                                                                        crypto)
