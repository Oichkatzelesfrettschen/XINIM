/**
 * @file proc_protocol.hpp
 * @brief Process Manager IPC Protocol Structures
 *
 * This header defines the request and response structures used for
 * communication between the kernel syscall dispatcher and the Process Manager.
 *
 * @ingroup ipc
 */

#ifndef XINIM_IPC_PROC_PROTOCOL_HPP
#define XINIM_IPC_PROC_PROTOCOL_HPP

#include <cstdint>
#include <cstring>
#include "message_types.h"

namespace xinim::ipc {

/* ========================================================================
 * Process Lifecycle Protocol Structures
 * ======================================================================== */

/**
 * @brief Request to fork a process
 * Message type: PROC_FORK
 */
struct ProcForkRequest {
    int32_t parent_pid;      /**< PID of parent process */
    uint64_t parent_rsp;     /**< Parent stack pointer */
    uint64_t parent_rip;     /**< Parent instruction pointer */
    uint64_t parent_rflags;  /**< Parent RFLAGS register */

    ProcForkRequest() : parent_pid(0), parent_rsp(0), parent_rip(0), parent_rflags(0) {}
} __attribute__((packed));

/**
 * @brief Response to fork request
 * Message type: PROC_REPLY
 */
struct ProcForkResponse {
    int32_t child_pid;       /**< Child PID in parent (>= 0), 0 in child, -1 on error */
    int32_t error;           /**< Error code */

    ProcForkResponse() : child_pid(-1), error(IPC_ENOSYS) {}
    ProcForkResponse(int32_t pid, int32_t e) : child_pid(pid), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to execute a program
 * Message type: PROC_EXEC
 */
struct ProcExecRequest {
    char path[256];          /**< Executable path */
    int32_t argc;            /**< Argument count */
    int32_t envc;            /**< Environment variable count */
    int32_t caller_pid;      /**< PID of calling process */
    // Note: argv and envp are transferred via shared memory or follow-up messages

    ProcExecRequest() : argc(0), envc(0), caller_pid(0) {
        path[0] = '\0';
    }
} __attribute__((packed));

/**
 * @brief Response to exec request
 * Message type: PROC_REPLY
 *
 * Note: On success, exec does not return (process image is replaced).
 * This response is only sent on error.
 */
struct ProcExecResponse {
    int32_t result;          /**< -1 on error (exec never returns on success) */
    int32_t error;           /**< Error code */

    ProcExecResponse() : result(-1), error(IPC_ENOSYS) {}
    ProcExecResponse(int32_t e) : result(-1), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to exit a process
 * Message type: PROC_EXIT
 */
struct ProcExitRequest {
    int32_t pid;             /**< PID of exiting process */
    int32_t exit_code;       /**< Exit status code */

    ProcExitRequest() : pid(0), exit_code(0) {}
    ProcExitRequest(int32_t p, int32_t code) : pid(p), exit_code(code) {}
} __attribute__((packed));

/**
 * @brief Request to wait for child process
 * Message type: PROC_WAIT
 */
struct ProcWaitRequest {
    int32_t parent_pid;      /**< PID of parent process */
    int32_t target_pid;      /**< PID to wait for (-1 = any child) */
    int32_t options;         /**< Wait options (WNOHANG, WUNTRACED, etc.) */

    ProcWaitRequest() : parent_pid(0), target_pid(-1), options(0) {}
} __attribute__((packed));

/**
 * @brief Response to wait request
 * Message type: PROC_REPLY
 */
struct ProcWaitResponse {
    int32_t child_pid;       /**< PID of child that exited (>= 0 on success, -1 on error) */
    int32_t exit_status;     /**< Exit status of child */
    int32_t error;           /**< Error code */

