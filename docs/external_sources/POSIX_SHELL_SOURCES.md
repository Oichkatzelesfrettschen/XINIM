# POSIX Shell and Utilities Source Contract

## Selected standard

XINIM implements toward this normative baseline:

```text
IEEE Std 1003.1-2017
The Open Group Base Specifications Issue 7, 2018 edition
POSIX.1-2008 with Technical Corrigenda 1 and 2
```

The canonical publication root is:

```text
https://pubs.opengroup.org/onlinepubs/9699919799/
```

The canonical personal-use archive is linked by the official download page:

```text
https://pubs.opengroup.org/onlinepubs/9699919799/download/index.html
https://pubs.opengroup.org/onlinepubs/9699919799/download/susv4-2018.tgz
```

Historical POSIX.2-1992 is not the implementation target. Shell and Utilities
was merged into the unified POSIX.1 specification. Issue 8 / POSIX.1-2024 is
also not the target because it postdates both mksh R59c and dietlibc 0.35.
Issue 7 is the newest mutually defensible specification baseline for this
specific shell and libc pair.

## Reproducible acquisition

Run:

```sh
scripts/fetch_posix_sources.sh
```

The fetcher uses GNU Wget and this fixed browser identity:

```text
Mozilla/5.0 (X11; Linux x86_64; rv:141.0) Gecko/20100101 Firefox/141.0
```

It requires HTTPS, limits redirects, writes through a partial file, verifies
the complete archive before replacement, rejects unsafe tar members, and then
extracts the verified tree. The pinned source identity is:

```text
archive: susv4-2018.tgz
size: 4976241 bytes
sha256: ab6636bca53c7d71d33d2c5149ede574d598fe6ec97fa8b08e0459ef7bcfc104
Last-Modified: Fri, 08 Apr 2022 09:58:14 GMT
ETag: "4bee71-5dc21a213ced4"
members: 1680
regular files: 1668
directories: 12
```

The canonical versioned source object is:

```text
data/external/posix/susv4-2018.tgz
```

The derived build-local paths are:

```text
build/_state/downloads/posix/susv4-2018.tgz
build/_state/downloads/posix/susv4-2018.response.log
build/_state/cache/posix/susv4-2018/
```

The build-state paths remain intentionally ignored by Git. This private
repository versions the exact archive at the canonical source path above as
the immutable SUSv4 authority for its conformance program. The extracted HTML
tree and HTTP response log remain derived evidence; they are regenerated from
the versioned archive and are not independent standards sources.

## OCR decision

The authoritative download is a machine-readable HTML corpus. Tesseract 5.5.3
was inventoried but is not used. OCR would introduce errors into normative
punctuation, shell grammar, tables, and cross-references without recovering any
missing text. OCR becomes applicable only if an authoritative source exists
solely as page images and its text layer cannot be validated.

## Exact utility denominator

The verifier derives 160 standalone utilities from the leaf HTML pages under
`utilities/`, excluding the seven volume navigation and chapter pages. It then
requires the 15 special built-in anchors defined in
`utilities/V3_chap02.html`:

```text
break colon continue dot eval exec exit export readonly return set shift times trap unset
```

The exact derived identities are:

```text
standalone count: 160
standalone key sha256: e4d9aa4398b7a4da71aa647dbb958c710905b3964c27a56bdb2d9fa0004d1906
full count: 175
full key sha256: e1c7db29258b2e7e869d9183306e2462261a87d598750d457722824f7b655e7d
```

`scripts/verify_posix_issue7_archive.py` requires exact equality between that
official set and `docs/posix/posix2_utility_ledger.tsv`. Its mutation test
proves rejection of an altered archive hash, a missing utility page, an
unexpected utility page, and a missing special built-in anchor.

## Exact shell-language denominator

The shell-language verifier owns the Chapter 2 scope beginning at `tag_18` and
ending immediately before the embedded `break` utility manual. That interval
contains 75 `tag_18*` anchors. The chapter root and these four example-only
anchors are informative exclusions:

