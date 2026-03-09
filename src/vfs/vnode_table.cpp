#include "vnode_table.hpp"

#include <cstring>

namespace {

static VnodeHandle g_vnodes[MAX_VNODES];
static uint16_t g_next_generation = 1U;
static uint32_t g_vnode_clock = 0U;

static VnodeHandle* select_reclaimable_slot() {
    VnodeHandle* best = nullptr;
    for (uint32_t index = 0; index < MAX_VNODES; ++index) {
        VnodeHandle& slot = g_vnodes[index];
        if (slot.ino == 0U) {
            return &slot;
        }
        if (slot.refs != 0U) {
            continue;
        }
        if (best == nullptr || slot.last_used < best->last_used) {
            best = &slot;
        }
    }
    return best;
}

} // namespace

void vnode_table_init() {
    __builtin_memset(g_vnodes, 0, sizeof(g_vnodes));
    g_next_generation = 1U;
    g_vnode_clock = 0U;
}

VnodeHandle* vnode_lookup(uint32_t ino) {
    if (ino == 0U) {
        return nullptr;
    }
    for (uint32_t index = 0; index < MAX_VNODES; ++index) {
        if (g_vnodes[index].ino == ino) {
            return &g_vnodes[index];
        }
    }
    return nullptr;
}

VnodeHandle* vnode_acquire(uint32_t ino) {
    if (ino == 0U) {
        return nullptr;
    }

    if (VnodeHandle* existing = vnode_lookup(ino)) {
        if (existing->refs != 0xFFFFU) {
            ++existing->refs;
        }
        existing->last_used = ++g_vnode_clock;
        return existing;
    }

    VnodeHandle* slot = select_reclaimable_slot();
    if (slot == nullptr) {
        return nullptr;
    }

    const uint16_t generation = g_next_generation++;
    *slot = {
        ino,
        1U,
        static_cast<uint16_t>(generation == 0U ? 1U : generation),
        ++g_vnode_clock,
        0U,
    };
    return slot;
}

void vnode_release(uint32_t ino) {
    VnodeHandle* vnode = vnode_lookup(ino);
    if (vnode == nullptr) {
        return;
    }
    if (vnode->refs != 0U) {
        --vnode->refs;
    }
    vnode->last_used = ++g_vnode_clock;
}

void vnode_forget(uint32_t ino) {
    VnodeHandle* vnode = vnode_lookup(ino);
    if (vnode == nullptr) {
        return;
    }
    __builtin_memset(vnode, 0, sizeof(*vnode));
}