    ProcWaitResponse() : child_pid(-1), exit_status(0), error(IPC_ENOSYS) {}
} __attribute__((packed));

/**
 * @brief Request to send signal to process
 * Message type: PROC_KILL
 */
struct ProcKillRequest {
    int32_t target_pid;      /**< Target PID (>0 = process, 0 = group, -1 = all, <-1 = -pgid) */
    int32_t signal;          /**< Signal number */
    int32_t sender_pid;      /**< PID of sending process */

    ProcKillRequest() : target_pid(0), signal(0), sender_pid(0) {}
    ProcKillRequest(int32_t tpid, int32_t sig, int32_t spid)
        : target_pid(tpid), signal(sig), sender_pid(spid) {}
} __attribute__((packed));

/**
 * @brief Request to get process ID
 * Message type: PROC_GETPID
 */
struct ProcGetpidRequest {
    int32_t caller_pid;      /**< PID of calling process (for validation) */

    ProcGetpidRequest() : caller_pid(0) {}
    ProcGetpidRequest(int32_t pid) : caller_pid(pid) {}
} __attribute__((packed));

/**
 * @brief Response to getpid request
 * Message type: PROC_REPLY
 */
struct ProcGetpidResponse {
    int32_t pid;             /**< Process ID */
    int32_t error;           /**< Error code */

    ProcGetpidResponse() : pid(-1), error(IPC_ENOSYS) {}
    ProcGetpidResponse(int32_t p, int32_t e) : pid(p), error(e) {}
} __attribute__((packed));

/**
 * @brief Request to get parent process ID
 * Message type: PROC_GETPPID
 */
struct ProcGetppidRequest {
    int32_t caller_pid;      /**< PID of calling process */

    ProcGetppidRequest() : caller_pid(0) {}
} __attribute__((packed));

/**
 * @brief Response to getppid request
 * Message type: PROC_REPLY
 */
struct ProcGetppidResponse {
    int32_t ppid;            /**< Parent process ID */
    int32_t error;           /**< Error code */

    ProcGetppidResponse() : ppid(-1), error(IPC_ENOSYS) {}
    ProcGetppidResponse(int32_t p, int32_t e) : ppid(p), error(e) {}
} __attribute__((packed));

/* ========================================================================
 * Signal Handling Protocol Structures
 * ======================================================================== */

/**
 * @brief Signal action flags
 */
enum class SigActionFlags : uint32_t {
    SA_NOCLDSTOP = 0x00000001,  /**< Don't send SIGCHLD when child stops */
    SA_NOCLDWAIT = 0x00000002,  /**< Don't create zombie on child exit */
    SA_SIGINFO   = 0x00000004,  /**< Invoke signal handler with 3 args */
    SA_ONSTACK   = 0x08000000,  /**< Use signal stack */
    SA_RESTART   = 0x10000000,  /**< Restart syscall on signal return */
    SA_NODEFER   = 0x40000000,  /**< Don't mask signal in handler */
    SA_RESETHAND = 0x80000000,  /**< Reset to SIG_DFL on entry */
};

/**
 * @brief Request to set signal action
 * Message type: PROC_SIGACTION
 */
struct ProcSigactionRequest {
    int32_t caller_pid;      /**< PID of calling process */
    int32_t signal;          /**< Signal number */
    uint64_t handler;        /**< Signal handler address (userspace) */
    uint64_t sigaction_flags; /**< Signal action flags */
    uint64_t sa_mask;        /**< Signal mask during handler */

    ProcSigactionRequest() : caller_pid(0), signal(0), handler(0),
                             sigaction_flags(0), sa_mask(0) {}
} __attribute__((packed));

/**
 * @brief Response to sigaction request
 * Message type: PROC_REPLY
 */
struct ProcSigactionResponse {
    uint64_t old_handler;    /**< Previous signal handler */
    int32_t result;          /**< 0 on success, -1 on error */
    int32_t error;           /**< Error code */