```text
tag_18
tag_18_06_02_01
tag_18_06_04_01
tag_18_07_04_01
tag_18_09_03_01
```

The resulting normative heading denominator is 70 source-ordered rows:

```text
anchor count: 70
anchor key sha256: 94198e92e4559a4919ea1f481341e474980b1472c0b66732c906673688f7f5af
anchor-title sha256: f9be211b16d1e77a9198419d0d85b56d3ca9756ccc6988406c52b9522f737493
```

`scripts/verify_posix_shell_language_ledger.py` derives those rows directly
from the verified archive and requires exact equality with
`docs/posix/posix_shell_language_ledger.tsv`. Its mutation self-test rejects a
duplicate ledger anchor, a missing row, a changed source title, a false closed
witness, a missing official anchor, and an unexpected official anchor.

The denominator does not flatten option fragments or broad clauses into one
smoke result. `tag_18_05_03`, `tag_18_09_01_01`, and `tag_18_12` contain UP or
XSI fragments and require profile-specific witnesses before closure.
`tag_18_10_02` contains 47 named grammar productions with 111 source-ordered
alternatives. The preceding lexical section contributes 3 initial token
classifications, and the grammar rules contribute 11 context rows: rules 1
through 9 with 6a, 6b, 7a, and 7b retained separately. The resulting
`docs/posix/posix_shell_grammar_requirements.tsv` denominator has 125 rows:

```text
requirement count: 125
production count: 47
grammar alternative count: 111
ordered key sha256: b0fdb308b771070cf97c8487fa584ab141a395cc922aefe96030d230b8c03c38
ordered source-row sha256: db0d57893ee7683fcf8046c52db4b2fde5463f111cc6166c81e56266afa99741
```

`scripts/verify_posix_shell_grammar_requirements.py` derives that denominator
directly from the verified Chapter 2 source. It rejects missing, duplicate,
reordered, or altered rows; false closure; unknown executable witnesses; and
mutations to both lexical rules and grammar alternatives. The ledger has 125
closed rows and no open rows. Exact Q35 Ring 3 discriminators close the 3
initial lexical classifications, all 11 context rules, and all 111 production
alternatives, including the simple-command and redirection spine.

Closing all 70 shell-language rows proves only the declared Chapter 2
denominator, not the 175 utility rows or broader POSIX system-interface
conformance.

### Quoting evidence boundary

The `tag_18_02_01` witness family separates the backslash requirements into
ordinary and lexer-special portable characters, a literal tab in a generated
non-interactive script, and a physical backslash-newline pair. The last case
requires one `leftright` token, proving that removal occurs before token
splitting and inserts no separator. In pinned mksh R59c, `lex.c` implements
the general unquoted-backslash rule in the `Sbase2` state by consuming the next
character and emitting `QCHAR` plus that character. The Q35 Ring 3 cases prove
the built shell retains that source behavior through the XINIM terminal,
filesystem, process, and libc path.

The `tag_18_02_02` family preserves an empty field, shell metacharacters, tab,
and newline, constructs a literal quote only by ending and reopening quoted
spans, and runs a malformed nested-quote script in a separate shell process.
Pinned mksh R59c `lex.c` enters `SSQUOTE` at an opening quote, emits every
non-quote input character as `QCHAR`, and leaves the state at the next quote.
The malformed-script witness requires a nonzero status and no command output;
it intentionally does not depend on implementation-specific diagnostic text.

The empty-field observation also exercises later field-splitting and quote-
removal behavior. The malformed-script observation also exercises the syntax-
error path covered by `tag_18_08_01`. These overlaps do not close those rows;
their remaining normative requirements retain independent open actions.

