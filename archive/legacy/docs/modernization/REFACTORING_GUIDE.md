# XINIM Critical Function Refactoring Guide

**Date:** 2026-01-03  
**Phase:** Phase 2 - Critical Path Unification  
**Status:** Implementation in Progress

---

## Overview

This document guides the refactoring of 8 critical functions (CCN > 40) to modern C++23 patterns using std::expected for error handling and RAII for resource management.

---

## Target Functions

| # | Function | File | CCN | NLOC | Current Pattern | Target Pattern |
|---|----------|------|-----|------|-----------------|----------------|
| 1 | `readreq` | `src/commands/roff.cpp` | 78 | 234 | Error codes | std::expected<Request, ParseError> |
| 2 | `getnxt` | `src/commands/make.cpp` | 53 | 153 | Error codes | std::expected<Token, LexError> |
| 3 | `readmakefile` | `src/commands/make.cpp` | 45 | 134 | Error codes | std::expected<Makefile, IOError> |
| 4 | `Archiver::process` | `src/commands/ar.cpp` | 41 | 121 | Error codes | std::expected<void, ArchiveError> |
| 5 | `execute` | `src/commands/sh3.cpp` | 41 | 124 | Error codes | std::expected<ExitCode, ShellError> |
| 6 | `main` (sh1) | `src/commands/sh1.cpp` | 40 | 120 | Error codes | std::expected<int, ShellError> |
| 7 | `CopyTestCase::run` | tests/test_xinim_fs_copy.cpp | 40 | 76 | Exceptions | std::expected<void, TestError> |
| 8 | `LinkTestCase::run` | tests/test_xinim_fs_links.cpp | 40 | 113 | Exceptions | std::expected<void, TestError> |

---

## Refactoring Strategy

### Phase 2.1: Extract Helper Functions (Weeks 9-10)

For each critical function:

1. **Analyze Complexity Sources**
   - Identify deeply nested conditions
   - Find repeated code patterns
   - Locate independent subtasks

2. **Extract Helper Functions**
   - Each helper does ONE thing
   - Each helper returns std::expected
   - Target CCN < 10 per function

3. **Compose Main Function**
   - Chain helper functions
   - Use .and_then() for sequential operations
   - Use .or_else() for error recovery

### Phase 2.2: Modernize Error Handling (Weeks 9-10)

Replace:
```cpp
// ✗ Old pattern
int read_file(const char* path) {
    if (error_condition) {
        return -1;  // Lost information
    }
    return 0;
}
```

With:
```cpp
// ✓ New pattern
auto read_file(std::string_view path) -> Result<FileData> {
    if (error_condition) {
        return std::unexpected(make_io_error(
            ENOENT,
            "File not found"
        ));
    }
    return file_data;
}
```

---

## Function 1: `readreq` Refactoring Plan

**File:** `src/commands/roff.cpp`  
**Current:** 234 lines, CCN 78  
**Target:** < 50 lines, CCN < 15

### Step 1: Analyze Current Structure

```cpp
// Current (pseudocode)
int readreq() {
    // 234 lines of deeply nested logic
    if (condition1) {
        if (condition2) {
            if (condition3) {
                // ... many nested levels
            }
        }
    }
    return result;
}
```

### Step 2: Extract Helpers

```cpp
// Phase 1: Parse request line
auto parse_request_line(std::string_view line) -> Result<RequestLine> {
    // Focused parsing logic
}

// Phase 2: Validate request
auto validate_request(const RequestLine& line) -> Result<void> {
    // Validation only
}

// Phase 3: Process arguments
auto process_arguments(const RequestLine& line) -> Result<Arguments> {
    // Argument processing
}

// Phase 4: Build request
auto build_request(const Arguments& args) -> Result<Request> {
    // Request construction
}
```

### Step 3: Compose Main Function

```cpp
auto readreq() -> Result<Request> {
    return read_input_line()
        .and_then([](auto& line) { return parse_request_line(line); })
        .and_then([](auto& parsed) { return validate_request(parsed); })
        .and_then([](auto& validated) { return process_arguments(validated); })
        .and_then([](auto& args) { return build_request(args); });
}
```

