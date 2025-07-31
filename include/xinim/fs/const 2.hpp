#pragma once

/**
 * @file const.hpp
 * @brief C++23 modernized file system constants with enhanced type safety
 */

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace minix::fs {

/** @enum block_nr
 *  @brief Strongly typed block number
 */
enum class block_nr : std::uint32_t {};

/** @enum inode_nr
 *  @brief Strongly typed inode number
 */
enum class inode_nr : std::uint32_t {};

/** @enum zone_nr
 *  @brief Strongly typed zone number
 */
enum class zone_nr : std::uint32_t {};

/** @enum dev_nr
 *  @brief Strongly typed device number
 */
enum class dev_nr : std::uint16_t {};

/** @enum bit_nr
 *  @brief Bit number type
 */
enum class bit_nr : std::uint32_t {};

/** @enum uid
 *  @brief User identifier type
 */
enum class uid : std::uint16_t {};

/** @enum gid
 *  @brief Group identifier type
 */
enum class gid : std::uint8_t {};

/** @enum file_pos
 *  @brief 32-bit file position type
 */
enum class file_pos : std::int32_t {};

/** @enum file_pos64
 *  @brief 64-bit file position type
 */
enum class file_pos64 : std::int64_t {};

/** @enum mask_bits
 *  @brief Permission mask bits
 */
enum class mask_bits : std::uint16_t {};

/** @enum links
 *  @brief Link count type
 */
enum class links : std::uint8_t {};

/** @enum real_time
 *  @brief Time type representation
 */
enum class real_time : std::int64_t {};

inline constexpr block_nr kNoBlock{0};
inline constexpr zone_nr kNoZone{0};
inline constexpr dev_nr kNoDev{0};
inline constexpr bit_nr kNoBit{0};
inline constexpr inode_nr kNoInode{0};

/**
 * @concept PowerOfTwo
 * @brief Concept for validating numeric constraints at compile time
 */
template <typename T>
concept PowerOfTwo = requires(T value) {
    std::is_integral_v<T>;
    ((value & (value - 1)) == 0) && (value > 0);
};

/**
 * @tparam BlockSize compile-time block size
 * @struct FsConstants
 * @brief File system constant set validated at compile time
 */
template <std::size_t BlockSize>
    requires PowerOfTwo<BlockSize>
struct FsConstants {
    static constexpr std::size_t kBlockSize = BlockSize;
    static constexpr std::size_t kZoneNumSize = sizeof(zone_nr);
    static constexpr std::size_t kNrZoneNums = 9;
    static constexpr std::size_t kNrBufs = 20;
    static constexpr std::size_t kNrBufHash = 32;
    static constexpr std::size_t kNrFds = 20;
    static constexpr std::size_t kNrFilps = 64;
    static constexpr std::size_t kNrInodes = 32;
    static constexpr std::size_t kNrSupers = 5;
    static constexpr std::size_t kNameSize = 14;
    static constexpr std::size_t kFsStackBytes = 512;
    static constexpr std::uint16_t kSuperMagic = 0x137F;
    static constexpr uid kSuUid{0};
    static constexpr uid kSysUid{0};
    static constexpr gid kSysGid{0};

    /** @enum IoMode
     *  @brief I/O mode flags
     */
    enum class IoMode : std::uint8_t { Normal = 0, NoRead = 1 };

    /** @enum DirOp
     *  @brief Directory operations semantics
     */
    enum class DirOp : std::uint8_t { LookUp = 0, Enter = 1, Delete = 2 };

    /** @enum BufferState
     *  @brief Dirty state for buffers
     */
    enum class BufferState : std::uint8_t { Clean = 0, Dirty = 1 };

    static constexpr std::size_t kNrDzoneNum = kNrZoneNums - 2;
    static constexpr std::size_t kDirEntrySize = sizeof(struct dir_struct);
    static constexpr std::size_t kInodeSize = sizeof(struct d_inode);
    static constexpr std::size_t kInodesPerBlock = kBlockSize / kInodeSize;
    static constexpr std::size_t kNrDirEntries = kBlockSize / kDirEntrySize;
    static constexpr std::size_t kNrIndirects = kBlockSize / kZoneNumSize;
    static constexpr std::size_t kIntsPerBlock = kBlockSize / sizeof(std::int32_t);
    static constexpr std::size_t kPipeSize = kNrDzoneNum * kBlockSize;

    static_assert(kBlockSize % kInodeSize == 0, "Block size must be divisible by inode size");
    static_assert(kNrBufHash > 0 && PowerOfTwo<kNrBufHash>,
                  "Buffer hash size must be power of two");
    static_assert(kNrFds <= 127, "File descriptor limit constraint");
    static_assert(kNrBufs >= 6, "Minimum buffer requirement");
};

using DefaultFsConstants = FsConstants<1024>;

/** @namespace FileTypes
 *  @brief File type constants with enhanced type safety
 */
namespace FileTypes {
inline constexpr mask_bits kRegular{0o100000};
inline constexpr mask_bits kDirectory{0o040000};
inline constexpr mask_bits kBlockSpecial{0o060000};
inline constexpr mask_bits kCharSpecial{0o020000};
inline constexpr mask_bits kPipe{0o010000};
inline constexpr mask_bits kTypeMask{0o170000};
} // namespace FileTypes

/** @namespace Permissions
 *  @brief Permission bits with semantic clarity
 */
namespace Permissions {
inline constexpr mask_bits kReadBit{0o4};
inline constexpr mask_bits kWriteBit{0o2};
inline constexpr mask_bits kExecBit{0o1};
inline constexpr mask_bits kAllModes{0o777};
inline constexpr mask_bits kRwxModes{0o700};
} // namespace Permissions

/** @enum BlockType
 *  @brief Classification of buffer block types
 */
enum class BlockType : std::uint8_t {
    InodeBlock = 0,
    DirectoryBlock = 1,
    IndirectBlock = 2,
    ImapBlock = 3,
    ZmapBlock = 4,
    SuperBlock = 5,
    FullDataBlock = 6,
    PartialDataBlock = 7
};

/** @namespace BlockFlags
 *  @brief Enhanced block type flags
 */
namespace BlockFlags {
inline constexpr std::uint8_t kWriteImmed = 0100;
inline constexpr std::uint8_t kOneShot = 0200;
} // namespace BlockFlags

/**
 * @brief Compute block number from zone number.
 * @tparam ZoneSize Size of a zone in blocks.
 */
template <std::size_t ZoneSize> constexpr auto zones_to_blocks(zone_nr zone) -> block_nr {
    return block_nr{std::to_underlying(zone) * ZoneSize};
}

/**
 * @brief Return maximum representable value for enum type.
 * @tparam T Enum type.
 */
template <typename T> constexpr T max_value() {
    return T{std::numeric_limits<std::underlying_type_t<T>>::max()};
}

} // namespace minix::fs

namespace legacy {
using ::minix::fs::DefaultFsConstants;
} // namespace legacy
