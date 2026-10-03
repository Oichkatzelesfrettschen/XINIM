# Pinned mksh source archive

`mksh-R59c.tgz` is the unmodified mksh R59c release archive from
`http://www.mirbsd.org/MirOS/dist/mir/mksh/mksh-R59c.tgz`. Its SHA-256 is
`77ae1665a337f1c48c61d6b961db3e52119b38e58884d1c89684af31f87bc506`.
The archive retains the upstream source headers and license terms.

The i486 build passes this archive to `scripts/acquire_mksh_source.py`, which
verifies the digest and validates member paths before extraction. Retaining
the exact release bytes permits public CI to build when the upstream HTTP
host is unreachable from a runner. The 32-bit mksh build uses the pristine
extracted source; XINIM-owned integration stays outside the archive.