`tag_18_02_03` contains thirteen source-ordered obligations or boundaries. The
retained Q35 Ring 3 matrix proves preservation of non-exception lexer classes,
literal tab and newline bytes, the five-character backslash whitelist,
backslash-newline removal, escaped quote inclusion, all three dollar-led
expansion forms, recursive `$()` parenthesis matching, even quote pairs and
escaped braces in `${...}`, escaped backquote delimiting, and both zero- and
multiple-field `"$@"` behavior.

The row remains open because the clause delegates the meanings of those
constructs. Closure requires `tag_18_03`, `tag_18_05_02`, `tag_18_06_02`,
`tag_18_06_03`, and `tag_18_06_04`. The retained mksh R59c patch and
`mksh_command_substitution_boundary` host test prove that alias substitution is
excluded while locating the physical command-substitution parenthesis, then
enabled while parsing the bounded text. The exact Q35 Ring 3 witness proves a
hostile alias containing `)` cannot execute beyond the physical delimiter.
The row explicitly assigns undefined results to an unclosed quote
inside a backquoted sequence and to an unclosed backquoted sequence inside the
same double-quoted string. The test suite deliberately does not execute or
assign diagnostics to either undefined form.

### Token-recognition evidence boundary

`docs/posix/posix_token_recognition_requirements.tsv` derives a 17-row
sub-denominator from `tag_18_03` through `tag_18_03_01`: four governing
token-recognition paragraphs, ten first-applicable ordered rules, and three
alias-substitution paragraphs. Each row carries the SHA-256 of its normalized
official source paragraph. The ordered requirement-source identity is:

```text
count: 17
sha256: 09e8a0cb31d41033af878004b9096a68cce6bc30ccf8962a27d343c7a90c50c5
```

The multi-character operator sub-denominator for recognition rules 2, 3, and
6 is:

```text
&& || ;; << >> <& >& <> <<- >|
```

The single-character operator starters are `&`, `;`, `|`, `<`, `>`, `(`, and
`)`; unquoted newline is the eighth rule 6 starter. `<<-` is the sole required
three-character operator. Closure requires every full operator, every proper
prefix, a non-extending next character, adjacency without blanks, and quoted
forms that remain word data.

Recognition rule 2 is closed by exact Q35 Ring 3 witnesses for all ten
multi-character operators. Each witness requires the combined token's real
control-flow, case, here-document, or redirection behavior. Recognition rule 3
is closed by an 18-case matrix covering every control and redirection operator,
including newline, plus every proper prefix. Each case places an adjacent
non-extending character after the operator and requires that character to
retain its command, reserved-word, delimiter, descriptor, or pathname role.
Recognition rule 6 is closed by a separate eight-case matrix for `&`, `;`,
`|`, `<`, `>`, `(`, `)`, and newline. Every case places the starter directly
after a preceding word and requires the new operator token's control-flow,
redirection, grammar-delimiter, or command-termination behavior. Rule 2 or 3
success is not reused as evidence for this distinct requirement.

Recognition rule 7 is closed by five exact Q35 Ring 3 witnesses. In the
selected C/POSIX locale, the complete `<blank>` set is space and tab. Separate
cases prove that each byte delimits a preceding word and is discarded, that a
repeated mixed blank sequence with no current token is discarded, and that
single-quoted, double-quoted, and backslash-quoted blanks remain word data. The
tab-delimitation case writes byte 0x09 into a noninteractive script so terminal
line editing cannot consume the byte before token recognition.

Recognition rule 8 is closed by five exact Q35 Ring 3 witnesses. The matrix
covers alphanumerics and portable filename characters; every printable
portable punctuation glyph that reaches rule 8 after the earlier predicates;
the nonblank portable controls alert, backspace, vertical tab, form feed, and
carriage return; and ordinary-character continuation after quoted text and
substitution units. Pathname expansion is disabled only while observing the
punctuation token. The control case generates exact bytes in a noninteractive
script and compares the unquoted token with a quoted byte-identical value.

