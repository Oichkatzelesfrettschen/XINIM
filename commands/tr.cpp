// tr.cpp - Modern C++23 implementation of the POSIX tr utility
// Author: Modernized by GitHub Copilot

#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <string_view>
#include <algorithm>
#include <iterator>
#include <cctype>
#include <ranges>
#include <span>
#include <optional>
#include <stdexcept>

constexpr size_t ASCII_SIZE = 256;
constexpr size_t BUFFER_SIZE = 4096;

struct TrOptions {
    bool complement = false;
    bool delete_mode = false;
    bool squeeze = false;
    std::string_view set1;
    std::string_view set2;
};

class TrTranslator {
public:
    TrTranslator(const TrOptions& opts)
        : options(opts)
    {
        build_translation();
    }

    void process(std::istream& in, std::ostream& out) {
        std::array<unsigned char, BUFFER_SIZE> inbuf{};
        std::array<unsigned char, BUFFER_SIZE> outbuf{};
        size_t outpos = 0;
        std::optional<unsigned char> last_output;

        while (in) {
            in.read(reinterpret_cast<char*>(inbuf.data()), inbuf.size());
            std::streamsize n = in.gcount();
            for (std::streamsize i = 0; i < n; ++i) {
                unsigned char c = inbuf[i];
                if (options.delete_mode && in_set[c])
                    continue;
                unsigned char mapped = translation[c];
                if (options.squeeze && last_output && *last_output == mapped && out_set[mapped])
                    continue;
                outbuf[outpos++] = mapped;
                last_output = mapped;
                if (outpos == outbuf.size()) {
                    out.write(reinterpret_cast<const char*>(outbuf.data()), outpos);
                    outpos = 0;
                }
            }
        }
        if (outpos > 0)
            out.write(reinterpret_cast<const char*>(outbuf.data()), outpos);
    }

private:
    TrOptions options;
    std::array<unsigned char, ASCII_SIZE> translation{};
    std::array<bool, ASCII_SIZE> in_set{};
    std::array<bool, ASCII_SIZE> out_set{};

    static std::vector<unsigned char> expand_set(std::string_view str) {
        std::vector<unsigned char> result;
        for (size_t i = 0; i < str.size(); ) {
            if (str[i] == '\\' && i + 1 < str.size()) {
                // Octal or escaped char
                if (std::isdigit(str[i+1]) && str[i+1] < '8') {
                    int val = 0, count = 0;
                    ++i;
                    while (count < 3 && i < str.size() && str[i] >= '0' && str[i] <= '7') {
                        val = (val << 3) + (str[i] - '0');
                        ++i; ++count;
                    }
                    result.push_back(static_cast<unsigned char>(val));
                } else {
                    result.push_back(static_cast<unsigned char>(str[i+1]));
                    i += 2;
                }
            } else if (i + 2 < str.size() && str[i+1] == '-' && str[i] != '\\' && str[i+2] != '\0') {
                // Range: a-z
                unsigned char start = str[i];
                unsigned char end = str[i+2];
                if (start > end)
                    throw std::runtime_error("Invalid range in set");
                for (unsigned char c = start; c <= end; ++c)
                    result.push_back(c);
                i += 3;
            } else {
                result.push_back(static_cast<unsigned char>(str[i]));
                ++i;
            }
        }
        return result;
    }

    void build_translation() {
        // Step 1: Expand sets
        std::vector<unsigned char> set1 = expand_set(options.set1);
        std::vector<unsigned char> set2 = expand_set(options.set2);

        // Step 2: Complement if needed
        if (options.complement) {
            std::array<bool, ASCII_SIZE> present{};
            for (unsigned char c : set1)
                present[c] = true;
            set1.clear();
            for (size_t i = 0; i < ASCII_SIZE; ++i)
                if (!present[i])
                    set1.push_back(static_cast<unsigned char>(i));
        }

        // Step 3: Build translation table
        for (size_t i = 0; i < ASCII_SIZE; ++i)
            translation[i] = static_cast<unsigned char>(i);

        if (!set1.empty()) {
            unsigned char last = set2.empty() ? set1.back() : set2.back();
            size_t n = set2.size();
            for (size_t i = 0; i < set1.size(); ++i) {
                translation[set1[i]] = (i < n) ? set2[i] : last;
            }
        }

        // Step 4: Build in_set and out_set for delete/squeeze
        for (unsigned char c : set1)
            in_set[c] = true;
        for (unsigned char c : set2)
            out_set[c] = true;
    }
};

TrOptions parse_args(int argc, char* argv[]) {
    TrOptions opts;
    int idx = 1;
    if (idx < argc && argv[idx][0] == '-') {
        for (const char* p = argv[idx] + 1; *p; ++p) {
            switch (*p) {
                case 'c': opts.complement = true; break;
                case 'd': opts.delete_mode = true; break;
                case 's': opts.squeeze = true; break;
                default:
                    std::cerr << "Usage: tr [-cds] [string1 [string2]]\n";
                    std::exit(1);
            }
        }
        ++idx;
    }
    if (idx < argc)
        opts.set1 = argv[idx++];
    if (idx < argc)
        opts.set2 = argv[idx++];
    return opts;
}

/**
 * @brief Entry point for the tr utility.
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return Exit status as specified by C++23 [basic.start.main].
 */
int main(int argc, char* argv[]) {
    try {
        TrOptions opts = parse_args(argc, argv);
        TrTranslator tr(opts);
        tr.process(std::cin, std::cout);
    } catch (const std::exception& ex) {
        std::cerr << "tr: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
