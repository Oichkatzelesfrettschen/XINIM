/**
 * @file test_recovery_dag.cpp
 * @brief Tests for RecoveryDag: DAG construction, cycle detection,
 *        topological sort, crash notification and restart ordering.
 */

#include "../src/kernel/recovery/recovery_dag.hpp"
#include <cstdio>
#include <cstdlib>

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        g_fail++; \
    } else { g_pass++; } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s == %d, expected %d\n", \
            __FILE__, __LINE__, #a, (int)(a), (int)(b)); \
        g_fail++; \
    } else { g_pass++; } \
} while(0)

using namespace xinim::kernel::recovery;

static void test_add_and_find() {
    RecoveryDag dag;
    int vfs = dag.add_service("vfs", RestartPolicy::RESTART);
    int pm  = dag.add_service("pm", RestartPolicy::RESTART);
    int mm  = dag.add_service("mm", RestartPolicy::PANIC);

    ASSERT_EQ(vfs, 0);
    ASSERT_EQ(pm, 1);
    ASSERT_EQ(mm, 2);
    ASSERT_EQ(dag.service_count(), 3u);

    ASSERT_EQ(dag.find_service("vfs"), 0);
    ASSERT_EQ(dag.find_service("pm"), 1);
    ASSERT_EQ(dag.find_service("mm"), 2);
    ASSERT_EQ(dag.find_service("nonexistent"), -1);
}

static void test_dependencies() {
    RecoveryDag dag;
    int vfs = dag.add_service("vfs", RestartPolicy::RESTART);
    int pm  = dag.add_service("pm", RestartPolicy::RESTART);
    int app = dag.add_service("app", RestartPolicy::RESTART);

    // app depends on vfs and pm
    ASSERT(dag.add_dependency(app, vfs));
    ASSERT(dag.add_dependency(app, pm));

    // Duplicate dependency is idempotent
    ASSERT(dag.add_dependency(app, vfs));

    // Verify via service node
    const ServiceNode* node = dag.get_service(app);
    ASSERT(node != nullptr);
    ASSERT_EQ(node->ndeps, 2);
}

static void test_cycle_detection() {
    RecoveryDag dag;
    int a = dag.add_service("a", RestartPolicy::RESTART);
    int b = dag.add_service("b", RestartPolicy::RESTART);
    int c = dag.add_service("c", RestartPolicy::RESTART);

    // a -> b -> c (a depends on b, b depends on c)
    ASSERT(dag.add_dependency(a, b));
    ASSERT(dag.add_dependency(b, c));

    // Adding c -> a would create a cycle
    ASSERT(!dag.add_dependency(c, a));

    // Self-dependency is rejected
    ASSERT(!dag.add_dependency(a, a));

    // DAG should still be valid
    ASSERT(dag.validate());
}

static void test_validate_valid_dag() {
    RecoveryDag dag;
    int vfs = dag.add_service("vfs", RestartPolicy::RESTART);
    int pm  = dag.add_service("pm", RestartPolicy::RESTART);
    int mm  = dag.add_service("mm", RestartPolicy::PANIC);
    int app = dag.add_service("app", RestartPolicy::RESTART);

    // app depends on vfs and pm
    dag.add_dependency(app, vfs);
    dag.add_dependency(app, pm);
    // vfs depends on mm
    dag.add_dependency(vfs, mm);

    ASSERT(dag.validate());
}

static void test_crash_restart_single() {
    RecoveryDag dag;
    int vfs = dag.add_service("vfs", RestartPolicy::RESTART);
    dag.set_pid(vfs, 100);
    dag.set_running(vfs);

    int order[32];
    int count = dag.notify_crash(vfs, order, 32);

    ASSERT_EQ(count, 1);
    ASSERT_EQ(order[0], vfs);
    ASSERT_EQ(dag.get_service(vfs)->state, ServiceState::CRASHED);
}