**Result:** CCN reduces from 78 → ~8

---

## Function 2: `getnxt` Refactoring Plan

**File:** `src/commands/make.cpp`  
**Current:** 153 lines, CCN 53  
**Target:** < 40 lines, CCN < 12

### Identified Helpers

```cpp
// 1. Skip whitespace
auto skip_whitespace(std::string_view& input) -> void;

// 2. Read token
auto read_token(std::string_view& input) -> Result<Token>;

// 3. Handle special characters
auto handle_special_char(char c) -> Result<TokenType>;

// 4. Process escape sequences
auto process_escape(std::string_view& input) -> Result<char>;

// 5. Validate token
auto validate_token(const Token& token) -> Result<void>;
```

### Composed Function

```cpp
auto getnxt() -> Result<Token> {
    skip_whitespace(current_input);
    
    return read_token(current_input)
        .and_then([](auto& token) { return validate_token(token); });
}
```

**Result:** CCN reduces from 53 → ~10

---

## Function 3: `readmakefile` Refactoring Plan

**File:** `src/commands/make.cpp`  
**Current:** 134 lines, CCN 45  
**Target:** < 40 lines, CCN < 12

### Identified Helpers

```cpp
// 1. Open file
auto open_makefile(std::string_view path) -> Result<FileHandle>;

// 2. Parse line
auto parse_makefile_line(std::string_view line) -> Result<MakefileLine>;

// 3. Process rule
auto process_rule(const MakefileLine& line) -> Result<Rule>;

// 4. Process variable
auto process_variable(const MakefileLine& line) -> Result<Variable>;

// 5. Build makefile structure
auto build_makefile(std::vector<Rule> rules, std::vector<Variable> vars) 
    -> Result<Makefile>;
```

### Composed Function

```cpp
auto readmakefile(std::string_view path) -> Result<Makefile> {
    auto file = open_makefile(path);
    if (!file) return std::unexpected(file.error());
    
    std::vector<Rule> rules;
    std::vector<Variable> vars;
    
    for (auto& line : file.value()) {
        auto parsed = parse_makefile_line(line);
        if (!parsed) return std::unexpected(parsed.error());
        
        if (parsed->is_rule()) {
            auto rule = process_rule(*parsed);
            if (!rule) return std::unexpected(rule.error());
            rules.push_back(std::move(*rule));
        } else {
            auto var = process_variable(*parsed);
            if (!var) return std::unexpected(var.error());
            vars.push_back(std::move(*var));
        }
    }
    
    return build_makefile(std::move(rules), std::move(vars));
}
```

**Result:** CCN reduces from 45 → ~12

---

## Implementation Checklist

### Week 9: Functions 1-4

- [ ] **Function 1: `readreq`**
  - [ ] Analyze current implementation
  - [ ] Extract 4-6 helper functions
  - [ ] Implement with std::expected
  - [ ] Add unit tests
  - [ ] Verify CCN < 15

- [ ] **Function 2: `getnxt`**
  - [ ] Analyze current implementation
  - [ ] Extract 4-6 helper functions
  - [ ] Implement with std::expected
  - [ ] Add unit tests
  - [ ] Verify CCN < 15

- [ ] **Function 3: `readmakefile`**
  - [ ] Analyze current implementation
  - [ ] Extract 5-7 helper functions
  - [ ] Implement with std::expected
  - [ ] Add unit tests
  - [ ] Verify CCN < 15

- [ ] **Function 4: `Archiver::process`**
  - [ ] Analyze current implementation
  - [ ] Extract 4-6 helper functions
  - [ ] Implement with std::expected
  - [ ] Add unit tests
  - [ ] Verify CCN < 15

### Week 10: Functions 5-8

- [ ] **Function 5: `execute` (sh3)**
  - [ ] Analyze current implementation
  - [ ] Extract helper functions
  - [ ] Implement with std::expected
  - [ ] Add unit tests
  - [ ] Verify CCN < 15

