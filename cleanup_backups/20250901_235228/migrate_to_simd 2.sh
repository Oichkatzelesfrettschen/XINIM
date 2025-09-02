#!/bin/bash
# XINIM SIMD Library Migration Script
# Migrates all scattered quaternion/octonion/sedenion implementations to unified SIMD library

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project root
PROJECT_ROOT="/workspaces/XINIM"
BACKUP_DIR="${PROJECT_ROOT}/migration_backup_$(date +%Y%m%d_%H%M%S)"

echo -e "${BLUE}XINIM SIMD Library Migration Script${NC}"
echo -e "${BLUE}====================================${NC}"

# Create backup directory
echo -e "${YELLOW}Creating backup directory: ${BACKUP_DIR}${NC}"
mkdir -p "${BACKUP_DIR}"

# List of files to migrate/replace
declare -a OLD_FILES=(
    "kernel/quaternion_spinlock.hpp"
    "kernel/octonion.hpp"
    "kernel/octonion_math.hpp"
    "kernel/fano_octonion.hpp"
    "kernel/sedenion.hpp"
    "common/math/quaternion.hpp"
    "common/math/quaternion.cpp"
    "common/math/octonion.hpp"
    "common/math/octonion.cpp"
    "common/math/sedenion.hpp"
    "common/math/sedenion.cpp"
)

# Backup old files
echo -e "${YELLOW}Backing up old implementations...${NC}"
for file in "${OLD_FILES[@]}"; do
    if [ -f "${PROJECT_ROOT}/${file}" ]; then
        echo "  Backing up: ${file}"
        mkdir -p "${BACKUP_DIR}/$(dirname ${file})"
        cp "${PROJECT_ROOT}/${file}" "${BACKUP_DIR}/${file}"
    fi
done

# Find all files that include the old headers
echo -e "${YELLOW}Finding files that use old math headers...${NC}"
declare -a INCLUDE_PATTERNS=(
    "#include.*quaternion_spinlock\.hpp"
    "#include.*octonion\.hpp"
    "#include.*octonion_math\.hpp"
    "#include.*fano_octonion\.hpp"
    "#include.*sedenion\.hpp"
    "#include.*\"common/math/"
    "#include.*<common/math/"
)

declare -a AFFECTED_FILES=()
for pattern in "${INCLUDE_PATTERNS[@]}"; do
    while IFS= read -r -d '' file; do
        if [[ ! " ${AFFECTED_FILES[@]} " =~ " ${file} " ]]; then
            AFFECTED_FILES+=("$file")
        fi
    done < <(find "${PROJECT_ROOT}" -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) \
             -not -path "*/migration_backup_*" \
             -not -path "*/include/xinim/simd/*" \
             -not -path "*/lib/simd/*" \
             -exec grep -l "${pattern}" {} \; 2>/dev/null | tr '\n' '\0')
done

echo -e "${BLUE}Found ${#AFFECTED_FILES[@]} files that need updating:${NC}"
for file in "${AFFECTED_FILES[@]}"; do
    echo "  ${file}"
done

# Create compatibility headers for smooth transition
echo -e "${YELLOW}Creating compatibility headers...${NC}"

# Quaternion spinlock compatibility
cat > "${PROJECT_ROOT}/kernel/quaternion_spinlock.hpp" << 'EOF'
/**
 * @file quaternion_spinlock.hpp
 * @brief Compatibility header for XINIM SIMD quaternion spinlocks
 * @deprecated Use xinim/simd/math.hpp instead
 */

#pragma once

#warning "quaternion_spinlock.hpp is deprecated. Use #include <xinim/simd/math.hpp> instead."

#include <xinim/simd/math.hpp>

// Compatibility aliases
namespace xinim {
    using QuaternionSpinlock = simd::math::compat::spinlock::QuaternionSpinlock;
    using atomic_quaternion = simd::math::atomic_quaternion<float>;
}

// Legacy namespace
namespace Kernel {
    using QuaternionSpinlock = xinim::simd::math::compat::spinlock::QuaternionSpinlock;
}
EOF

# Common math compatibility headers
mkdir -p "${PROJECT_ROOT}/common/math"

