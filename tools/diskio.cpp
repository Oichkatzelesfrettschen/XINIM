/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/**
 * @file diskio.cpp
 * @brief Cross-platform disk I/O implementation with caching and performance optimization
 */

#include "diskio.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define NOMINMAX // Prevent Windows.h from defining min/max macros
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/fs.h>
#include <sys/ioctl.h>
#endif
#endif

namespace diskio {

DiskInterface::DiskInterface(const std::string &device_path, bool read_only)
    : device_path_(device_path), read_only_(read_only) {
    if (device_path_.empty()) {
        throw std::invalid_argument("Device path cannot be empty");
    }
    cache_.reserve(CACHE_SIZE);
    open_device();
}

DiskInterface::~DiskInterface() {
    try {
        if (!read_only_) {
            writeback_cache();
            sync();
        }
        close_device();
    } catch (...) {
        // Destructors should not throw
    }
}

DiskInterface::DiskInterface(DiskInterface &&other) noexcept
    : device_path_(std::move(other.device_path_)), read_only_(other.read_only_), fd_(other.fd_),
      statistics_(std::move(other.statistics_)), cache_(std::move(other.cache_)),
      cache_next_victim_(other.cache_next_victim_) {
    other.fd_ = -1;
    other.cache_next_victim_ = 0;
}

DiskInterface &DiskInterface::operator=(DiskInterface &&other) noexcept {
    if (this != &other) {
        // Clean up current state
        try {
            if (!read_only_) {
                writeback_cache();
                sync();
            }
            close_device();
        } catch (...) {
            // Ignore errors during cleanup
        }

        // Move from other
        device_path_ = std::move(other.device_path_);
        read_only_ = other.read_only_;
        fd_ = other.fd_;
        statistics_ = std::move(other.statistics_);
        cache_ = std::move(other.cache_);
        cache_next_victim_ = other.cache_next_victim_;

        // Clear other
        other.fd_ = -1;
        other.cache_next_victim_ = 0;
    }
    return *this;
}

void DiskInterface::open_device() {
#ifdef _WIN32
    DWORD access = GENERIC_READ;
    if (!read_only_) {
        access |= GENERIC_WRITE;
    }

    HANDLE handle = CreateFileA(
        device_path_.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, // Direct I/O for better performance
        nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        throw DiskIoError(device_path_, SectorAddress(0), "open",
                          "CreateFile failed with error " + std::to_string(error));
    }

    fd_ = _open_osfhandle(reinterpret_cast<intptr_t>(handle), read_only_ ? _O_RDONLY : _O_RDWR);
    if (fd_ == -1) {
        CloseHandle(handle);
        throw DiskIoError(device_path_, SectorAddress(0), "open",
                          "Failed to convert handle to file descriptor");
    }
#else
    int flags = read_only_ ? O_RDONLY : O_RDWR;

// Add direct I/O flag if available for better performance
#ifdef O_DIRECT
    flags |= O_DIRECT;
#endif

    fd_ = open(device_path_.c_str(), flags);

    if (fd_ == -1) {
        const int error = errno;
        throw DiskIoError(device_path_, SectorAddress(0), "open",
                          std::string(std::strerror(error)));
    }

    // Try to get exclusive access for write mode
    if (!read_only_) {
#ifdef LOCK_EX
        if (flock(fd_, LOCK_EX | LOCK_NB) == -1) {
            // Non-fatal - device might be in use
        }
#endif
    }
#endif
}

void DiskInterface::close_device() noexcept {
    if (fd_ != -1) {
#ifdef _WIN32
        _close(fd_);
#else
        close(fd_);
#endif
        fd_ = -1;
    }
}

SectorBuffer DiskInterface::read_sector(SectorAddress sector) {
    validate_sector_address(sector);

    // Check cache first
    auto cache_it = find_in_cache(sector);
    if (cache_it != cache_.end()) {
        cache_it->last_access = std::chrono::steady_clock::now();
        return cache_it->data;
    }

    // Read from disk
    auto buffer = read_sector_raw(sector);

    // Add to cache
    add_to_cache(sector, buffer);

    return buffer;
}

SectorBuffer DiskInterface::read_sector_raw(SectorAddress sector) {
    const auto start_time = std::chrono::steady_clock::now();

    SectorBuffer buffer(DiskConstants::SECTOR_SIZE);
    const auto offset = static_cast<std::int64_t>(sector.value) * DiskConstants::SECTOR_SIZE;

#ifdef _WIN32
    LARGE_INTEGER li;
    li.QuadPart = offset;
    HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd_));

    if (SetFilePointerEx(handle, li, nullptr, FILE_BEGIN) == 0) {
        const DWORD error = GetLastError();
        throw DiskIoError(device_path_, sector, "seek",
                          "SetFilePointerEx failed with error " + std::to_string(error));
    }

    DWORD bytes_read;
    if (ReadFile(handle, buffer.data(), DiskConstants::SECTOR_SIZE, &bytes_read, nullptr) == 0) {
        const DWORD error = GetLastError();
        throw DiskIoError(device_path_, sector, "read",
                          "ReadFile failed with error " + std::to_string(error));
    }

    if (bytes_read != DiskConstants::SECTOR_SIZE) {
        throw DiskIoError(device_path_, sector, "read",
                          "Short read: expected " + std::to_string(DiskConstants::SECTOR_SIZE) +
                              " bytes, got " + std::to_string(bytes_read));
    }