    ProcSigactionResponse() : old_handler(0), result(-1), error(IPC_ENOSYS) {}
} __attribute__((packed));

/* ========================================================================
 * Process Attributes Protocol Structures
 * ======================================================================== */

/**
 * @brief Request to get/set user ID
 * Message types: PROC_GETUID, PROC_GETEUID, PROC_SETUID
 */
struct ProcUidRequest {
    int32_t caller_pid;      /**< PID of calling process */
    int32_t new_uid;         /**< New UID (for setuid, -1 for getuid) */

    ProcUidRequest() : caller_pid(0), new_uid(-1) {}
} __attribute__((packed));

/**
 * @brief Response to uid request
 * Message type: PROC_REPLY
 */
struct ProcUidResponse {
    int32_t uid;             /**< User ID */
    int32_t result;          /**< 0 on success (for setuid), uid (for getuid) */
    int32_t error;           /**< Error code */

    ProcUidResponse() : uid(-1), result(-1), error(IPC_ENOSYS) {}
} __attribute__((packed));

/**
 * @brief Request to get/set group ID
 * Message types: PROC_GETGID, PROC_GETEGID, PROC_SETGID
 */
struct ProcGidRequest {
    int32_t caller_pid;      /**< PID of calling process */
    int32_t new_gid;         /**< New GID (for setgid, -1 for getgid) */

    ProcGidRequest() : caller_pid(0), new_gid(-1) {}
} __attribute__((packed));

/**
 * @brief Response to gid request
 * Message type: PROC_REPLY
 */
struct ProcGidResponse {
    int32_t gid;             /**< Group ID */
    int32_t result;          /**< 0 on success (for setgid), gid (for getgid) */
    int32_t error;           /**< Error code */

    ProcGidResponse() : gid(-1), result(-1), error(IPC_ENOSYS) {}
} __attribute__((packed));

/* ========================================================================
 * Generic Response Structure
 * ======================================================================== */

/**
 * @brief Generic process manager response for simple operations
 * Message type: PROC_REPLY
 */
struct ProcGenericResponse {
    int32_t result;          /**< Operation result */
    int32_t error;           /**< Error code */
    int64_t extra_data;      /**< Optional extra return value */

    ProcGenericResponse() : result(-1), error(IPC_ENOSYS), extra_data(0) {}
    ProcGenericResponse(int32_t r, int32_t e, int64_t x = 0)
        : result(r), error(e), extra_data(x) {}
} __attribute__((packed));

/* ========================================================================
 * Helper Functions
 * ======================================================================== */

/**
 * @brief Check if a message type is a process manager message
 */
inline constexpr bool is_proc_message(int msg_type) noexcept {
    return msg_type >= 200 && msg_type < 300;
}

/**
 * @brief Get human-readable name for process manager message type
 */
inline const char* proc_message_name(int msg_type) noexcept {
    switch (msg_type) {
        case PROC_FORK: return "PROC_FORK";
        case PROC_EXEC: return "PROC_EXEC";
        case PROC_EXIT: return "PROC_EXIT";
        case PROC_WAIT: return "PROC_WAIT";
        case PROC_KILL: return "PROC_KILL";
        case PROC_GETPID: return "PROC_GETPID";
        case PROC_GETPPID: return "PROC_GETPPID";
        case PROC_SIGACTION: return "PROC_SIGACTION";
        case PROC_GETUID: return "PROC_GETUID";
        case PROC_GETEUID: return "PROC_GETEUID";
        case PROC_SETUID: return "PROC_SETUID";
        case PROC_GETGID: return "PROC_GETGID";
        case PROC_GETEGID: return "PROC_GETEGID";
        case PROC_SETGID: return "PROC_SETGID";
        case PROC_REPLY: return "PROC_REPLY";
        case PROC_ERROR: return "PROC_ERROR";
        default: return "UNKNOWN_PROC_MESSAGE";
    }
}

} // namespace xinim::ipc

#endif /* XINIM_IPC_PROC_PROTOCOL_HPP */