static void test_crash_panic() {
    RecoveryDag dag;
    int mm = dag.add_service("mm", RestartPolicy::PANIC);
    dag.set_pid(mm, 200);
    dag.set_running(mm);

    int order[32];
    int count = dag.notify_crash(mm, order, 32);

    // -1 signals kernel panic
    ASSERT_EQ(count, -1);
}

static void test_crash_ignore() {
    RecoveryDag dag;
    int log = dag.add_service("log", RestartPolicy::IGNORE);
    dag.set_pid(log, 300);
    dag.set_running(log);

    int order[32];
    int count = dag.notify_crash(log, order, 32);

    ASSERT_EQ(count, 0);
}

static void test_crash_kill_deps_topo_order() {
    // DAG: mm -> vfs -> app1, app2
    //      (app1 and app2 depend on vfs, vfs depends on mm)
    RecoveryDag dag;
    int mm   = dag.add_service("mm",   RestartPolicy::KILL_DEPS);
    int vfs  = dag.add_service("vfs",  RestartPolicy::KILL_DEPS);
    int app1 = dag.add_service("app1", RestartPolicy::RESTART);
    int app2 = dag.add_service("app2", RestartPolicy::RESTART);

    dag.add_dependency(vfs, mm);
    dag.add_dependency(app1, vfs);
    dag.add_dependency(app2, vfs);

    dag.set_running(mm);
    dag.set_running(vfs);
    dag.set_running(app1);
    dag.set_running(app2);

    // Crash mm -- should restart mm, then vfs, then app1+app2
    int order[32];
    int count = dag.notify_crash(mm, order, 32);

    ASSERT_EQ(count, 4);

    // mm must come first (it's the root with no deps among affected)
    ASSERT_EQ(order[0], mm);
    // vfs must come before app1 and app2
    ASSERT_EQ(order[1], vfs);
    // app1 and app2 can be in either order
    ASSERT((order[2] == app1 && order[3] == app2) ||
           (order[2] == app2 && order[3] == app1));
}

static void test_find_by_pid() {
    RecoveryDag dag;
    int vfs = dag.add_service("vfs", RestartPolicy::RESTART);
    dag.set_pid(vfs, 42);

    ASSERT_EQ(dag.find_by_pid(42), vfs);
    ASSERT_EQ(dag.find_by_pid(99), -1);
}

static void test_restart_count() {
    RecoveryDag dag;
    int svc = dag.add_service("svc", RestartPolicy::RESTART, 3);
    dag.set_running(svc);

    int order[4];

    dag.notify_crash(svc, order, 4);
    ASSERT_EQ(dag.get_service(svc)->restart_count, 1);

    dag.set_running(svc);
    dag.notify_crash(svc, order, 4);
    ASSERT_EQ(dag.get_service(svc)->restart_count, 2);
}

static void test_restart_limit_stops_restarts() {
    RecoveryDag dag;
    int svc = dag.add_service("svc", RestartPolicy::RESTART, 2);
    dag.set_running(svc);

    int order[4];

    ASSERT_EQ(dag.notify_crash(svc, order, 4), 1);
    dag.set_running(svc);
    ASSERT_EQ(dag.notify_crash(svc, order, 4), 1);
    dag.set_running(svc);
    ASSERT_EQ(dag.notify_crash(svc, order, 4), 0);
    ASSERT_EQ(dag.get_service(svc)->restart_count, 3);
    ASSERT_EQ(dag.get_service(svc)->state, ServiceState::CRASHED);
}

int main() {
    test_add_and_find();
    test_dependencies();
    test_cycle_detection();
    test_validate_valid_dag();
    test_crash_restart_single();
    test_crash_panic();
    test_crash_ignore();
    test_crash_kill_deps_topo_order();
    test_find_by_pid();
    test_restart_count();
    test_restart_limit_stops_restarts();

    std::printf("test_recovery_dag: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