#else
    if (lseek(fd_, offset, SEEK_SET) == -1) {
        const int error = errno;
        throw DiskIoError(device_path_, sector, "seek", std::strerror(error));
    }

    ssize_t bytes_read = read(fd_, buffer.data(), DiskConstants::SECTOR_SIZE);
    if (bytes_read == -1) {
        const int error = errno;
        throw DiskIoError(device_path_, sector, "read", std::strerror(error));
    }

    if (static_cast<std::size_t>(bytes_read) != DiskConstants::SECTOR_SIZE) {
        throw DiskIoError(device_path_, sector, "read",
                          "Short read: expected " + std::to_string(DiskConstants::SECTOR_SIZE) +
                              " bytes, got " + std::to_string(bytes_read));
    }
#endif

    // Update statistics
    const auto end_time = std::chrono::steady_clock::now();
    const auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    statistics_.bytes_read += DiskConstants::SECTOR_SIZE;
    statistics_.read_operations++;
    statistics_.total_time_ms += duration.count();

    return buffer;
}

void DiskInterface::write_sector(SectorAddress sector, const SectorBuffer &buffer) {
    if (read_only_) {
        throw DiskIoError(device_path_, sector, "write", "Device opened in read-only mode");
    }

    validate_sector_address(sector);

    if (buffer.size_bytes() != DiskConstants::SECTOR_SIZE) {
        throw std::invalid_argument("Buffer size must be exactly one sector (" +
                                    std::to_string(DiskConstants::SECTOR_SIZE) + " bytes)");
    }

    // Update cache if sector is cached
    auto cache_it = find_in_cache(sector);
    if (cache_it != cache_.end()) {
        cache_it->data = buffer;
        cache_it->dirty = true;
        cache_it->last_access = std::chrono::steady_clock::now();
    } else {
        // Add to cache as dirty
        add_to_cache(sector, buffer);
        auto new_cache_it = find_in_cache(sector);
        if (new_cache_it != cache_.end()) {
            new_cache_it->dirty = true;
        }
    }

    // Write through to disk immediately for data integrity
    write_sector_raw(sector, buffer);
}

void DiskInterface::write_sector_raw(SectorAddress sector, const SectorBuffer &buffer) {
    const auto start_time = std::chrono::steady_clock::now();

    const auto offset = static_cast<std::int64_t>(sector.value) * DiskConstants::SECTOR_SIZE;

#ifdef _WIN32
    LARGE_INTEGER li;
    li.QuadPart = offset;
    HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd_));

    if (SetFilePointerEx(handle, li, nullptr, FILE_BEGIN) == 0) {
        const DWORD error = GetLastError();
        throw DiskIoError(device_path_, sector, "seek",
                          "SetFilePointerEx failed with error " + std::to_string(error));
    }

    DWORD bytes_written;
    if (WriteFile(handle, buffer.data(), DiskConstants::SECTOR_SIZE, &bytes_written, nullptr) ==
        0) {
        const DWORD error = GetLastError();
        throw DiskIoError(device_path_, sector, "write",
                          "WriteFile failed with error " + std::to_string(error));
    }

    if (bytes_written != DiskConstants::SECTOR_SIZE) {
        throw DiskIoError(device_path_, sector, "write",
                          "Short write: expected " + std::to_string(DiskConstants::SECTOR_SIZE) +
                              " bytes, wrote " + std::to_string(bytes_written));
    }
#else
    if (lseek(fd_, offset, SEEK_SET) == -1) {
        const int error = errno;
        throw DiskIoError(device_path_, sector, "seek", std::strerror(error));
    }

    ssize_t bytes_written = write(fd_, buffer.data(), DiskConstants::SECTOR_SIZE);
    if (bytes_written == -1) {
        const int error = errno;
        throw DiskIoError(device_path_, sector, "write", std::strerror(error));
    }

    if (static_cast<std::size_t>(bytes_written) != DiskConstants::SECTOR_SIZE) {
        throw DiskIoError(device_path_, sector, "write",
                          "Short write: expected " + std::to_string(DiskConstants::SECTOR_SIZE) +
                              " bytes, wrote " + std::to_string(bytes_written));
    }
#endif

    // Update statistics
    const auto end_time = std::chrono::steady_clock::now();
    const auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    statistics_.bytes_written += DiskConstants::SECTOR_SIZE;
    statistics_.write_operations++;
    statistics_.total_time_ms += duration.count();
}

std::vector<SectorBuffer> DiskInterface::read_sectors(SectorAddress start_sector,
                                                      std::size_t count) {
    if (count == 0) {
        return {};
    }

    std::vector<SectorBuffer> buffers;
    buffers.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        buffers.push_back(read_sector(SectorAddress(start_sector.value + i)));
    }

    return buffers;
}

