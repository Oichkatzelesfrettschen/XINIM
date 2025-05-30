#pragma once
// Modernized for C++17

// Scoped bit flags describing the file type and permissions.
enum class FileMode : unsigned short {
    S_IFMT = 0170000,  // mask for file type bits
    S_IFDIR = 0040000, // directory
    S_IFCHR = 0020000, // character special
    S_IFBLK = 0060000, // block special
    S_IFREG = 0100000, // regular file
    S_ISUID = 04000,   // set user id on execution
    S_ISGID = 02000,   // set group id on execution
    S_ISVTX = 01000,   // save swapped text even after use
    S_IREAD = 00400,   // read permission, owner
    S_IWRITE = 00200,  // write permission, owner
    S_IEXEC = 00100    // execute/search permission, owner
};

// Standard file status information returned by stat().
struct stat {
    short int st_dev;  // device number
    unsigned short st_ino; // inode number
    FileMode st_mode;  // file type and permissions
    short int st_nlink; // link count
    short int st_uid;   // owner user id
    short int st_gid;   // owner group id
    short int st_rdev;  // special device number
    long st_size;       // file size in bytes
    long st_atime;      // last access time
    long st_mtime;      // last modification time
    long st_ctime;      // creation time
};

// Enable bitwise and masking semantics for FileMode values.
constexpr unsigned short operator&(FileMode lhs, FileMode rhs) {
    return static_cast<unsigned short>(lhs) & static_cast<unsigned short>(rhs);
}

constexpr unsigned short operator&(FileMode lhs, unsigned short rhs) {
    return static_cast<unsigned short>(lhs) & rhs;
}

constexpr unsigned short operator&(unsigned short lhs, FileMode rhs) {
    return lhs & static_cast<unsigned short>(rhs);
}
