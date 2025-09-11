#pragma once

/*
 * XINIM xmake Integration Wrapper
 * Apache License Isolation Layer
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef XINIM_XMAKE_WRAPPER_HPP_INCLUDED_20250901
#define XINIM_XMAKE_WRAPPER_HPP_INCLUDED_20250901

#include <filesystem>
#include <string>
#include <vector>
#include <expected>

namespace xinim::third_party::apache::xmake {

/**
 * @brief Isolated xmake Integration Wrapper
 * 
 * This class provides a clean interface to xmake functionality while
 * maintaining strict license isolation. All xmake-specific code is
 * contained within this Apache-licensed module.
 */
class XMakeIntegration {
public:
    struct BuildConfig {
        std::string mode{"release"};
        std::string arch{"native"};
        std::filesystem::path source_dir;
        std::filesystem::path build_dir;
        std::vector<std::string> targets;
    };

    struct BuildResult {
        bool success{false};
        std::string output;
        std::chrono::milliseconds build_time{0};
        std::vector<std::filesystem::path> artifacts;
    };

    /**
     * @brief Initialize xmake integration
     * @param config Build configuration
     * @return Expected success or error message
     */
    [[nodiscard]] static std::expected<void, std::string> 
    initialize(const BuildConfig& config);

    /**
     * @brief Execute xmake build
     * @param config Build configuration  
     * @return Expected build result or error
     */
    [[nodiscard]] static std::expected<BuildResult, std::string>
    build(const BuildConfig& config);

    /**
     * @brief Clean build artifacts
     * @param build_dir Build directory to clean
     * @return Expected success or error message
     */
    [[nodiscard]] static std::expected<void, std::string>
    clean(const std::filesystem::path& build_dir);

    /**
     * @brief Get xmake version information
     * @return Expected version string or error
     */
    [[nodiscard]] static std::expected<std::string, std::string>
    get_version();

private:
    // Internal implementation details isolated from BSD code
    static bool is_xmake_available();
    static std::string execute_xmake_command(const std::string& command);
};

} // namespace xinim::third_party::apache::xmake

#endif // XINIM_XMAKE_WRAPPER_HPP_INCLUDED_20250901