Recognition rule 9 is closed by six exact Q35 Ring 3 witnesses. Generated
scripts prove that a word-start `#` discards unmatched quote characters,
substitution syntax, operators, text, and tab through the end of the line while
leaving the terminating newline active. Separate cases start a comment after
leading blanks and directly after an operator. Quoted, escaped, and mid-word
hash characters remain word data, preserving rule 8 precedence for the
mid-word case.

Recognition rule 10 is closed by 13 exact Q35 Ring 3 witnesses. The boundary
matrix starts words at input start and after space, tab, a control operator, a
redirection operator, newline, and a comment-terminating newline. A printable
matrix enumerates every portable punctuation glyph that reaches rule 10, and
five byte-specific scripts cover alert, backspace, vertical tab, form feed,
and carriage return. The byte-specific transport commands stay below the
terminal's 255-byte canonical input record; the generated noninteractive
scripts, not the interactive line editor, carry the exact control bytes.

Recognition rule 4 is closed by five exact Q35 Ring 3 witnesses. Separate
cases cover backslash, single-quote, and double-quote introducers, preserve
special characters inside one token, and append ordinary text after the quoted
field without delimiting that token. Parameter, command, and arithmetic
substitutions yield operator-looking bytes without reparsing them as shell
syntax, demonstrating deferred expansion. A generated script removes only a
backslash-newline pair while retaining an unescaped newline inside quoted data.

Recognition rule 5 is closed by seven exact Q35 Ring 3 witnesses. The matrix
covers `$name`, `${name}`, `$(command)`, backquote command substitution, and
`$((expression))`, with ordinary text joined on both sides of each expansion.
Nested parameter, command, arithmetic, and quote constructs prove recursive
delimiter matching. A separate hostile-alias invocation proves that alias text
cannot move the physical command-substitution boundary. All ten ordered token
rules are closed independently. The governing grammar-categorization row binds
their token forms to the complete 125-row grammar denominator and its 128 exact
Q35 Ring 3 cases.

The governing line-input paragraph is closed by the source mechanism and two
exact Q35 Ring 3 witnesses. mksh `lex.c` reads noninteractive input through an
`XString` and calls `XcheckN` whenever the remaining capacity is exhausted;
`misc.c` grows that area rather than applying a fixed line-length ceiling. Each
witness constructs a 32768-byte physical line with fifteen exact string
doublings. One line is parsed in ordinary mode and executes a length assertion;
the other carries a 32768-byte here-document body and verifies the body length.
The supporting kernel contract replaces the former 256-byte exec string matrix
with a 64953-byte aggregate argument and environment arena derived from the
mapped 64 KiB initial stack. The p04 grammar-categorization row is closed
through the complete grammar case group and the zero-open-row grammar
dependency gate.

The governing first-applicable paragraph is closed by a separate eight-case
overlap matrix. It discriminates operator continuation from a new operator,
operator delimitation followed by reprocessing, quote and substitution
recursion from later delimiter rules, mid-word from word-start hash handling,
leading empty-delimiter skipping, and EOF delimitation. These witnesses do not
reuse the individual ordered-rule cases.

The here-document trigger paragraph is closed by two exact Q35 Ring 3
witnesses. A single-body case keeps an `&&` list on the `io_here` command line
and proves that body acquisition begins only after the next NEWLINE. A
multiple-body case attaches fd 3 and standard input to separate here-documents
and verifies their values in source order, making reversal or collapse
observable.

The command-name alias-substitution paragraph is closed by seven independent
exact Q35 Ring 3 witnesses. They distinguish a command name from an ordinary
argument, exclude a quoted command name and a reserved word in grammatical
context, exercise definition and later removal through `alias` and `unalias`,
invoke one portable alias name containing alphabetics, digits, underscore,
`!`, `%`, `,`, and `@`, and prove that a self-reference executes the alias body
once before the still-active name is left unexpanded. The lifecycle cases use
`eval` only to create a later parse after the defining or removal utility has
executed; they do not rely on aliases taking effect while the original input
line is still being parsed.

