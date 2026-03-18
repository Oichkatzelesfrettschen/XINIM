#pragma once
// System V IPC: shared memory, semaphores, message queues.
// Cleanroom implementation per POSIX.1 / Single UNIX Specification.

#include <stdint.h>

namespace xinim::i486::ipc {

// -- Common IPC (per POSIX sys/ipc.h) -------------------------------------
using key_t = int32_t;

constexpr key_t IPC_PRIVATE = 0;
constexpr int IPC_CREAT = 01000;
constexpr int IPC_EXCL = 02000;
constexpr int IPC_NOWAIT = 04000;
constexpr int IPC_RMID = 0;
constexpr int IPC_SET = 1;
constexpr int IPC_STAT = 2;

struct IpcPerm {
    uint32_t uid;
    uint32_t gid;
    uint32_t cuid;
    uint32_t cgid;
    uint32_t mode;
    uint32_t seq;
    key_t key;
};

// -- Shared Memory (per POSIX sys/shm.h) ----------------------------------
constexpr uint32_t kMaxShmSegments = 4U;
constexpr uint32_t kMaxShmSize = 65536U; // 64K per segment

constexpr int SHM_RDONLY = 010000;

struct ShmIdDs {
    IpcPerm shm_perm;
    uint32_t shm_segsz;
    uint32_t shm_cpid;
    uint32_t shm_lpid;
    uint32_t shm_nattch;
};

int sys_shmget(key_t key, uint32_t size, int shmflg) noexcept;
int sys_shmat(int shmid, uint32_t shmaddr, int shmflg, uint32_t* result_addr) noexcept;
int sys_shmdt(uint32_t shmaddr) noexcept;
int sys_shmctl(int shmid, int cmd, ShmIdDs* buf) noexcept;

// -- Semaphores (per POSIX sys/sem.h) -------------------------------------
constexpr uint32_t kMaxSemSets = 4U;
constexpr uint32_t kMaxSemsPerSet = 8U;
constexpr int SETVAL = 16;
constexpr int GETVAL = 12;
constexpr int GETALL = 13;
constexpr int SETALL = 17;

struct SemIdDs {
    IpcPerm sem_perm;
    uint32_t sem_nsems;
};

struct SemBuf {
    uint16_t sem_num;
    int16_t sem_op;
    int16_t sem_flg;
};

int sys_semget(key_t key, int nsems, int semflg) noexcept;
int sys_semop(int semid, const SemBuf* sops, uint32_t nsops) noexcept;
int sys_semctl(int semid, int semnum, int cmd, int val) noexcept;

// -- Message Queues (per POSIX sys/msg.h) ---------------------------------
constexpr uint32_t kMaxMsgQueues = 4U;
constexpr uint32_t kMaxMsgSize = 2048U;
constexpr uint32_t kMaxMsgQueueBytes = 8192U;

struct MsgIdDs {
    IpcPerm msg_perm;
    uint32_t msg_qnum;
    uint32_t msg_qbytes;
    uint32_t msg_lspid;
    uint32_t msg_lrpid;
};

int sys_msgget(key_t key, int msgflg) noexcept;
int sys_msgsnd(int msqid, const void* msgp, uint32_t msgsz, int msgflg) noexcept;
int sys_msgrcv(int msqid, void* msgp, uint32_t msgsz, int32_t msgtyp, int msgflg) noexcept;
int sys_msgctl(int msqid, int cmd, MsgIdDs* buf) noexcept;

} // namespace xinim::i486::ipc