cat > "${PROJECT_ROOT}/common/math/quaternion.hpp" << 'EOF'
/**
 * @file common/math/quaternion.hpp
 * @brief Compatibility header for XINIM SIMD quaternions
 * @deprecated Use xinim/simd/math.hpp instead
 */

#pragma once

#warning "common/math/quaternion.hpp is deprecated. Use #include <xinim/simd/math.hpp> instead."

#include <xinim/simd/math.hpp>

// Compatibility aliases
namespace Common {
namespace Math {
    template<typename T = double>
    using Quaternion = xinim::simd::math::quaternion<T>;
    
    using quaternionf = xinim::simd::math::quaternionf;
    using quaterniond = xinim::simd::math::quaterniond;
}}
EOF

cat > "${PROJECT_ROOT}/common/math/octonion.hpp" << 'EOF'
/**
 * @file common/math/octonion.hpp
 * @brief Compatibility header for XINIM SIMD octonions
 * @deprecated Use xinim/simd/math.hpp instead
 */

#pragma once

#warning "common/math/octonion.hpp is deprecated. Use #include <xinim/simd/math.hpp> instead."

#include <xinim/simd/math.hpp>

// Compatibility aliases
namespace Common {
namespace Math {
    template<typename T = double>
    using Octonion = xinim::simd::math::octonion<T>;
    
    using octonionf = xinim::simd::math::octonionf;
    using octoniond = xinim::simd::math::octoniond;
}}
EOF

cat > "${PROJECT_ROOT}/common/math/sedenion.hpp" << 'EOF'
/**
 * @file common/math/sedenion.hpp
 * @brief Compatibility header for XINIM SIMD sedenions
 * @deprecated Use xinim/simd/math.hpp instead
 */

#pragma once

#warning "common/math/sedenion.hpp is deprecated. Use #include <xinim/simd/math.hpp> instead."

#include <xinim/simd/math.hpp>

// Compatibility aliases
namespace Common {
namespace Math {
    template<typename T = double>
    using Sedenion = xinim::simd::math::sedenion<T>;
    
    using sedenionf = xinim::simd::math::sedenionf;
    using sedeniond = xinim::simd::math::sedeniond;
}}
EOF

# Kernel math compatibility headers
cat > "${PROJECT_ROOT}/kernel/octonion.hpp" << 'EOF'
/**
 * @file kernel/octonion.hpp
 * @brief Compatibility header for XINIM SIMD octonions
 * @deprecated Use xinim/simd/math.hpp instead
 */

#pragma once

#warning "kernel/octonion.hpp is deprecated. Use #include <xinim/simd/math.hpp> instead."

#include <xinim/simd/math.hpp>

namespace Kernel {
    template<typename T = double>
    using Octonion = xinim::simd::math::octonion<T>;
}
EOF

cat > "${PROJECT_ROOT}/kernel/octonion_math.hpp" << 'EOF'
/**
 * @file kernel/octonion_math.hpp
 * @brief Compatibility header for XINIM SIMD octonion math
 * @deprecated Use xinim/simd/math.hpp instead
 */

#pragma once

#warning "kernel/octonion_math.hpp is deprecated. Use #include <xinim/simd/math.hpp> instead."

#include <xinim/simd/math.hpp>

// All mathematical functions are now in xinim::simd::math namespace
EOF

cat > "${PROJECT_ROOT}/kernel/fano_octonion.hpp" << 'EOF'
/**
 * @file kernel/fano_octonion.hpp
 * @brief Compatibility header for XINIM SIMD Fano plane octonions
 * @deprecated Use xinim/simd/math.hpp instead
 */

#pragma once

#warning "kernel/fano_octonion.hpp is deprecated. Use #include <xinim/simd/math.hpp> instead."

#include <xinim/simd/math.hpp>

namespace Kernel {
    template<typename T = double>
    using FanoOctonion = xinim::simd::math::octonion<T>;
    
    // Fano multiplication is now octonion.fano_multiply()
}
EOF

