#include "xinim/filesystem.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

class temp_entity {
public:
    enum class kind {
        file,
        directory,
        symlink,
    };

    explicit temp_entity(const std::string& prefix,
                         kind entity_kind,
                         const fs::path& symlink_target = {})
        : kind_(entity_kind) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = fs::temp_directory_path() / (prefix + "_" + std::to_string(now) + "_" +
                                             std::to_string(counter_++));

        std::error_code ec;
        switch (kind_) {
        case kind::file: {
            std::ofstream out(path_);
            out << "hello test";
            out.close();
            break;
        }
        case kind::directory:
            fs::create_directory(path_, ec);
            break;
        case kind::symlink:
            fs::create_symlink(symlink_target, path_, ec);
            break;
        }

        if (ec) {
            std::println(std::cerr, "FATAL: setup failed for {}: {}", path_.string(), ec.message());
            std::exit(EXIT_FAILURE);
        }
    }

    ~temp_entity() {
        std::error_code ec;
        if (fs::is_symlink(fs::symlink_status(path_, ec))) {
            ec.clear();
            fs::remove(path_, ec);
        } else {
            ec.clear();
            fs::remove_all(path_, ec);
        }
        if (ec) {
            std::println(std::cerr, "Warning: cleanup failed for {}: {}", path_.string(), ec.message());
        }
    }

    temp_entity(const temp_entity&) = delete;
    temp_entity& operator=(const temp_entity&) = delete;

    [[nodiscard]] const fs::path& path() const noexcept { return path_; }

private:
    inline static int counter_{0};
    fs::path path_{};
    kind kind_{kind::file};
};

fs::perms perms_from_mode(const mode_t mode) {
    fs::perms perms = fs::perms::none;
    if ((mode & S_IRUSR) != 0) perms |= fs::perms::owner_read;
    if ((mode & S_IWUSR) != 0) perms |= fs::perms::owner_write;
    if ((mode & S_IXUSR) != 0) perms |= fs::perms::owner_exec;
    if ((mode & S_IRGRP) != 0) perms |= fs::perms::group_read;
    if ((mode & S_IWGRP) != 0) perms |= fs::perms::group_write;
    if ((mode & S_IXGRP) != 0) perms |= fs::perms::group_exec;
    if ((mode & S_IROTH) != 0) perms |= fs::perms::others_read;
    if ((mode & S_IWOTH) != 0) perms |= fs::perms::others_write;
    if ((mode & S_IXOTH) != 0) perms |= fs::perms::others_exec;
    if ((mode & S_ISUID) != 0) perms |= fs::perms::set_uid;
    if ((mode & S_ISGID) != 0) perms |= fs::perms::set_gid;
    if ((mode & S_ISVTX) != 0) perms |= fs::perms::sticky_bit;
    return perms;
}

fs::file_type file_type_from_mode(const mode_t mode) {
    if (S_ISREG(mode)) return fs::file_type::regular;
    if (S_ISDIR(mode)) return fs::file_type::directory;
    if (S_ISLNK(mode)) return fs::file_type::symlink;
    if (S_ISBLK(mode)) return fs::file_type::block;
    if (S_ISCHR(mode)) return fs::file_type::character;
    if (S_ISFIFO(mode)) return fs::file_type::fifo;
    if (S_ISSOCK(mode)) return fs::file_type::socket;
    return fs::file_type::unknown;
}

bool compare_status(const xinim::fs::file_status_ex& status,
                    const struct stat& expected,
                    const fs::path& original_path,
                    int& failures) {
    bool ok = true;
    const auto report = [&](const std::string& field, const auto expected_value, const auto actual_value) {
        std::println(std::cerr,
                     "  {} mismatch for {}: expected {}, got {}",
                     field,
                     original_path.string(),
                     expected_value,
                     actual_value);
        ok = false;
    };

    if (!status.is_populated) {
        std::println(std::cerr, "  is_populated mismatch for {}: expected true, got false", original_path.string());
        ok = false;
    }
    if (status.uid != expected.st_uid) report("uid", expected.st_uid, status.uid);
    if (status.gid != expected.st_gid) report("gid", expected.st_gid, status.gid);
    if (status.link_count != static_cast<nlink_t>(expected.st_nlink)) {
        report("link_count", expected.st_nlink, status.link_count);
    }
    if (status.inode != expected.st_ino) report("inode", expected.st_ino, status.inode);
    if (status.type != file_type_from_mode(expected.st_mode)) {
        report("type", static_cast<int>(file_type_from_mode(expected.st_mode)), static_cast<int>(status.type));
    }
    if (status.permissions != perms_from_mode(expected.st_mode)) {
        report("permissions",
               static_cast<unsigned int>(perms_from_mode(expected.st_mode)),
               static_cast<unsigned int>(status.permissions));
    }

    if (S_ISREG(expected.st_mode) || S_ISLNK(expected.st_mode)) {
        if (status.file_size != static_cast<std::uintmax_t>(expected.st_size)) {
            report("size", expected.st_size, status.file_size);
        }
    }

    const auto mtime_delta =
        std::llabs(std::chrono::system_clock::to_time_t(status.mtime) - expected.st_mtime);
    const auto atime_delta =
        std::llabs(std::chrono::system_clock::to_time_t(status.atime) - expected.st_atime);
    const auto ctime_delta =
        std::llabs(std::chrono::system_clock::to_time_t(status.ctime) - expected.st_ctime);

    if (mtime_delta > 2) report("mtime", expected.st_mtime, std::chrono::system_clock::to_time_t(status.mtime));
    if (atime_delta > 2) report("atime", expected.st_atime, std::chrono::system_clock::to_time_t(status.atime));
    if (ctime_delta > 2) report("ctime", expected.st_ctime, std::chrono::system_clock::to_time_t(status.ctime));

    if (!ok) {
        ++failures;
    }
    return ok;
}

