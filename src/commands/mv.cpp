/**
 * @file mv.cpp
 * @brief Portable host implementation of the `mv` utility.
 */

#include <filesystem>
#include <iostream>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using file_path = std::filesystem::path;

void print_error(std::string_view message, const std::error_code& ec = {}) {
    if (ec) {
        std::cerr << "mv: " << message << ": " << ec.message() << '\n';
    } else {
        std::cerr << "mv: " << message << '\n';
    }
}

bool move_item(const file_path& source, const file_path& target) {
    if (!std::filesystem::exists(source)) {
        print_error(std::string("cannot stat " + source.string()));
        return false;
    }

    std::error_code ec_rename;
    std::filesystem::rename(source, target, ec_rename);
    if (!ec_rename) {
        return true;
    }

    if (ec_rename == std::errc::cross_device_link) {
        std::error_code ec_copy;
        std::filesystem::copy(source, target,
                              std::filesystem::copy_options::recursive
                                  | std::filesystem::copy_options::overwrite_existing,
                              ec_copy);
        if (ec_copy) {
            print_error(std::string("cannot copy '" + source.string() + "'"), ec_copy);
            return false;
        }

        std::error_code ec_remove;
        std::filesystem::remove_all(source, ec_remove);
        if (ec_remove) {
            print_error(std::string("cannot remove '" + source.string() + "'"), ec_remove);
            return false;
        }
        return true;
    }

    print_error(std::string("cannot move '" + source.string() + "'"), ec_rename);
    return false;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: mv SOURCE... DIRECTORY\n";
        return 1;
    }

    std::vector<file_path> sources;
    for (int i = 1; i < argc - 1; ++i) {
        sources.emplace_back(argv[i]);
    }

    const file_path target = argv[argc - 1];
    bool multiple = sources.size() > 1;

    if (multiple && (!std::filesystem::is_directory(target) || !std::filesystem::exists(target))) {
        print_error(std::string("target '" + target.string() + "' must be a directory"));
        return 1;
    }

    bool ok = true;
    for (const auto& source : sources) {
        const auto destination = multiple
                                    ? (target / source.filename())
                                    : target;
        if (sources.size() == 1 && std::filesystem::is_directory(target)) {
            const auto renamed = target / source.filename();
            ok = move_item(source, renamed) && ok;
        } else {
            ok = move_item(source, destination) && ok;
        }
    }

    return ok ? 0 : 1;
}