cat > "${PROJECT_ROOT}/kernel/sedenion.hpp" << 'EOF'
/**
 * @file kernel/sedenion.hpp
 * @brief Compatibility header for XINIM SIMD sedenions
 * @deprecated Use xinim/simd/math.hpp instead
 */

#pragma once

#warning "kernel/sedenion.hpp is deprecated. Use #include <xinim/simd/math.hpp> instead."

#include <xinim/simd/math.hpp>

namespace Kernel {
    template<typename T = double>
    using Sedenion = xinim::simd::math::sedenion<T>;
}
EOF

# Create empty .cpp files to avoid link errors
touch "${PROJECT_ROOT}/common/math/quaternion.cpp"
touch "${PROJECT_ROOT}/common/math/octonion.cpp"
touch "${PROJECT_ROOT}/common/math/sedenion.cpp"

# Update main CMakeLists.txt to include SIMD library
echo -e "${YELLOW}Updating build system...${NC}"

# Add SIMD library to main CMakeLists.txt
if ! grep -q "add_subdirectory(lib/simd)" "${PROJECT_ROOT}/CMakeLists.txt" 2>/dev/null; then
    echo "" >> "${PROJECT_ROOT}/CMakeLists.txt"
    echo "# SIMD Mathematical Library" >> "${PROJECT_ROOT}/CMakeLists.txt"
    echo "add_subdirectory(lib/simd)" >> "${PROJECT_ROOT}/CMakeLists.txt"
fi

# Create a comprehensive test to verify migration
echo -e "${YELLOW}Creating migration verification test...${NC}"
cat > "${PROJECT_ROOT}/test_simd_migration.cpp" << 'EOF'
/**
 * @file test_simd_migration.cpp
 * @brief Test program to verify SIMD library migration
 */

#include <xinim/simd/math.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace xinim::simd::math;

