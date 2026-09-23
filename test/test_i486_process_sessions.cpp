#include "../src/kernel/i486/process.hpp"
#include "i486_check.hpp"

namespace {
int foreground_group = 0;
}
namespace xinim::kernel::bootfs {
int foreground_pgrp() noexcept { return foreground_group; }
void set_foreground_pgrp(int pgid) noexcept { foreground_group = pgid; }
}
namespace xinim::i486::ring3 {
Process g_processes[kMaxProcesses]{};
void send_signal_to_process(Process* target, uint32_t signum) noexcept {
    target->signals.pending |= 1U << signum;
}
}

int main() {
    using namespace xinim::i486::ring3;
    for (size_t index = 0U; index < kMaxProcesses; ++index) {
        Process& process = g_processes[index];
        process.in_use = true;
        process.pid = static_cast<uint32_t>(index + 1U);
        process.state = ProcessState::Runnable;
        process.session_id = process.pid;
        process.pgid = process.pid;
    }
    Process& leader = g_processes[0];
    Process& child = g_processes[1];
    Process& peer = g_processes[2];
    Process& other_session = g_processes[3];
    Process& background = g_processes[4];
    initialize_supervised_session(leader, true);
    CHECK(owns_controlling_terminal(leader));
    CHECK(foreground_group == 1);
    CHECK(create_process_session(leader) == kErrnoPerm);
    CHECK(acquire_controlling_terminal(other_session) == kErrnoPerm);
    child.ppid = leader.pid;
    inherit_process_session(child, leader);
    CHECK(child.session_id == 1U && child.pgid == 1U);
    CHECK(owns_controlling_terminal(child));
    CHECK(acquire_controlling_terminal(child) == kErrnoPerm);
    CHECK(query_process_session(child, 0U) == 1U);
    CHECK(query_process_session(other_session, child.pid) == 1U);
    CHECK(query_process_session(child, 99U) == kErrnoSrch);

    // Ordinary child exit releases its reference without hanging up its parent.
    release_controlling_terminal(child);
    CHECK(leader.signals.pending == 0U);
    CHECK(owns_controlling_terminal(leader));
    CHECK(foreground_group == 1);
    inherit_process_session(child, leader);

    CHECK(set_process_group(leader, child.pid, 0U) == 0U);
    CHECK(child.pgid == child.pid && child.session_id == leader.session_id);
    CHECK(query_process_session(child, 0U) == 1U);
    CHECK(set_process_group(leader, 0U, 0U) == kErrnoPerm);
    CHECK(set_process_group(leader, UINT32_MAX, 0U) == kErrnoInvalid);
    CHECK(set_process_group(leader, child.pid, UINT32_MAX) == kErrnoInvalid);
    CHECK(set_process_group(leader, 99U, 0U) == kErrnoSrch);
    CHECK(set_process_group(child, leader.pid, 0U) == kErrnoSrch);
    CHECK(set_process_group(leader, child.pid, 99U) == kErrnoPerm);
    CHECK(set_process_group(leader, child.pid, other_session.pgid) == kErrnoPerm);
    child.executed_since_fork = true;
    CHECK(set_process_group(leader, child.pid, 0U) == kErrnoAcces);
    child.executed_since_fork = false;
    CHECK(create_process_session(child) == kErrnoPerm);

    peer.ppid = leader.pid;
    inherit_process_session(peer, leader);
    CHECK(set_process_group(leader, peer.pid, child.pgid) == 0U);
    CHECK(set_terminal_foreground(leader, 99) == kErrnoPerm);
    CHECK(set_terminal_foreground(leader, -1) == kErrnoInvalid);
    CHECK(set_terminal_foreground(other_session, 4) == kErrnoNoTTY);
    CHECK(set_terminal_foreground(leader, static_cast<int32_t>(child.pgid)) == 0U);
    CHECK(foreground_group == 2);
    CHECK(set_terminal_foreground(leader, 1) == kErrnoIntr);
    CHECK((leader.signals.pending & (1U << kSigTtou)) != 0U);
    leader.signals.blocked = 1U << kSigTtou;
    CHECK(set_terminal_foreground(leader, 1) == 0U);
    CHECK(set_terminal_foreground(leader, 2) == 0U);
    leader.signals.blocked = 0U;
    leader.signals.handlers[kSigTtou].handler = kSigIgn;
    CHECK(set_terminal_foreground(leader, 1) == 0U);
    CHECK(set_terminal_foreground(leader, 2) == 0U);

    background.ppid = leader.pid;
    inherit_process_session(background, leader);
    background.pgid = background.pid;
    other_session.pgid = child.pgid; // Conflicting fixture group proves session scoping.
    release_controlling_terminal(leader);
    CHECK((child.signals.pending & (1U << kSigHup)) != 0U);
    CHECK((peer.signals.pending & (1U << kSigHup)) != 0U);
    CHECK(other_session.signals.pending == 0U);
    CHECK(background.signals.pending == 0U);
    CHECK(!owns_controlling_terminal(leader));
    CHECK(!child.has_controlling_terminal && !peer.has_controlling_terminal);
    CHECK(!background.has_controlling_terminal);
    CHECK(foreground_group == 0);

    // A former session member may create a new session; terminal ownership
    // follows the session leader and survives unrelated supervisor restarts.
    CHECK(set_process_group(background, 0U, child.pgid) == 0U);
    CHECK(create_process_session(background) == background.pid);
    CHECK(background.session_id == background.pid && background.pgid == background.pid);
    CHECK(!background.has_controlling_terminal);
    CHECK(acquire_controlling_terminal(background) == 0U);
    CHECK(set_process_group(leader, background.pid, 0U) == kErrnoPerm);
    initialize_supervised_session(other_session, false);
    CHECK(owns_controlling_terminal(background));
    release_controlling_terminal(background, true);
    initialize_supervised_session(leader, true);
    CHECK(owns_controlling_terminal(leader));
    CHECK(foreground_group == 1);
    CHECK(leader.session_id == leader.pid && leader.pgid == leader.pid);
    CHECK(!child.has_controlling_terminal && !peer.has_controlling_terminal);
    CHECK(leader.session_generation != child.session_generation);
    CHECK(set_terminal_foreground(leader, static_cast<int32_t>(child.pgid)) == kErrnoPerm);

    Process& new_member = g_processes[5];
    new_member.ppid = leader.pid;
    inherit_process_session(new_member, leader);
    CHECK(!new_member.executed_since_fork);
    CHECK(create_process_session(new_member) == new_member.pid);
    CHECK(!new_member.has_controlling_terminal);
    CHECK(owns_controlling_terminal(leader));
    CHECK(acquire_controlling_terminal(new_member) == kErrnoPerm);
    std::puts("i486 session, group, terminal ownership, and hangup checks passed");
    return 0;
}
