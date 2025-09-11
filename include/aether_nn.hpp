#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aether {

using f32 = float;
using usize = std::size_t;

/**
 * @brief Simple linear arena allocator.
 *
 * The arena is backed by a std::vector to ease integration with the existing
 * codebase while retaining the explicit allocation semantics of the original
 * C implementation. Allocation is bump‑pointer style and reset() releases all
 * memory at once.
 */
struct Arena {
    std::vector<unsigned char> buffer{};
    usize used{0};

    explicit Arena(usize size) : buffer(size) {}

    void reset() { used = 0; }

    void *alloc(usize bytes, usize align) {
        auto base = reinterpret_cast<usize>(buffer.data());
        usize p = base + used;
        usize aligned = (p + (align - 1)) & ~(align - 1);
        usize new_used = aligned - base + bytes;
        if (new_used > buffer.size()) {
            return nullptr;
        }
        used = new_used;
        return reinterpret_cast<void *>(aligned);
    }
};

struct Tensor {
    f32 *data{nullptr};
    usize n{0};
};

inline Tensor tensor_new(Arena &A, usize n) {
    auto *ptr = static_cast<f32 *>(A.alloc(n * sizeof(f32), alignof(f32)));
    return Tensor{ptr, n};
}

inline void tensor_zero(Tensor t) {
    for (usize i = 0; i < t.n; ++i) {
        t.data[i] = 0.0f;
    }
}

inline f32 dot(const f32 *a, const f32 *b, usize n) {
    f32 s = 0.0f;
    for (usize i = 0; i < n; ++i) {
        s += a[i] * b[i];
    }
    return s;
}

// ========================= Embedding + Aggregator =========================

enum class AggKind { Sum, Shift };

struct Embedding {
    usize vocab{0};
    usize dim{0};
    std::vector<f32> table;

    Embedding(Arena &A, usize vocab_, usize dim_, uint32_t seed);
};

struct Aggregator {
    AggKind kind{AggKind::Sum};
    usize dim{0};
    bool norm{true};
};

Tensor embedding_forward(Arena &A, const Embedding &E, const std::vector<usize> &idx,
                         const Aggregator &G);

void embedding_sgd(Embedding &E, const std::vector<usize> &idx, const Tensor &g, f32 lr,
                   const Aggregator &G, Arena &scratch);

// ========================= Dense Layer =========================

struct Dense {
    usize in{0};
    usize out{0};
    std::vector<f32> W;
    std::vector<f32> b;

    Dense(Arena &A, usize in_, usize out_, uint32_t seed);
};

enum class OptimKind { SGD };

struct Optim {
    OptimKind kind{OptimKind::SGD};
    f32 lr{0.01f};
    f32 l2{0.0f};
};

Tensor dense_forward(Arena &A, const Dense &L, Tensor x);

f32 softmax_ce_from_logits(f32 *z, usize C, usize y_true);

f32 softmax_train(Dense &L, Tensor x, usize y_true, Optim &opt, f32 *out_logits);

// ========================= High-level training step =========================

f32 model_train_step_softmax(Arena &scratch, Embedding &E, Dense &D, const std::vector<usize> &idx,
                             usize y_true, Optim &opt, const Aggregator &G);

} // namespace aether