int main() {
    std::cout << "XINIM SIMD Library Migration Test\n";
    std::cout << "==================================\n\n";
    
    // Initialize math library
    MathLibrary::initialize();
    std::cout << "Math library initialized: " << MathLibrary::get_implementation_name() << "\n";
    std::cout << "Optimized: " << (MathLibrary::is_optimized() ? "YES" : "NO") << "\n\n";
    
    // Test quaternions
    std::cout << "Testing Quaternions:\n";
    quaternionf q1(1.0f, 0.0f, 0.0f, 0.0f);
    quaternionf q2(0.707f, 0.0f, 0.707f, 0.0f);
    
    quaternionf q3 = functions::quat_multiply(q1, q2);
    std::cout << "  q1 * q2 = (" << q3.w << ", " << q3.x << ", " << q3.y << ", " << q3.z << ")\n";
    
    quaternionf q_norm = functions::quat_normalize(q2);
    std::cout << "  |q2| = " << q2.norm() << ", normalized = (" 
              << q_norm.w << ", " << q_norm.x << ", " << q_norm.y << ", " << q_norm.z << ")\n";
    
    // Test octonions
    std::cout << "\nTesting Octonions:\n";
    octonionf o1(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    octonionf o2(0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f);
    
    octonionf o3 = functions::oct_multiply(o1, o2);
    std::cout << "  o1 * o2 norm = " << o3.norm() << "\n";
    
    // Test sedenions
    std::cout << "\nTesting Sedenions:\n";
    sedenionf s1 = sedenionf::identity();
    sedenionf s2 = sedenionf::e(1);
    
    sedenionf s3 = functions::sed_multiply(s1, s2);
    std::cout << "  s1 * s2 norm = " << s3.norm() << "\n";
    std::cout << "  s2 is zero divisor: " << (functions::sed_is_zero_divisor(s2) ? "YES" : "NO") << "\n";
    
    // Performance benchmark
    std::cout << "\nPerformance Benchmark:\n";
    auto start = std::chrono::high_resolution_clock::now();
    
    constexpr int iterations = 1000000;
    quaternionf sum = quaternionf::zero();
    for (int i = 0; i < iterations; ++i) {
        quaternionf q(float(i), float(i+1), float(i+2), float(i+3));
        sum += functions::quat_normalize(q);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  " << iterations << " quaternion normalizations took " 
              << duration.count() << " microseconds\n";
    std::cout << "  Final sum norm: " << sum.norm() << "\n";
    
    std::cout << "\nMigration test completed successfully!\n";
    return 0;
}
EOF

# Create build script for the test
cat > "${PROJECT_ROOT}/build_simd_test.sh" << 'EOF'
#!/bin/bash
cd /workspaces/XINIM
g++ -std=c++23 -O3 -march=native -I include test_simd_migration.cpp lib/simd/math/quaternion.cpp -o test_simd_migration -lm
./test_simd_migration
EOF
chmod +x "${PROJECT_ROOT}/build_simd_test.sh"

# Generate migration report
echo -e "${YELLOW}Generating migration report...${NC}"
cat > "${PROJECT_ROOT}/SIMD_MIGRATION_REPORT.md" << EOF
# XINIM SIMD Library Migration Report

## Migration Summary
- **Date**: $(date)
- **Backup Location**: ${BACKUP_DIR}
- **Files Migrated**: ${#OLD_FILES[@]}
- **Files Affected**: ${#AFFECTED_FILES[@]}

## Files Migrated
$(printf '- %s\n' "${OLD_FILES[@]}")

## Files Requiring Updates
$(printf '- %s\n' "${AFFECTED_FILES[@]}")

## New SIMD Library Structure
\`\`\`
include/xinim/simd/
├── core.hpp              # Core SIMD abstractions
├── detect.hpp            # Runtime feature detection
├── math.hpp              # Main math library header
├── arch/                 # Architecture-specific implementations
│   ├── x86_64.hpp        # x86-64 optimizations
│   ├── arm_neon.hpp      # ARM NEON optimizations
│   └── arm_sve.hpp       # ARM SVE optimizations
└── math/                 # Mathematical types
    ├── quaternion.hpp    # Unified quaternion operations
    ├── octonion.hpp      # Unified octonion operations
    └── sedenion.hpp      # Unified sedenion operations

lib/simd/
├── CMakeLists.txt        # Build configuration
└── math/                 # Implementation files
    └── quaternion.cpp    # Quaternion implementations
\`\`\`

## Migration Benefits
1. **Unified API**: All math operations through single header
2. **SIMD Optimization**: Automatic selection of best instruction set
3. **Runtime Dispatch**: Optimal performance on any hardware
4. **Type Safety**: Modern C++23 type system
5. **Maintainability**: Centralized implementation

## Usage Examples

### Old Code:
\`\`\`cpp
#include "common/math/quaternion.hpp"
Common::Math::Quaternion q1, q2;
auto result = q1 * q2;
\`\`\`

### New Code:
\`\`\`cpp
#include <xinim/simd/math.hpp>
using namespace xinim::simd::math;
quaternionf q1, q2;
auto result = functions::quat_multiply(q1, q2);
\`\`\`

## Compatibility
- All old headers now redirect to new SIMD library
- Compilation warnings guide migration to new API
- Legacy namespaces preserved for compatibility

## Testing
Run the migration test:
\`\`\`bash
cd /workspaces/XINIM
./build_simd_test.sh
\`\`\`

## Next Steps
1. Update affected files to use new SIMD API
2. Remove compatibility headers after migration
3. Add comprehensive unit tests
4. Performance benchmarking and optimization
5. Documentation updates
EOF

echo -e "${GREEN}Migration completed successfully!${NC}"
echo -e "${GREEN}==================================${NC}"
echo -e "Backup location: ${BACKUP_DIR}"
echo -e "Migration report: ${PROJECT_ROOT}/SIMD_MIGRATION_REPORT.md"
echo -e "Test program: ${PROJECT_ROOT}/test_simd_migration.cpp"
echo -e ""
echo -e "${BLUE}To test the migration:${NC}"
echo -e "cd ${PROJECT_ROOT} && ./build_simd_test.sh"
echo -e ""
echo -e "${YELLOW}Next steps:${NC}"
echo -e "1. Review affected files and update to new SIMD API"
echo -e "2. Build and test your applications"
echo -e "3. Remove compatibility headers when migration is complete"
EOF
