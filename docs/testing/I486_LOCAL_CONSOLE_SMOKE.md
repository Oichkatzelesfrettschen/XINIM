# i486 Local Console Smoke

Date: 2026-03-09

This checklist validates the QEMU 486 local-console path for the i486 lane.
It complements the automated COM2 shell tests by proving the guest is usable
from the visible VGA window with keyboard input.

## Goal

Confirm that the i486 image:

- paints boot and shell output to VGA text mode
- accepts keyboard input at the local console
- keeps COM2 available for debugging and automation

## Prerequisites

- `qemu-system-i386` 10.2.0 or compatible
- built i486 image at `build/i486/Debug/images/i486/xinim-i486dx.iso`

## Launch

Run:

```bash
scripts/qemu_i486.sh \
  --boot-image build/i486/Debug/images/i486/xinim-i486dx.iso \
  --cpu 486 \
  --machine pc-i440fx-10.2 \
  --vga std
```

Notes:

- Omit `--headless` so QEMU opens the local display.
- COM2 remains available on `localhost:4555` for parallel debugging.

## Expected Visual Result

You should see:

- early boot text in the VGA window
- `Launching supervised Ring 3 init service`
- a live `mksh` prompt

## Keyboard Checks

Type these in the QEMU window:

```sh
echo local-console
pwd
cat /etc/motd
heapprobe
exit
```

Expected results:

- text appears in the VGA window as you type
- command output appears on the VGA window
- `heapprobe` reports success
- `exit` respawns the supervised init service instead of dropping into rescue

## Parallel COM2 Check

Optionally, in another terminal:

```bash
telnet 127.0.0.1 4555
```

Expected result:

- COM2 remains connected to the same running shell/tty path for debugging

## Failure Modes To Capture

If the smoke fails, record whether the issue is:

- VGA output missing
- keyboard input ignored
- shell prompt only reachable on COM2
- `exit` drops into rescue or hangs
- local console works on `pc-i440fx-10.2` but not `isapc`

This checklist is the acceptance gate before moving on to disk-backed rootfs
work for the i486 lane.