The trailing-blank alias paragraph is closed by three exact Q35 Ring 3
witnesses. One chain passes alias eligibility through two successive command
words and observes both replacements. The stop cases place another defined
alias after either a slash-containing invalid alias name or an alias whose
value has no trailing blank; both require that later word to remain literal.
This distinguishes continued propagation from each source-specified stopping
condition.

The alias-environment paragraph is closed by three exact Q35 Ring 3 witnesses.
The defining shell executes its alias after definition, an explicitly invoked
`/bin/sh` reports the same name absent while the parent definition still
executes, and a directly invoked executable shell-script utility reports the
name absent from its utility environment. The utility witness enters through
the command execution path rather than through `/bin/sh script`, preserving
the standard's distinction between a separate shell invocation and a utility
environment. All alias cases reuse one bounded bootfs scratch inode and remove
it after the utility witness.

The interval contains no unspecified, undefined, or application-controlled
result. Predefined aliases are permitted but not required, so tests isolate
their own names and never require the initial alias table to be empty.

Unpatched mksh R59c has a mechanism defect at the `$()` recognition boundary.
`lex.c` recognizes `$(` and enters `yyrecursive(COMSUB)`. In `syn.c`, that path
invokes the nested parser with `ALIAS`, allowing the `lex.c` alias lookup and
`SALIAS` source path while the physical matching parenthesis is being located
and the parse tree is serialized. `eval.c` later recompiles a `COMSUB` with
alias expansion disabled. This is the reverse of the required boundary even
when an ordinary output example happens to succeed.

The retained mksh R59c patch corrects that boundary by separating two
operations:

1. Scan original physical source to the matching parenthesis with recursive
   quote and substitution recognition and no alias-table access.
2. Parse the bounded command text normally with command-name alias
   substitution enabled.

The mechanism test varies the alias table while holding source bytes fixed.
The physical delimiter offset and captured-source hash must remain identical,
and the scan-time alias-lookup count must remain zero. Parsing the bounded
text must then perform normal alias lookup. A mutation that enables alias
lookup in the scanner must fail the counter and offset-or-hash invariant.

## Shell and libc compatibility boundary

mksh R59c is not itself a whole-shell conformance claim. Its upstream FAQ says
that the closer POSIX configuration is the `-L` legacy profile in POSIX mode
under the `C` locale. That profile uses the host C `long` arithmetic required
by POSIX, while full mksh deliberately uses defined 32-bit arithmetic on every
host. POSIX mode must also keep mksh UTF-8 mode disabled.

The x86_64 image therefore builds one executable from the pinned mksh tree with
`MKSH_LEGACY_MODE` and `MKSH_BINSHPOSIX`, installs it byte-identically as
`/bin/mksh` and `/bin/sh`, and installs no `/bin/lksh` or `/bin/xash`. The
legacy identity remains visible in `KSH_VERSION`; only the redundant pathname
and alternate shell implementations are removed. The exact QEMU gate proves
Ring 3 PID 1 startup through `/bin/sh`, the sole-shell layout, the full 64-bit
signed-`long` value range, POSIX mode, the `C` locale, disabled brace
expansion, and byte-oriented string length. These witnesses are necessary but
do not close the `sh` row without the complete shell-language matrix.

dietlibc 0.35 currently defines `_POSIX_VERSION` as `199506L`. It contains
selected later interfaces and recognizes `_POSIX_C_SOURCE >= 200809L`, but
that does not establish Issue 7 conformance. `WANT_FULL_POSIX_COMPAT` remains
disabled in the pristine vendored defaults; the XINIM-owned x86_64 build copy
enables it without rewriting the pinned source or raising `_POSIX_VERSION`.
Every required libc, kernel, shell, and utility behavior must still close
before advertising a newer `_POSIX_VERSION`.

The result is a three-part statement:

- Engineering target: POSIX.1-2008 / Issue 7, 2018 edition.
- Present advertised libc floor: POSIX 1995 (`199506L`).
- Present conformance claim: none; 1 utility row is closed and 174 remain open.
