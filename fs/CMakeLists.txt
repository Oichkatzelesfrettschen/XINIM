##
#List all source files for the file system server explicitly.Using an
#explicit list avoids surprises when new files are added and makes
#dependencies clear.
    ##set(FS_SRC
#Buffer cache implementation with RAII semantics
              cache.cpp compat.cpp device.cpp filedes.cpp inode.cpp link.cpp main.cpp misc.cpp mount
                  .cpp open.cpp path.cpp pipe.cpp protect.cpp putc.cpp read.cpp stadir.cpp super
                  .cpp table.cpp time.cpp utility.cpp write.cpp)

        add_executable(minix_fs_server ${FS_SRC})

#FS server should link against the C library(minix_libc)
#Assuming minixlib will be renamed to minix_libc from lib / CMakeLists.txt
            target_link_libraries(minix_fs_server PRIVATE minix_libc)

#Add include directories that might be needed by FS source files
                target_include_directories(
                    minix_fs_server PUBLIC "." #For headers in fs /
                    itself(e.g., buf.h, const.h, dev.h,
                           etc.) "${CMAKE_SOURCE_DIR}/include" #For global includes
                                 "${CMAKE_SOURCE_DIR}/h" #For global includes from h /)