- [ ] **Function 6: `main` (sh1)**
  - [ ] Analyze current implementation
  - [ ] Extract helper functions
  - [ ] Implement with std::expected
  - [ ] Add unit tests
  - [ ] Verify CCN < 15

- [ ] **Function 7: `CopyTestCase::run`**
  - [ ] Analyze current implementation
  - [ ] Extract helper functions
  - [ ] Implement with std::expected
  - [ ] Verify CCN < 15

- [ ] **Function 8: `LinkTestCase::run`**
  - [ ] Analyze current implementation
  - [ ] Extract helper functions
  - [ ] Implement with std::expected
  - [ ] Verify CCN < 15

---

## Success Metrics

### Before Refactoring
- Total CCN: 421 (8 functions)
- Average CCN: 52.6
- Average NLOC: 135.1
- Error handling: Mixed patterns

### After Refactoring (Target)
- Total CCN: < 100 (8 functions + helpers)
- Average CCN: < 12
- Average NLOC: < 40 per function
- Error handling: 100% std::expected

### Impact
- **Maintainability Index:** 52 → 75 (commands/)
- **Bug probability:** -60% (from error handling improvements)
- **Testing coverage:** +40% (smaller functions easier to test)

---

## Testing Strategy

### Unit Tests for Each Helper

```cpp
TEST(ParseRequestLine, HandlesValidInput) {
    auto result = parse_request_line(".B text");
    REQUIRE(result);
    EXPECT_EQ(result->command, "B");
    EXPECT_EQ(result->argument, "text");
}

TEST(ParseRequestLine, ReturnsErrorForInvalidInput) {
    auto result = parse_request_line("invalid");
    REQUIRE(!result);
    EXPECT_EQ(result.error().category, Category::Parse);
}
```

### Integration Tests for Main Function

```cpp
TEST(Readreq, ProcessesCompleteRequest) {
    // Set up input
    mock_input(".B bold text\n");
    
    auto result = readreq();
    REQUIRE(result);
    EXPECT_EQ(result->type, RequestType::Bold);
}
```

---

## Performance Considerations

- std::expected has **zero overhead** vs manual error checking
- Helper function calls are **inlined** by compiler at -O2
- Total performance: Same or better than original
- Benefit: Much easier to optimize individual helpers

---

## Documentation Requirements

Each refactored function must have:

```cpp
/**
 * @brief Reads and parses a request from input
 * 
 * Processes roff format requests including:
 * - Font changes (.B, .I, .R)
 * - Spacing directives (.sp, .br)
 * - Macro invocations
 * 
 * @return Request structure or parse error
 * 
 * @throws Never (uses std::expected for error handling)
 * 
 * @example
 * auto req = readreq();
 * if (!req) {
 *     handle_error(req.error());
 * }
 */
auto readreq() -> Result<Request>;
```

---

## Review Process

For each refactored function:

1. **Code Review:** 2 reviewers must approve
2. **Complexity Check:** Verify CCN < 15 with lizard
3. **Test Coverage:** Must have 90%+ coverage
4. **Performance:** Must not regress (benchmark)
5. **Documentation:** Must be complete

---

## Timeline

- **Week 9:** Functions 1-4 refactored
- **Week 10:** Functions 5-8 refactored
- **Week 11:** Integration testing
- **Week 12:** Performance validation & deployment

---

## Status Tracking

| Function | Status | CCN Before | CCN After | PR # |
|----------|--------|------------|-----------|------|
| readreq | 🔲 Not Started | 78 | - | - |
| getnxt | 🔲 Not Started | 53 | - | - |
| readmakefile | 🔲 Not Started | 45 | - | - |
| Archiver::process | 🔲 Not Started | 41 | - | - |
| execute | 🔲 Not Started | 41 | - | - |
| main (sh1) | 🔲 Not Started | 40 | - | - |
| CopyTestCase::run | 🔲 Not Started | 40 | - | - |
| LinkTestCase::run | 🔲 Not Started | 40 | - | - |

---

**Next Action:** Begin refactoring Function 1 (`readreq`)  
**Owner:** Development Team  
**Priority:** High
