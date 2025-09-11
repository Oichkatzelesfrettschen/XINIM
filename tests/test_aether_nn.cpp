#include "aether_nn.hpp"

#include <cassert>
#include <iostream>
#include <vector>

int main() {
    using namespace aether;

    Arena perm(1 << 16);
    Arena scratch(1 << 16);
    Embedding E(perm, /*vocab=*/16, /*dim=*/8, 0x1234);
    Dense D(perm, /*in=*/8, /*out=*/3, 0x5678);
    Aggregator G{AggKind::Shift, 8, true};
    Optim opt{};
    opt.lr = 0.05f;

    std::vector<usize> A{1, 2};
    std::vector<usize> B{3, 4};
    std::vector<usize> C{5, 6};

    float loss = 0.0f;
    for (int t = 0; t < 200; ++t) {
        loss = model_train_step_softmax(scratch, E, D, A, 0, opt, G);
        loss += model_train_step_softmax(scratch, E, D, B, 1, opt, G);
        loss += model_train_step_softmax(scratch, E, D, C, 2, opt, G);
    }
    std::cout << "loss=" << loss / 3.0f << "\n";
    assert(loss / 3.0f < 0.2f);
    return 0;
}
