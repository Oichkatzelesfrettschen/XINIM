#include "aether_nn.hpp"

#include <cmath>
#include <random>

namespace aether {

// ========================= Embedding =========================

Embedding::Embedding(Arena &A, usize vocab_, usize dim_, uint32_t seed)
    : vocab(vocab_), dim(dim_), table(vocab_ * dim_) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<f32> dist{-1.0f, 1.0f};
    f32 scale = std::sqrt(6.0f / static_cast<f32>(dim_));
    for (auto &w : table) {
        w = dist(rng) * scale;
    }
}

static void rotate_right(const f32 *x, f32 *out, usize n, usize shift) {
    shift %= n;
    for (usize d = 0; d < n; ++d) {
        usize s = (d + n - shift) % n;
        out[d] = x[s];
    }
}

Tensor embedding_forward(Arena &A, const Embedding &E, const std::vector<usize> &idx,
                         const Aggregator &G) {
    Tensor out = tensor_new(A, E.dim);
    tensor_zero(out);
    Tensor tmp = tensor_new(A, E.dim);
    const usize k = idx.size();
    for (usize j = 0; j < k; ++j) {
        const usize r = idx[j];
        if (r >= E.vocab) {
            continue;
        }
        const f32 *row = &E.table[r * E.dim];
        if (G.kind == AggKind::Sum) {
            for (usize d = 0; d < E.dim; ++d) {
                out.data[d] += row[d];
            }
        } else { // AggKind::Shift
            rotate_right(row, tmp.data, E.dim, j % E.dim);
            for (usize d = 0; d < E.dim; ++d) {
                out.data[d] += tmp.data[d];
            }
        }
    }
    if (G.norm && k > 0) {
        const f32 s = 1.0f / std::sqrt(static_cast<f32>(k));
        for (usize d = 0; d < E.dim; ++d) {
            out.data[d] *= s;
        }
    }
    return out;
}

void embedding_sgd(Embedding &E, const std::vector<usize> &idx, const Tensor &g, f32 lr,
                   const Aggregator &G, Arena &scratch) {
    const usize k = idx.size();
    if (G.kind == AggKind::Sum) {
        f32 scale = 1.0f;
        if (G.norm && k > 0) {
            scale = 1.0f / std::sqrt(static_cast<f32>(k));
        }
        for (usize j = 0; j < k; ++j) {
            const usize r = idx[j];
            if (r >= E.vocab) {
                continue;
            }
            f32 *row = &E.table[r * E.dim];
            for (usize d = 0; d < E.dim; ++d) {
                row[d] -= lr * scale * g.data[d];
            }
        }
        return;
    }
    Tensor tmp = tensor_new(scratch, E.dim);
    for (usize j = 0; j < k; ++j) {
        const usize r = idx[j];
        if (r >= E.vocab) {
            continue;
        }
        f32 *row = &E.table[r * E.dim];
        // rotate gradient left to match right rotation in forward pass
        rotate_right(g.data, tmp.data, E.dim, E.dim - (j % E.dim));
        f32 scale = 1.0f;
        if (G.norm && k > 0) {
            scale = 1.0f / std::sqrt(static_cast<f32>(k));
        }
        for (usize d = 0; d < E.dim; ++d) {
            row[d] -= lr * scale * tmp.data[d];
        }
    }
}

// ========================= Dense Layer =========================

Dense::Dense(Arena &, usize in_, usize out_, uint32_t seed)
    : in(in_), out(out_), W(out_ * in_), b(out_) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<f32> dist{-1.0f, 1.0f};
    const f32 scale = std::sqrt(6.0f / static_cast<f32>(in_ + out_));
    for (auto &w : W) {
        w = dist(rng) * scale;
    }
    for (auto &x : b) {
        x = 0.0f;
    }
}

Tensor dense_forward(Arena &A, const Dense &L, Tensor x) {
    Tensor y = tensor_new(A, L.out);
    for (usize o = 0; o < L.out; ++o) {
        const f32 *wrow = &L.W[o * L.in];
        y.data[o] = L.b[o] + dot(wrow, x.data, L.in);
    }
    return y;
}

f32 softmax_ce_from_logits(f32 *z, usize C, usize y_true) {
    f32 m = z[0];
    for (usize i = 1; i < C; ++i) {
        m = std::max(m, z[i]);
    }
    f32 sum = 0.0f;
    for (usize i = 0; i < C; ++i) {
        z[i] = std::exp(z[i] - m);
        sum += z[i];
    }
    for (usize i = 0; i < C; ++i) {
        z[i] /= sum;
    }
    const f32 e = 1e-9f;
    return -std::log(std::max(e, z[y_true]));
}

f32 softmax_train(Dense &L, Tensor x, usize y_true, Optim &opt, f32 *out_logits) {
    const usize C = L.out;
    std::vector<f32> z(C);
    for (usize o = 0; o < C; ++o) {
        const f32 *wrow = &L.W[o * L.in];
        z[o] = L.b[o] + dot(wrow, x.data, L.in);
    }
    f32 loss = softmax_ce_from_logits(z.data(), C, y_true);
    // gradient and update
    for (usize o = 0; o < C; ++o) {
        f32 delta = z[o] - (o == y_true ? 1.0f : 0.0f);
        f32 *wrow = &L.W[o * L.in];
        for (usize i = 0; i < L.in; ++i) {
            f32 g = delta * x.data[i] + opt.l2 * wrow[i];
            wrow[i] -= opt.lr * g;
        }
        L.b[o] -= opt.lr * delta;
    }
    if (out_logits) {
        for (usize o = 0; o < C; ++o) {
            out_logits[o] = z[o];
        }
    }
    return loss;
}

f32 model_train_step_softmax(Arena &scratch, Embedding &E, Dense &D, const std::vector<usize> &idx,
                             usize y_true, Optim &opt, const Aggregator &G) {
    scratch.reset();
    Tensor v = embedding_forward(scratch, E, idx, G);
    std::vector<f32> logits(D.out);
    f32 loss = softmax_train(D, v, y_true, opt, logits.data());
    Tensor gv = tensor_new(scratch, E.dim);
    for (usize i = 0; i < E.dim; ++i) {
        f32 acc = 0.0f;
        for (usize o = 0; o < D.out; ++o) {
            f32 p = logits[o];
            p -= (o == y_true ? 1.0f : 0.0f);
            acc += D.W[o * D.in + i] * p;
        }
        gv.data[i] = acc;
    }
    embedding_sgd(E, idx, gv, opt.lr, G, scratch);
    return loss;
}

} // namespace aether
