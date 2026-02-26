# SIMD Migration Workflow

```{doxygenfile} include/xinim/simd/math.hpp
:project: xinim
```

The legacy `migrate_to_simd.sh` script has been retired in favor of explicit, auditable steps. The following manual procedure reproduces the original automation while enabling fine-grained control over the migration process.

## 1. Create a Backup

Preserve existing quaternion, octonion, and sedenion implementations before modifying the source tree.

```bash
PROJECT_ROOT=/workspaces/XINIM
BACKUP_DIR="${PROJECT_ROOT}/migration_backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "${BACKUP_DIR}"
```

Copy each legacy header and source into the backup directory. The list below mirrors the files previously handled by the script.

- `kernel/quaternion_spinlock.hpp`
- `kernel/octonion.hpp`
- `kernel/octonion_math.hpp`
- `kernel/fano_octonion.hpp`
- `kernel/sedenion.hpp`
- `common/math/quaternion.(hpp|cpp)`
- `common/math/octonion.(hpp|cpp)`
- `common/math/sedenion.(hpp|cpp)`

## 2. Locate Dependencies

Identify translation units that include the deprecated headers so they can be migrated to the unified SIMD library.

```bash
find "${PROJECT_ROOT}" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) \
    -not -path "*/migration_backup_*" \
    -not -path "*/include/xinim/simd/*" \
    -not -path "*/lib/simd/*" \
    -exec grep -lE "#include.*(quaternion_spinlock|octonion|octonion_math|fano_octonion|sedenion|common/math)" {} +
```

Record the output to guide manual code refactoring.

## 3. Provide Compatibility Headers

Introduce temporary wrappers that redirect legacy includes to the new SIMD API. Each wrapper should issue a compile-time warning to encourage eventual removal.

```cpp
#pragma once
#warning "quaternion_spinlock.hpp is deprecated. Use <xinim/simd/math.hpp>."
#include <xinim/simd/math.hpp>
namespace xinim {
  using QuaternionSpinlock = simd::math::compat::spinlock::QuaternionSpinlock;
}
```

Replicate this pattern for `quaternion`, `octonion`, and `sedenion` types within `common/math/`.

## 4. Build and Run the Sanity Test

A minimal test validates that the new SIMD library links and executes correctly.

```bash
cat <<'CPP' > test_simd_migration.cpp
#include <xinim/simd/math.hpp>
#include <chrono>
#include <iostream>
using namespace xinim::simd::math;
int main() {
  auto start = std::chrono::high_resolution_clock::now();
  quaternionf sum = quaternionf::zero();
  for (int i = 0; i < 1000000; ++i) {
    quaternionf q(float(i), float(i+1), float(i+2), float(i+3));
    sum += functions::quat_normalize(q);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "Completed in "
            << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
            << " microseconds\n";
}
CPP

g++ -std=c++23 -O3 -march=native -I include test_simd_migration.cpp \
    lib/simd/math/quaternion.cpp -o test_simd_migration -lm
./test_simd_migration
```

## 5. Generate a Migration Report

Document the migration by recording the backup location, modified files, and remaining tasks.

```bash
cat > SIMD_MIGRATION_REPORT.md <<'EOF'
# XINIM SIMD Library Migration Report
- Date: $(date)
- Backup: ${BACKUP_DIR}
EOF
```

Extend the report with lists of migrated files and any translation units that still require attention.

## 6. Next Steps

1. Refactor all affected source files to consume `<xinim/simd/math.hpp>` exclusively.
2. Remove the compatibility headers once the codebase no longer relies on legacy includes.
3. Augment the test suite to cover SIMD-enabled math routines.
4. Delete the backup after verifying that the migration is complete.

These instructions ensure the migration proceeds deterministically while remaining transparent for code review and documentation purposes.
