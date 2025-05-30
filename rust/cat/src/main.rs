use std::env;
use std::fs::File;
use std::io::{self, Read, Write, BufReader};

/// Copy data from the reader to the writer using a small buffer.
/// When `unbuffered` is true, data is flushed immediately after each read.
fn copy_reader<R: Read, W: Write>(reader: &mut R, writer: &mut W, unbuffered: bool) -> io::Result<()> {
    // Local buffer for transfers
    let mut buf = [0u8; 512];
    // Optional buffer for batched writes
    let mut out_buf: Vec<u8> = Vec::new();
    loop {
        let n = reader.read(&mut buf)?;
        if n == 0 {
            break;
        }
        if unbuffered {
            // Write immediately when running unbuffered
            writer.write_all(&buf[..n])?;
        } else {
            // Accumulate output and flush when full
            out_buf.extend_from_slice(&buf[..n]);
            if out_buf.len() >= 512 {
                writer.write_all(&out_buf)?;
                out_buf.clear();
            }
        }
    }
    // Flush any remaining buffered data
    if !unbuffered && !out_buf.is_empty() {
        writer.write_all(&out_buf)?;
    }
    Ok(())
}

fn main() -> io::Result<()> {
    // Collect command line arguments into a vector for reuse
    let mut args: Vec<String> = env::args().skip(1).collect();
    // Check for -u which selects unbuffered mode
    let mut unbuffered = false;
    if let Some(first) = args.first() {
        if first == "-u" {
            unbuffered = true;
            args.remove(0);
        }
    }

    let mut stdout = io::stdout();

    // If no files were provided, read from stdin
    if args.is_empty() {
        copy_reader(&mut io::stdin(), &mut stdout, unbuffered)?;
    } else {
        for fname in args {
            if fname == "-" {
                // '-' denotes standard input
                copy_reader(&mut io::stdin(), &mut stdout, unbuffered)?;
            } else {
                match File::open(&fname) {
                    Ok(file) => {
                        // Use a buffered reader for files
                        let mut reader = BufReader::new(file);
                        copy_reader(&mut reader, &mut stdout, unbuffered)?;
                    }
                    Err(e) => {
                        // Mirror the C implementation's error reporting
                        eprintln!("cat: cannot open {}: {}", fname, e);
                    }
                }
            }
        }
    }

    // Ensure all output is written
    stdout.flush()?;
    Ok(())
}
