#CMake configuration for XINIM unit tests

add_executable(minix_test_lib test_lib.cpp "${CMAKE_SOURCE_DIR}/lib/strlen.cpp"
                                           "${CMAKE_SOURCE_DIR}/lib/strcmp.cpp"
                                           "${CMAKE_SOURCE_DIR}/lib/rand.cpp")

    target_include_directories(minix_test_lib PUBLIC "${CMAKE_SOURCE_DIR}/include"
                                                     "${CMAKE_SOURCE_DIR}/h")

        target_compile_options(minix_test_lib PRIVATE - std = c++ 23 - fno - builtin)

            add_test(NAME minix_test_lib COMMAND minix_test_lib)

                add_executable(minix_test_syscall test_syscall.cpp)

                    target_include_directories(minix_test_syscall PUBLIC
                                               "${CMAKE_SOURCE_DIR}/include"
                                               "${CMAKE_SOURCE_DIR}/h")

                        add_test(NAME minix_test_syscall COMMAND minix_test_syscall)

                            add_executable(minix_test_streams test_streams.cpp)

                                target_link_libraries(minix_test_streams PRIVATE minix_libc)

                                    target_include_directories(minix_test_streams PUBLIC
                                                               "${CMAKE_SOURCE_DIR}/include"
                                                               "${CMAKE_SOURCE_DIR}/h")

                                        set_target_properties(
                                            minix_test_streams PROPERTIES EXCLUDE_FROM_ALL TRUE)
                                            add_test(
                                                NAME minix_test_streams COMMAND minix_test_streams)
                                                set_tests_properties(
                                                    minix_test_streams PROPERTIES DISABLED TRUE)

                                                    add_executable(
                                                        minix_test_fastpath test_fastpath.cpp
                                                        "${CMAKE_SOURCE_DIR}/kernel/wormhole.cpp"
                                                        "${CMAKE_SOURCE_DIR}/kernel/schedule.cpp"
                                                        "${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp")

                                                        target_include_directories(
                                                            minix_test_fastpath PUBLIC
                                                            "${CMAKE_SOURCE_DIR}/kernel")

                                                            add_test(
                                                                NAME minix_test_fastpath COMMAND
                                                                    minix_test_fastpath)

                                                                add_executable(
                                                                    minix_test_scheduler
                                                                        test_scheduler.cpp
                                                                    "${CMAKE_SOURCE_DIR}/kernel/"
                                                                    "schedule.cpp"
                                                                    "${CMAKE_SOURCE_DIR}/kernel/"
                                                                    "wait_graph.cpp")

                                                                    target_include_directories(
                                                                        minix_test_scheduler PUBLIC
                                                                        "${CMAKE_SOURCE_DIR}/kernel"
                                                                        "${CMAKE_SOURCE_DIR}/"
                                                                        "include")

                                                                        add_test(
                                                                            NAME minix_test_scheduler
                                                                                COMMAND
                                                                                    minix_test_scheduler)

                                                                            add_executable(
                                                                                minix_test_semantic_region
                                                                                    test_semantic_region
                                                                                        .cpp)

                                                                                target_include_directories(
                                                                                    minix_test_semantic_region PUBLIC
                                                                                    "${CMAKE_"
                                                                                    "SOURCE_DIR}/"
                                                                                    "include")

                                                                                    add_test(
                                                                                        NAME minix_test_semantic_region
                                                                                            COMMAND
                                                                                                minix_test_semantic_region)

                                                                                        add_executable(
                                                                                            minix_test_stream_foundation
                                                                                                test_stream_foundation
                                                                                                    .cpp
                                                                                            "${"
                                                                                            "CMAKE_"
                                                                                            "SOURCE"
                                                                                            "_DIR}/"
                                                                                            "lib/"
                                                                                            "io/"
                                                                                            "src/"
                                                                                            "file_"
                                                                                            "stream"
                                                                                            ".cpp"
                                                                                            "${"
                                                                                            "CMAKE_"
                                                                                            "SOURCE"
                                                                                            "_DIR}/"
                                                                                            "lib/"
                                                                                            "io/"
                                                                                            "src/"
                                                                                            "file_"
                                                                                            "operat"
                                                                                            "ions."
                                                                                            "cpp"
                                                                                            "${"
                                                                                            "CMAKE_"
                                                                                            "SOURCE"
                                                                                            "_DIR}/"
                                                                                            "lib/"
                                                                                            "io/"
                                                                                            "src/"
                                                                                            "standa"
                                                                                            "rd_"
                                                                                            "stream"
                                                                                            "s.cpp")

                                                                                            set_target_properties(
                                                                                                minix_test_stream_foundation PROPERTIES
                                                                                                    CXX_STANDARD 23 CXX_STANDARD_REQUIRED
                                                                                                        ON EXCLUDE_FROM_ALL
                                                                                                            TRUE)

                                                                                                target_include_directories(
                                                                                                    minix_test_stream_foundation PUBLIC
                                                                                                    "${CMAKE_SOURCE_DIR}/include")

                                                                                                    add_test(
                                                                                                        NAME minix_test_stream_foundation
                                                                                                            COMMAND
                                                                                                                minix_test_stream_foundation)
                                                                                                        set_tests_properties(
                                                                                                            minix_test_stream_foundation
                                                                                                                PROPERTIES DISABLED
                                                                                                                    TRUE)

                                                                                                            add_executable(
                                                                                                                minix_test_memory_stream
                                                                                                                    test_memory_stream
                                                                                                                        .cpp
                                                                                                                "${CMAKE_SOURCE_DIR}/lib/io/src/memory_stream.cpp")

                                                                                                                set_target_properties(
                                                                                                                    minix_test_memory_stream
                                                                                                                        PROPERTIES
                                                                                                                            CXX_STANDARD 23 CXX_STANDARD_REQUIRED
                                                                                                                                ON)

                                                                                                                    target_include_directories(
                                                                                                                        minix_test_memory_stream PUBLIC
                                                                                                                        "${CMAKE_SOURCE_DIR}/include")

                                                                                                                        add_test(
                                                                                                                            NAME minix_test_memory_stream
                                                                                                                                COMMAND
                                                                                                                                    minix_test_memory_stream)

                                                                                                                            add_executable(
                                                                                                                                minix_test_scheduler_edge
                                                                                                                                    test_scheduler_edge
                                                                                                                                        .cpp
                                                                                                                                "${CMAKE_SOURCE_DIR}/kernel/schedule.cpp"
                                                                                                                                "${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp")

                                                                                                                                set_target_properties(
                                                                                                                                    minix_test_scheduler_edge
                                                                                                                                        PROPERTIES
                                                                                                                                            CXX_STANDARD 23 CXX_STANDARD_REQUIRED
                                                                                                                                                ON)

                                                                                                                                    target_include_directories(
                                                                                                                                        minix_test_scheduler_edge PUBLIC
                                                                                                                                        "${CMAKE_SOURCE_DIR}/kernel"
                                                                                                                                        "${CMAKE_SOURCE_DIR}/include")

                                                                                                                                        add_test(
                                                                                                                                            NAME minix_test_scheduler_edge
                                                                                                                                                COMMAND
                                                                                                                                                    minix_test_scheduler_edge)

                                                                                                                                            add_executable(
                                                                                                                                                minix_test_wait_graph
                                                                                                                                                    test_wait_graph
                                                                                                                                                        .cpp "${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp")

                                                                                                                                                set_target_properties(
                                                                                                                                                    minix_test_wait_graph PROPERTIES
                                                                                                                                                        CXX_STANDARD 23 CXX_STANDARD_REQUIRED
                                                                                                                                                            ON)

                                                                                                                                                    target_include_directories(
                                                                                                                                                        minix_test_wait_graph PUBLIC
                                                                                                                                                        "${CMAKE_SOURCE_DIR}/kernel"
                                                                                                                                                        "${CMAKE_SOURCE_DIR}/include")

                                                                                                                                                        add_test(
                                                                                                                                                            NAME minix_test_wait_graph
                                                                                                                                                                COMMAND
                                                                                                                                                                    minix_test_wait_graph)

                                                                                                                                                            add_executable(
                                                                                                                                                                minix_test_fastpath_preconditions
                                                                                                                                                                    test_fastpath_preconditions
                                                                                                                                                                        .cpp)

                                                                                                                                                                target_include_directories(
                                                                                                                                                                    minix_test_fastpath_preconditions PUBLIC
                                                                                                                                                                    "${CMAKE_SOURCE_DIR}/include")

                                                                                                                                                                    add_test(
                                                                                                                                                                        NAME minix_test_fastpath_preconditions
                                                                                                                                                                            COMMAND
                                                                                                                                                                                minix_test_fastpath_preconditions)

                                                                                                                                                                        add_subdirectory(
                                                                                                                                                                            crypto)