void DiskInterface::write_sectors(SectorAddress start_sector,
                                  const std::vector<SectorBuffer> &buffers) {
    if (read_only_) {
        throw DiskIoError(device_path_, start_sector, "write_sectors",
                          "Device opened in read-only mode");
    }

    for (std::size_t i = 0; i < buffers.size(); ++i) {
        write_sector(SectorAddress(start_sector.value + i), buffers[i]);
    }
}

void DiskInterface::sync() {
    if (read_only_) {
        return; // Nothing to sync in read-only mode
    }

    // Write back any dirty cache entries
    writeback_cache();

#ifdef _WIN32
    HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd_));
    if (FlushFileBuffers(handle) == 0) {
        const DWORD error = GetLastError();
        throw DiskIoError(device_path_, SectorAddress(0), "sync",
                          "FlushFileBuffers failed with error " + std::to_string(error));
    }
#else
    if (fsync(fd_) == -1) {
        const int error = errno;
        throw DiskIoError(device_path_, SectorAddress(0), "sync", std::strerror(error));
    }
#endif
}

void DiskInterface::flush_cache() {
    if (!read_only_) {
        writeback_cache();
    }
    cache_.clear();
    cache_next_victim_ = 0;
}

std::uint64_t DiskInterface::get_size() const {
#ifdef _WIN32
    HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd_));
    LARGE_INTEGER file_size;

    if (GetFileSizeEx(handle, &file_size) == 0) {
        const DWORD error = GetLastError();
        throw DiskIoError(device_path_, SectorAddress(0), "get_size",
                          "GetFileSizeEx failed with error " + std::to_string(error));
    }

    return static_cast<std::uint64_t>(file_size.QuadPart);
#else
    struct stat st;
    if (fstat(fd_, &st) == -1) {
        const int error = errno;
        throw DiskIoError(device_path_, SectorAddress(0), "get_size", std::strerror(error));
    }

    if (S_ISREG(st.st_mode)) {
        return static_cast<std::uint64_t>(st.st_size);
    } else if (S_ISBLK(st.st_mode)) {
// For block devices, try to get the size via ioctl
#ifdef __linux__
        std::uint64_t size;
        if (ioctl(fd_, BLKGETSIZE64, &size) == 0) {
            return size;
        }
#endif

        // Fallback: seek to end and get position
        const auto current_pos = lseek(fd_, 0, SEEK_CUR);
        const auto end_pos = lseek(fd_, 0, SEEK_END);
        lseek(fd_, current_pos, SEEK_SET); // Restore position

        if (end_pos == -1) {
            const int error = errno;
            throw DiskIoError(device_path_, SectorAddress(0), "get_size", std::strerror(error));
        }

        return static_cast<std::uint64_t>(end_pos);
    } else {
        throw DiskIoError(device_path_, SectorAddress(0), "get_size", "Unsupported file type");
    }
#endif
}

bool DiskInterface::is_accessible() const noexcept {
    if (fd_ == -1) {
        return false;
    }

    try {
        // Try reading the first sector to check accessibility
        const_cast<DiskInterface *>(this)->read_sector_raw(SectorAddress(0));
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<DiskInterface::CacheEntry>::iterator
DiskInterface::find_in_cache(SectorAddress sector) const {
    return std::find_if(cache_.begin(), cache_.end(),
                        [sector](const CacheEntry &entry) { return entry.address == sector; });
}

void DiskInterface::add_to_cache(SectorAddress sector, SectorBuffer data) const {
    if (cache_.size() < CACHE_SIZE) {
        // Cache not full, just add
        cache_.emplace_back(sector, std::move(data));
    } else {
        // Cache full, replace victim using round-robin
        auto &victim = cache_[cache_next_victim_];

        // Write back if dirty
        if (victim.dirty && !read_only_) {
            try {
                const_cast<DiskInterface *>(this)->write_sector_raw(victim.address, victim.data);
                victim.dirty = false;
            } catch (...) {
                // Ignore write-back errors during cache eviction
            }
        }

        // Replace victim
        victim.address = sector;
        victim.data = std::move(data);
        victim.dirty = false;
        victim.last_access = std::chrono::steady_clock::now();

        // Advance to next victim
        cache_next_victim_ = (cache_next_victim_ + 1) % CACHE_SIZE;
    }
}

void DiskInterface::writeback_cache() {
    for (auto &entry : cache_) {
        if (entry.dirty) {
            try {
                write_sector_raw(entry.address, entry.data);
                entry.dirty = false;
            } catch (...) {
                // Re-throw write-back errors
                throw;
            }
        }
    }
}

void DiskInterface::validate_sector_address(SectorAddress sector) const {
    try {
        const auto device_size = get_size();
        const auto max_sector = device_size / DiskConstants::SECTOR_SIZE;

        if (sector.value >= max_sector) {
            throw std::out_of_range("Sector " + std::to_string(sector.value) +
                                    " out of range (max: " + std::to_string(max_sector - 1) + ")");
        }
    } catch (const std::out_of_range &) {
        throw; // Re-throw range errors
    } catch (const std::exception &) {
        // If we can't get device size, assume sector is valid
        // This allows operation on special devices where size query might fail
    }
}

} // namespace diskio
