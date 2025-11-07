# mksh Integration for XINIM

## Overview

This directory contains the integration of mksh (MirBSD Korn Shell) as the default shell for XINIM.

## Source

- **Upstream**: https://github.com/MirBSD/mksh  
- **License**: BSD-like (MirOS License)
- **Version**: R59c (latest stable)

## Integration Strategy

### 1. XINIM-Specific Syscall Layer

File: `xinim_syscalls.c`

Provides mksh with XINIM system call interface:
- Process control (fork, exec, wait, exit)
- File operations (open, read, write, close, dup)
- Terminal I/O (ioctl, tcgetattr, tcsetattr)
- Signal handling (signal, kill, sigaction)
- Environment (getenv, setenv, unsetenv)

### 2. Terminal Integration

File: `xinim_terminal.c`

Integrates mksh with XINIM terminal subsystem:
- Raw/cooked mode switching
- Line editing support
- Terminal size detection
- Character input/output
- Color support (ANSI escape sequences)

### 3. Job Control

File: `xinim_job_control.c`

Implements POSIX job control:
- Background/foreground process groups
- Process group management
- SIGTSTP/SIGCONT handling
- Terminal ownership

## Build Process

```bash
# Download mksh source
cd userland/shell/mksh
wget https://www.mirbsd.org/MirOS/dist/mir/mksh/mksh-R59c.tgz
tar xzf mksh-R59c.tgz

# Build with XINIM integration
sh Build.sh \
    -r \
    -c lto \
    -t XINIM \
    -L \
    -DMKSH_ASSUME_UTF8=1 \
    -DMKSH_DISABLE_TTY_WARNING=1
```

## Features Enabled

- ✅ Command-line editing (emacs mode)
- ✅ Command history
- ✅ Tab completion
- ✅ Job control
- ✅ Aliases and functions
- ✅ UTF-8 support
- ✅ POSIX compliance mode
- ✅ Bash compatibility extensions

## Testing

```bash
# Run mksh test suite
cd userland/shell/mksh
./mksh test.sh

# Interactive testing
./mksh
mksh$ echo $KSH_VERSION
mksh$ set -o posix
mksh$ function test { echo "Hello from mksh"; }
mksh$ test
```

## Configuration

Default configuration file: `/etc/mkshrc`

```sh
# System-wide mksh configuration
export PS1='$(whoami)@$(hostname):${PWD} \$ '
export HISTFILE=~/.mksh_history
export HISTSIZE=1000

# Useful aliases
alias ll='ls -la'
alias la='ls -A'
alias l='ls -CF'

# Set safe umask
umask 022
```

User configuration file: `~/.mkshrc`

## Integration Status

- [ ] Download mksh source
- [ ] Implement xinim_syscalls.c
- [ ] Implement xinim_terminal.c
- [ ] Implement xinim_job_control.c
- [ ] Build mksh for XINIM
- [ ] Run test suite
- [ ] Install to /bin/mksh
- [ ] Set as default shell
