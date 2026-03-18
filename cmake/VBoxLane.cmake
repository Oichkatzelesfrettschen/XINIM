include_guard()

# WHY: VirtualBox provides a deterministic, headless test environment that
# matches real hardware much more closely than QEMU for the i486 lane.
# Using VBoxManage as the control plane allows keyboard injection, serial
# capture, and screenshots without a GUI -- key for CI and PGO collection.
#
# WHAT: Adds the following custom CMake targets for a given lane:
#   vbox_setup_<lane>       -- convert qcow2 -> VDI, register VM
#   vbox_start_<lane>       -- start VM headless
#   vbox_stop_<lane>        -- power off VM
#   vbox_teardown_<lane>    -- unregister + delete VM
#   vbox_status_<lane>      -- print VM power state
#   vbox_test_<lane>        -- run test suite against running VM
#   vbox_screenshot_<lane>  -- take a PNG screenshot
#   vbox_pgo_collect_<lane> -- boot PGO-instrumented kernel, wait for profile
# And one CTest entry:
#   <lane>_vbox_test        -- registered with label "vbox;integration"
#
# HOW: include(VBoxLane) in CMakeLists.txt, then call
#      xinim_add_vbox_targets(i486 <build_dir> <project_root>)

function(xinim_add_vbox_targets lane build_dir project_root)
    set(_py "${project_root}/scripts/vbox_i486.py")
    set(_args
        --build-dir "${build_dir}"
        --project-root "${project_root}"
    )

    # vbox_setup_<lane>
    add_custom_target(vbox_setup_${lane}
        COMMAND python3 "${_py}" ${_args} setup
        COMMENT "VBox: create/register XINIM ${lane} VM"
        USES_TERMINAL
    )
    # vbox_start_<lane>
    add_custom_target(vbox_start_${lane}
        COMMAND python3 "${_py}" ${_args} start
        COMMENT "VBox: start XINIM ${lane} VM headless"
        USES_TERMINAL
    )
    # vbox_stop_<lane>
    add_custom_target(vbox_stop_${lane}
        COMMAND python3 "${_py}" ${_args} stop
        COMMENT "VBox: stop XINIM ${lane} VM"
        USES_TERMINAL
    )
    # vbox_teardown_<lane>
    add_custom_target(vbox_teardown_${lane}
        COMMAND python3 "${_py}" ${_args} teardown
        COMMENT "VBox: teardown XINIM ${lane} VM"
        USES_TERMINAL
    )
    # vbox_status_<lane>
    add_custom_target(vbox_status_${lane}
        COMMAND python3 "${_py}" ${_args} status
        COMMENT "VBox: status of XINIM ${lane} VM"
        USES_TERMINAL
    )
    # vbox_screenshot_<lane>
    add_custom_target(vbox_screenshot_${lane}
        COMMAND python3 "${_py}" ${_args} screenshot
        COMMENT "VBox: screenshot XINIM ${lane} VM"
        USES_TERMINAL
    )
    # vbox_test_<lane> (drives the bash test suite)
    add_custom_target(vbox_test_${lane}
        COMMAND python3 "${_py}" ${_args} test
        COMMENT "VBox: run XINIM ${lane} integration test suite"
        USES_TERMINAL
    )
    # vbox_pgo_collect_<lane> -- boot PGO kernel, wait for XNPGO_END
    add_custom_target(vbox_pgo_collect_${lane}
        COMMAND python3 "${_py}" ${_args} pgo-collect
        COMMENT "VBox: collect PGO profile from XINIM ${lane}"
        USES_TERMINAL
    )

    # CTest integration: run the bash test suite
    # Skip when VBoxManage is absent (exit code 77 -> SKIP in ctest).
    set(_test_name "${lane}_vbox_test")
    add_test(
        NAME "${_test_name}"
        COMMAND bash "${project_root}/scripts/test_i486_vbox.sh"
        WORKING_DIRECTORY "${project_root}"
    )
    set_tests_properties("${_test_name}" PROPERTIES
        ENVIRONMENT "BUILD_DIR=${build_dir}"
        LABELS "vbox;integration"
        SKIP_RETURN_CODE 77
        TIMEOUT 300
    )
endfunction()