bool expect_error(const std::expected<xinim::fs::file_status_ex, std::error_code>& result,
                  const std::errc expected,
                  const std::string& name,
                  int& failures) {
    if (result.has_value()) {
        std::println(std::cout, "FAIL ({}: expected error, got success)", name);
        ++failures;
        return false;
    }
    if (result.error() != expected) {
        std::println(std::cout,
                     "FAIL ({}: expected error {}, got {})",
                     name,
                     std::make_error_code(expected).message(),
                     result.error().message());
        ++failures;
        return false;
    }
    std::println(std::cout, "PASS ({})", name);
    return true;
}

} // namespace

int main() {
    int failures = 0;

    temp_entity target_file("xinim_status_target_file", temp_entity::kind::file);
    temp_entity target_dir("xinim_status_target_dir", temp_entity::kind::directory);
    const fs::path dangling_target = fs::temp_directory_path() / "xinim_status_dangling_target";
    std::error_code dangling_ec;
    fs::remove(dangling_target, dangling_ec);

    struct test_case {
        std::string name;
        fs::path path;
        xinim::fs::operation_context ctx;
        bool expect_success;
        std::errc expected_error;
    };

    temp_entity regular_file("xinim_status_regular", temp_entity::kind::file);
    temp_entity regular_dir("xinim_status_dir", temp_entity::kind::directory);
    temp_entity symlink_file("xinim_status_symlink_file", temp_entity::kind::symlink, target_file.path());
    temp_entity symlink_dir("xinim_status_symlink_dir", temp_entity::kind::symlink, target_dir.path());
    temp_entity dangling_symlink("xinim_status_dangling", temp_entity::kind::symlink, dangling_target);

    const std::vector<test_case> tests = {
        {"regular_file_follow", regular_file.path(), {}, true, {}},
        {"regular_dir_follow", regular_dir.path(), {}, true, {}},
        {"symlink_file_follow", symlink_file.path(), {}, true, {}},
        {"symlink_file_nofollow", symlink_file.path(), {xinim::fs::mode::auto_detect, false, false}, true, {}},
        {"symlink_dir_follow", symlink_dir.path(), {}, true, {}},
        {"symlink_dir_nofollow", symlink_dir.path(), {xinim::fs::mode::auto_detect, false, false}, true, {}},
        {"dangling_symlink_follow", dangling_symlink.path(), {}, false, std::errc::no_such_file_or_directory},
        {"dangling_symlink_nofollow", dangling_symlink.path(), {xinim::fs::mode::auto_detect, false, false}, true, {}},
        {"missing_path", fs::temp_directory_path() / "xinim_status_missing", {}, false, std::errc::no_such_file_or_directory},
    };

    for (const auto& test : tests) {
        std::print(std::cout, "Test Case: {}... ", test.name);
        const auto result = xinim::fs::get_status(test.path, test.ctx);
        if (!test.expect_success) {
            expect_error(result, test.expected_error, test.name, failures);
            continue;
        }
        if (!result.has_value()) {
            std::println(std::cout, "FAIL ({}: get_status returned {})", test.name, result.error().message());
            ++failures;
            continue;
        }

        struct stat expected {};
        const int stat_rc = test.ctx.follow_symlinks ? ::stat(test.path.c_str(), &expected)
                                                     : ::lstat(test.path.c_str(), &expected);
        if (stat_rc != 0) {
            std::println(std::cout, "FAIL ({}: stat/lstat failed)", test.name);
            ++failures;
            continue;
        }

        if (compare_status(result.value(), expected, test.path, failures)) {
            std::println(std::cout, "PASS");
        } else {
            std::println(std::cout, "FAIL");
        }
    }

    std::print(std::cout, "Test Case: field_check_regular_file... ");
    temp_entity field_file("xinim_status_field_file", temp_entity::kind::file);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
        std::ofstream out(field_file.path(), std::ios::app);
        out << "more data";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto field_result = xinim::fs::get_status(field_file.path());
    if (!field_result.has_value()) {
        std::println(std::cout, "FAIL (get_status returned {})", field_result.error().message());
        ++failures;
    } else {
        struct stat expected {};
        if (::stat(field_file.path().c_str(), &expected) != 0) {
            std::println(std::cout, "FAIL (stat failed)");
            ++failures;
        } else if (compare_status(field_result.value(), expected, field_file.path(), failures)) {
            std::println(std::cout, "PASS");
        } else {
            std::println(std::cout, "FAIL");
        }
    }

    if (failures != 0) {
        std::println(std::cerr, "\n{} XINIM::FS::GET_STATUS TEST(S) FAILED.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "\nALL XINIM::FS::GET_STATUS TESTS PASSED.");
    return EXIT_SUCCESS;
}
