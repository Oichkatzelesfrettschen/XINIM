#include "sysv_ipc.hpp"

namespace xinim::i486::ipc {
namespace {

// -- Common IPC allocation (per POSIX ipcget pattern) ---------------------
constexpr uint32_t kAllocBit = 0100000U;

void ipc_init_perm(IpcPerm& perm, key_t key, int flag) noexcept {
    perm = {};
    perm.mode = kAllocBit | (static_cast<uint32_t>(flag) & 0777U);
    perm.key = key;
    // Single-user model: uid/gid = 0
}

// -- Shared Memory --------------------------------------------------------

struct ShmSegment {
    bool in_use;
    ShmIdDs ds;
    uint8_t data[kMaxShmSize];
};

ShmSegment g_shm[kMaxShmSegments]{};

// -- Semaphores -----------------------------------------------------------

struct SemSet {
    bool in_use;
    SemIdDs ds;
    int16_t vals[kMaxSemsPerSet];
};

SemSet g_sem[kMaxSemSets]{};

// -- Message Queues -------------------------------------------------------

struct MsgEntry {
    int32_t mtype;
    uint32_t msize;
    uint8_t mtext[kMaxMsgSize];
};

struct MsgQueue {
    bool in_use;
    MsgIdDs ds;
    MsgEntry msgs[8]; // up to 8 messages buffered
    uint32_t head;
    uint32_t count;
};

MsgQueue g_msg[kMaxMsgQueues]{};

} // namespace

// == Shared Memory ========================================================

int sys_shmget(key_t key, uint32_t size, int shmflg) noexcept {
    if (size > kMaxShmSize) return -22; // EINVAL

    // Search for existing
    if (key != IPC_PRIVATE) {
        for (uint32_t i = 0U; i < kMaxShmSegments; ++i) {
            if (g_shm[i].in_use && g_shm[i].ds.shm_perm.key == key) {
                if ((shmflg & (IPC_CREAT | IPC_EXCL)) == (IPC_CREAT | IPC_EXCL)) {
                    return -17; // EEXIST
                }
                return static_cast<int>(i);
            }
        }
        if ((shmflg & IPC_CREAT) == 0) return -2; // ENOENT
    }

    // Allocate new
    for (uint32_t i = 0U; i < kMaxShmSegments; ++i) {
        if (!g_shm[i].in_use) {
            g_shm[i].in_use = true;
            g_shm[i].ds = {};
            ipc_init_perm(g_shm[i].ds.shm_perm, key, shmflg);
            g_shm[i].ds.shm_segsz = size;
            for (uint32_t j = 0U; j < kMaxShmSize; ++j) g_shm[i].data[j] = 0U;
            return static_cast<int>(i);
        }
    }
    return -28; // ENOSPC
}

int sys_shmat(int shmid, uint32_t /*shmaddr*/, int /*shmflg*/,
              uint32_t* result_addr) noexcept {
    if (shmid < 0 || static_cast<uint32_t>(shmid) >= kMaxShmSegments) return -22;
    if (!g_shm[shmid].in_use) return -22;
    // Return kernel address directly (flat memory model, ring3 can access)
    *result_addr = reinterpret_cast<uint32_t>(g_shm[shmid].data);
    ++g_shm[shmid].ds.shm_nattch;
    return 0;
}

int sys_shmdt(uint32_t shmaddr) noexcept {
    // Find segment by data pointer
    for (auto& seg : g_shm) {
        if (seg.in_use && reinterpret_cast<uint32_t>(seg.data) == shmaddr) {
            if (seg.ds.shm_nattch > 0U) --seg.ds.shm_nattch;
            return 0;
        }
    }
    return -22; // EINVAL
}

int sys_shmctl(int shmid, int cmd, ShmIdDs* buf) noexcept {
    if (shmid < 0 || static_cast<uint32_t>(shmid) >= kMaxShmSegments) return -22;
    if (!g_shm[shmid].in_use) return -22;

    switch (cmd) {
    case IPC_STAT:
        if (buf != nullptr) *buf = g_shm[shmid].ds;
        return 0;
    case IPC_SET:
        if (buf != nullptr) {
            g_shm[shmid].ds.shm_perm.uid = buf->shm_perm.uid;
            g_shm[shmid].ds.shm_perm.gid = buf->shm_perm.gid;
            g_shm[shmid].ds.shm_perm.mode = (g_shm[shmid].ds.shm_perm.mode & ~0777U) |
                                              (buf->shm_perm.mode & 0777U);
        }
        return 0;
    case IPC_RMID:
        if (g_shm[shmid].ds.shm_nattch == 0U) {
            g_shm[shmid].in_use = false;
        } else {
            g_shm[shmid].ds.shm_perm.mode &= ~kAllocBit; // mark for deletion
        }
        return 0;
    default:
        return -22;
    }
}

// == Semaphores ===========================================================

int sys_semget(key_t key, int nsems, int semflg) noexcept {
    if (nsems < 0 || static_cast<uint32_t>(nsems) > kMaxSemsPerSet) return -22;

    if (key != IPC_PRIVATE) {
        for (uint32_t i = 0U; i < kMaxSemSets; ++i) {
            if (g_sem[i].in_use && g_sem[i].ds.sem_perm.key == key) {
                if ((semflg & (IPC_CREAT | IPC_EXCL)) == (IPC_CREAT | IPC_EXCL)) {
                    return -17;
                }
                return static_cast<int>(i);
            }
        }
        if ((semflg & IPC_CREAT) == 0) return -2;
    }

    for (uint32_t i = 0U; i < kMaxSemSets; ++i) {
        if (!g_sem[i].in_use) {
            g_sem[i].in_use = true;
            g_sem[i].ds = {};
            ipc_init_perm(g_sem[i].ds.sem_perm, key, semflg);
            g_sem[i].ds.sem_nsems = static_cast<uint32_t>(nsems);
            for (auto& v : g_sem[i].vals) v = 0;
            return static_cast<int>(i);
        }
    }
    return -28;
}

int sys_semop(int semid, const SemBuf* sops, uint32_t nsops) noexcept {
    if (semid < 0 || static_cast<uint32_t>(semid) >= kMaxSemSets) return -22;
    if (!g_sem[semid].in_use || sops == nullptr) return -22;

    auto& set = g_sem[semid];

    // Check all operations can proceed (no partial apply)
    for (uint32_t i = 0U; i < nsops; ++i) {
        if (sops[i].sem_num >= set.ds.sem_nsems) return -22;
        const int16_t val = set.vals[sops[i].sem_num];
        if (sops[i].sem_op < 0 && val < -sops[i].sem_op) {
            if (sops[i].sem_flg & static_cast<int16_t>(IPC_NOWAIT)) return -11; // EAGAIN
            return -11; // Would block (no sleep support yet)
        }
        if (sops[i].sem_op == 0 && val != 0) {
            if (sops[i].sem_flg & static_cast<int16_t>(IPC_NOWAIT)) return -11;
            return -11;
        }
    }

    // Apply
    for (uint32_t i = 0U; i < nsops; ++i) {
        set.vals[sops[i].sem_num] = static_cast<int16_t>(
            set.vals[sops[i].sem_num] + sops[i].sem_op);
    }
    return 0;
}

int sys_semctl(int semid, int semnum, int cmd, int val) noexcept {
    if (semid < 0 || static_cast<uint32_t>(semid) >= kMaxSemSets) return -22;
    if (!g_sem[semid].in_use) return -22;

    auto& set = g_sem[semid];

    switch (cmd) {
    case IPC_RMID:
        set.in_use = false;
        return 0;
    case IPC_STAT:
        return 0; // Would need buf pointer; simplified
    case SETVAL:
        if (semnum < 0 || static_cast<uint32_t>(semnum) >= set.ds.sem_nsems) return -22;
        set.vals[semnum] = static_cast<int16_t>(val);
        return 0;
    case GETVAL:
        if (semnum < 0 || static_cast<uint32_t>(semnum) >= set.ds.sem_nsems) return -22;
        return static_cast<int>(set.vals[semnum]);
    default:
        return -22;
    }
}

// == Message Queues =======================================================

int sys_msgget(key_t key, int msgflg) noexcept {
    if (key != IPC_PRIVATE) {
        for (uint32_t i = 0U; i < kMaxMsgQueues; ++i) {
            if (g_msg[i].in_use && g_msg[i].ds.msg_perm.key == key) {
                if ((msgflg & (IPC_CREAT | IPC_EXCL)) == (IPC_CREAT | IPC_EXCL)) {
                    return -17;
                }
                return static_cast<int>(i);
            }
        }
        if ((msgflg & IPC_CREAT) == 0) return -2;
    }

    for (uint32_t i = 0U; i < kMaxMsgQueues; ++i) {
        if (!g_msg[i].in_use) {
            g_msg[i] = {};
            g_msg[i].in_use = true;
            ipc_init_perm(g_msg[i].ds.msg_perm, key, msgflg);
            g_msg[i].ds.msg_qbytes = kMaxMsgQueueBytes;
            return static_cast<int>(i);
        }
    }
    return -28;
}

int sys_msgsnd(int msqid, const void* msgp, uint32_t msgsz, int msgflg) noexcept {
    if (msqid < 0 || static_cast<uint32_t>(msqid) >= kMaxMsgQueues) return -22;
    if (!g_msg[msqid].in_use || msgp == nullptr) return -22;
    if (msgsz > kMaxMsgSize) return -22;

    auto& q = g_msg[msqid];
    if (q.count >= 8U) {
        if (msgflg & IPC_NOWAIT) return -11; // EAGAIN
        return -11; // Would block
    }

    // msgp layout: int32_t mtype, then mtext[msgsz]
    const auto* raw = static_cast<const uint8_t*>(msgp);
    const auto mtype = *reinterpret_cast<const int32_t*>(raw);
    if (mtype <= 0) return -22;

    const uint32_t slot = (q.head + q.count) % 8U;
    q.msgs[slot].mtype = mtype;
    q.msgs[slot].msize = msgsz;
    for (uint32_t i = 0U; i < msgsz; ++i) {
        q.msgs[slot].mtext[i] = raw[4U + i];
    }
    ++q.count;
    ++q.ds.msg_qnum;
    return 0;
}

int sys_msgrcv(int msqid, void* msgp, uint32_t msgsz,
               int32_t msgtyp, int msgflg) noexcept {
    if (msqid < 0 || static_cast<uint32_t>(msqid) >= kMaxMsgQueues) return -22;
    if (!g_msg[msqid].in_use || msgp == nullptr) return -22;

    auto& q = g_msg[msqid];

    // Find matching message
    uint32_t found = 0xFFFFFFFFU;
    for (uint32_t i = 0U; i < q.count; ++i) {
        const uint32_t idx = (q.head + i) % 8U;
        if (msgtyp == 0 ||
            (msgtyp > 0 && q.msgs[idx].mtype == msgtyp) ||
            (msgtyp < 0 && q.msgs[idx].mtype <= -msgtyp)) {
            found = i;
            break;
        }
    }

    if (found == 0xFFFFFFFFU) {
        if (msgflg & IPC_NOWAIT) return -42; // ENOMSG
        return -42;
    }

    const uint32_t idx = (q.head + found) % 8U;
    uint32_t copy_len = q.msgs[idx].msize;
    if (copy_len > msgsz) copy_len = msgsz;

    auto* out = static_cast<uint8_t*>(msgp);
    *reinterpret_cast<int32_t*>(out) = q.msgs[idx].mtype;
    for (uint32_t i = 0U; i < copy_len; ++i) {
        out[4U + i] = q.msgs[idx].mtext[i];
    }

    // Remove message (shift remaining)
    if (found == 0U) {
        q.head = (q.head + 1U) % 8U;
    } else {
        // Compact by shifting
        for (uint32_t i = found; i + 1U < q.count; ++i) {
            const uint32_t dst = (q.head + i) % 8U;
            const uint32_t src = (q.head + i + 1U) % 8U;
            q.msgs[dst] = q.msgs[src];
        }
    }
    --q.count;
    --q.ds.msg_qnum;
    return static_cast<int>(copy_len);
}

int sys_msgctl(int msqid, int cmd, MsgIdDs* buf) noexcept {
    if (msqid < 0 || static_cast<uint32_t>(msqid) >= kMaxMsgQueues) return -22;
    if (!g_msg[msqid].in_use) return -22;

    switch (cmd) {
    case IPC_STAT:
        if (buf != nullptr) *buf = g_msg[msqid].ds;
        return 0;
    case IPC_SET:
        if (buf != nullptr) {
            g_msg[msqid].ds.msg_perm.uid = buf->msg_perm.uid;
            g_msg[msqid].ds.msg_perm.gid = buf->msg_perm.gid;
            g_msg[msqid].ds.msg_perm.mode = (g_msg[msqid].ds.msg_perm.mode & ~0777U) |
                                              (buf->msg_perm.mode & 0777U);
        }
        return 0;
    case IPC_RMID:
        g_msg[msqid].in_use = false;
        return 0;
    default:
        return -22;
    }
}

} // namespace xinim::i486::ipc
