# Rust Reimplementation

This directory houses experimental Rust versions of select utilities from the original MINIX 1 sources. Each tool is implemented as a separate Cargo package.

The initial example is a rewrite of the `cat` command. It behaves similarly to the original C program, supporting the `-u` flag for unbuffered operation and reading from standard input when no files are provided or when a filename of `-` is supplied.

Build with:

```sh
cargo build --manifest-path cat/Cargo.toml
```

Run the resulting binary from `target/debug` or `target/release` as desired.
