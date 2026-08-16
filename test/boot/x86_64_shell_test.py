#!/usr/bin/env python3
"""
Programmatic Ring 3 x86_64 POSIX sh PID 1 test for XINIM.

Boots the Limine x86_64 image, requires the marker emitted by the mapped
shell startup file after CPL3 entry, and validates the shell over the COM2 TTY.
"""

import os
import socket
import subprocess
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))

XINIM_STATE_ROOT = os.environ.get(
    "XINIM_STATE_ROOT", os.path.join(PROJECT_ROOT, "build", "_state")
)
XINIM_BUILD_ROOT = os.environ.get(
    "XINIM_BUILD_ROOT", os.path.join(PROJECT_ROOT, "build", "x86_64", "Debug")
)
XINIM_IMAGE_ROOT = os.environ.get(
    "XINIM_IMAGE_ROOT", os.path.join(XINIM_BUILD_ROOT, "images")
)
BOOT_IMAGE = os.environ.get(
    "XINIM_QEMU_BOOT_IMAGE",
    os.path.join(XINIM_IMAGE_ROOT, "x86_64", "xinim-x86_64.iso"),
)
QEMU_BIN = "qemu-system-x86_64"
SHELL_PORT = 4555
COM1_LOG = os.environ.get("XINIM_QEMU_COM1_LOG", "/dev/null")
BOOT_TIMEOUT = 20
CMD_TIMEOUT = 10
SHELL_SERIAL_RX_PAYLOAD_LIMIT = 255
SHELL_LANGUAGE_SCRATCH_DIRECTORY = "/tmp/t"
SHELL_PROMPT = "sh# "
RING3_SENTINEL = "[user] Ring 3 POSIX sh PID 1 entry"
MOTD_SENTINEL = "XINIM x86_64"
SHELL_LANGUAGE_COMMAND_CASES = (
    (
        "tag_18_03.p01_line_input_modes.dynamic_ordinary_line",
        (
            "v=x;c=0;while test $c -lt 15;do v=$v$v;c=$((c+1));done;"
            '{ printf v=;printf %s "$v";printf \';test ${#v} -eq 32768&&'
            "printf TOKEN_P01_ORDINARY_OK\\n';}>/tmp/l;"
            "/bin/sh /tmp/l;s=$?;rm -f /tmp/l;unset v;test $s = 0"
        ),
        "TOKEN_P01_ORDINARY_OK",
    ),
    (
        "tag_18_03.p01_line_input_modes.dynamic_here_document_line",
        (
            "v=x;c=0;while test $c -lt 15;do v=$v$v;c=$((c+1));done;"
            "{ printf 'IFS= read -r v <<E\\n';printf %s \"$v\";"
            "printf '\\nE\\ntest ${#v} -eq 32768&&"
            "printf TOKEN_P01_HEREDOC_OK\\n';}>/tmp/l;"
            "/bin/sh /tmp/l;s=$?;rm -f /tmp/l;unset v;test $s = 0"
        ),
        "TOKEN_P01_HEREDOC_OK",
    ),
    (
        "tag_18_03.p02_here_document_mode.body_after_next_newline",
        (
            '/bin/printf \'IFS= read -r v <<E && test "$v" = body && '
            "/bin/printf TOKEN_P02_NEXT_NEWLINE_OK\\nbody\\nE\\n' "
            ">/tmp/p02n; /bin/sh /tmp/p02n"
        ),
        "TOKEN_P02_NEXT_NEWLINE_OK",
    ),
    (
        "tag_18_03.p02_here_document_mode.multiple_bodies_in_source_order",
        (
            "/bin/printf '{ IFS= read -r a <&3; IFS= read -r b; "
            'test "$a" = one && test "$b" = two && '
            "/bin/printf TOKEN_P02_SOURCE_ORDER_OK; } 3<<A <<B\\n"
            "one\\nA\\ntwo\\nB\\n' >/tmp/p02m; /bin/sh /tmp/p02m"
        ),
        "TOKEN_P02_SOURCE_ORDER_OK",
    ),
    (
        "tag_18_03.p03_first_applicable_rule.operator_continuation_precedes_start",
        "/bin/true&&/bin/printf TOKEN_P03_CONTINUATION_OK",
        "TOKEN_P03_CONTINUATION_OK",
    ),
    (
        "tag_18_03.p03_first_applicable_rule.operator_delimit_then_restart",
        "/bin/true&&(/bin/printf TOKEN_P03_OPERATOR_RESTART_OK)",
        "TOKEN_P03_OPERATOR_RESTART_OK",
    ),
    (
        "tag_18_03.p03_first_applicable_rule.quoted_specials_precede_later_rules",
        (
            "set -- ' $|# ' \"\\$|# \"; "
            'test "$#" -eq 2 && test "$1" = \' $|# \' && '
            "test \"$2\" = '$|# ' && /bin/printf TOKEN_P03_QUOTED_OK"
        ),
        "TOKEN_P03_QUOTED_OK",
    ),
    (
        "tag_18_03.p03_first_applicable_rule.substitution_precedes_delimiters",
        (
            "value=$(/bin/printf '%s' '| # )'); "
            "test \"$value\" = '| # )' && /bin/printf TOKEN_P03_SUBSTITUTION_OK"
        ),
        "TOKEN_P03_SUBSTITUTION_OK",
    ),
    (
        "tag_18_03.p03_first_applicable_rule.word_continuation_precedes_comment",
        (
            'set -- left#right; test "$#" -eq 1 && '
            'test "$1" = left#right && /bin/printf TOKEN_P03_MIDWORD_HASH_OK'
        ),
        "TOKEN_P03_MIDWORD_HASH_OK",
    ),
    (
        "tag_18_03.p03_first_applicable_rule.comment_precedes_word_start",
        (
            "/bin/printf '#; exit 99\\n"
            "/bin/printf TOKEN_P03_COMMENT_OK\\n' >/tmp/p03c; /bin/sh /tmp/p03c"
        ),
        "TOKEN_P03_COMMENT_OK",
    ),
    (
        "tag_18_03.p03_first_applicable_rule.empty_delimiters_are_skipped",
        (
            "/bin/printf ' \\011  set -- value\\n"
            'test "$#" -eq 1 && test "$1" = value && '
            "/bin/printf TOKEN_P03_EMPTY_DELIMITER_OK\\n' >/tmp/p03b; "
            "/bin/sh /tmp/p03b"
        ),
        "TOKEN_P03_EMPTY_DELIMITER_OK",
    ),
    (
        "tag_18_03.p03_first_applicable_rule.end_of_input_delimits_word",
        (
            "/bin/printf \"/bin/printf 'TOKEN_P03_EOF_OK'\" "
            ">/tmp/p03e; /bin/sh /tmp/p03e"
        ),
        "TOKEN_P03_EOF_OK",
    ),
    (
        "tag_18_03.rule01_end_of_input.final_word_executes_at_eof",
        "/bin/printf \"/bin/printf 'TOKEN_END_OF_INPUT_OK'\" "
        "> /tmp/posix-token-eof; /bin/sh /tmp/posix-token-eof",
        "TOKEN_END_OF_INPUT_OK",
    ),
    (
        "tag_18_03.rule02_operator_continuation.and_if",
        "/bin/true && /bin/printf 'TOKEN_AND_IF_OK\\n'",
        "TOKEN_AND_IF_OK",
    ),
    (
        "tag_18_03.rule02_operator_continuation.or_if",
        "test left = right || /bin/printf 'TOKEN_OR_IF_OK\\n'",
        "TOKEN_OR_IF_OK",
    ),
    (
        "tag_18_03.rule02_operator_continuation.case_terminator",
        "case token in token) /bin/printf 'TOKEN_CASE_TERMINATOR_OK\\n' ;; esac",
        "TOKEN_CASE_TERMINATOR_OK",
    ),
    (
        "tag_18_03.rule02_operator_continuation.here_document",
        (
            "/bin/printf 'IFS= read -r value <<EOF\\n"
            "TOKEN_HEREDOC_VALUE\\n"
            "EOF\\n"
            'test "$value" = TOKEN_HEREDOC_VALUE && '
            '/bin/printf "%%s\\\\n" TOKEN_LESS_LESS_OK\\n\' '
            "> /tmp/token-less-less.sh; /bin/sh /tmp/token-less-less.sh"
        ),
        "TOKEN_LESS_LESS_OK",
    ),
    (
        "tag_18_03.rule02_operator_continuation.append_redirection",
        (
            "/bin/printf 'first' >/tmp/token-append; "
            "/bin/printf 'second' >>/tmp/token-append; "
            "exec 3</tmp/token-append; IFS= read -r appended_value <&3; "
            "exec 3<&-; "
            'test "$appended_value" = firstsecond && '
            "/bin/printf 'TOKEN_GREATER_GREATER_OK\\n'"
        ),
        "TOKEN_GREATER_GREATER_OK",
    ),
    (
        "tag_18_03.rule02_operator_continuation.duplicate_input",
        (
            "/bin/printf 'TOKEN_INPUT_DUPLICATION_VALUE\\n' >/tmp/token-input; "
            "exec 3</tmp/token-input; IFS= read -r duplicated_input <&3; "
            "exec 3<&-; "
            'test "$duplicated_input" = TOKEN_INPUT_DUPLICATION_VALUE && '
            "/bin/printf 'TOKEN_LESS_AMPERSAND_OK\\n'"
        ),
        "TOKEN_LESS_AMPERSAND_OK",
    ),
    (
        "tag_18_03.rule02_operator_continuation.duplicate_output",
        (
            "exec 3>/tmp/to; /bin/printf 'OUTPUT_VALUE\\n' >&3; "
            "exec 3>&-; exec 3</tmp/to; IFS= read -r value <&3; "
            'exec 3<&-; test "$value" = OUTPUT_VALUE && '
            "/bin/printf 'TOKEN_GREATER_AMPERSAND_OK\\n'"
        ),
        "TOKEN_GREATER_AMPERSAND_OK",
    ),
    (
        "tag_18_03.rule02_operator_continuation.read_write_redirection",
        (
            "/bin/printf 'TOKEN_READ_WRITE_VALUE\\n' >/tmp/token-read-write; "
            "exec 3<>/tmp/token-read-write; "
            "IFS= read -r read_write_value <&3; exec 3>&-; "
            'test "$read_write_value" = TOKEN_READ_WRITE_VALUE && '
            "/bin/printf 'TOKEN_LESS_GREATER_OK\\n'"
        ),
        "TOKEN_LESS_GREATER_OK",
    ),
    (
        "tag_18_03.rule02_operator_continuation.tab_stripping_here_document",
        (
            "/bin/printf 'IFS= read -r value <<-EOF\\n"
            "\\tTOKEN_TAB_HEREDOC_VALUE\\n"
            "\\tEOF\\n"
            'test "$value" = TOKEN_TAB_HEREDOC_VALUE && '
            '/bin/printf "%%s\\\\n" TOKEN_LESS_LESS_HYPHEN_OK\\n\' '
            "> /tmp/token-less-less-hyphen.sh; "
            "/bin/sh /tmp/token-less-less-hyphen.sh"
        ),
        "TOKEN_LESS_LESS_HYPHEN_OK",
    ),
    (
        "tag_18_03.rule02_operator_continuation.clobber_override",
        (
            "/bin/printf old >/tmp/tc; set -C; "
            "/bin/printf new >|/tmp/tc; status=$?; set +C; "
            "exec 3</tmp/tc; IFS= read -r value <&3; exec 3<&-; "
            'test "$status" -eq 0 && test "$value" = new && '
            "/bin/printf 'TOKEN_GREATER_PIPE_OK\\n'"
        ),
        "TOKEN_GREATER_PIPE_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.ampersand",
        "/bin/true&wait; /bin/printf TOKEN_R3_AMPERSAND_OK",
        "TOKEN_R3_AMPERSAND_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.and_if",
        "/bin/true&&/bin/printf TOKEN_R3_AND_IF_OK",
        "TOKEN_R3_AND_IF_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.left_parenthesis",
        "(/bin/printf TOKEN_R3_LEFT_PARENTHESIS_OK)",
        "TOKEN_R3_LEFT_PARENTHESIS_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.right_parenthesis",
        "case x in x)/bin/printf TOKEN_R3_RIGHT_PARENTHESIS_OK;;esac",
        "TOKEN_R3_RIGHT_PARENTHESIS_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.semicolon",
        "/bin/true;/bin/printf TOKEN_R3_SEMICOLON_OK",
        "TOKEN_R3_SEMICOLON_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.case_terminator",
        "case x in x) :;;esac; /bin/printf TOKEN_R3_CASE_TERMINATOR_OK",
        "TOKEN_R3_CASE_TERMINATOR_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.newline",
        (
            "/bin/printf '/bin/true\\n"
            "/bin/printf TOKEN_R3_NEWLINE_OK\\n' >/tmp/r3n; "
            "/bin/sh /tmp/r3n"
        ),
        "TOKEN_R3_NEWLINE_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.pipe",
        (
            "/bin/printf 'V\\n'|while IFS= read -r value; do "
            'test "$value" = V && /bin/printf TOKEN_R3_PIPE_OK; done'
        ),
        "TOKEN_R3_PIPE_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.or_if",
        "test x = y||/bin/printf TOKEN_R3_OR_IF_OK",
        "TOKEN_R3_OR_IF_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.less",
        (
            "/bin/printf 'V\\n' >/tmp/r3l; exec 3</tmp/r3l; "
            "IFS= read -r value <&3; exec 3<&-; "
            'test "$value" = V && /bin/printf TOKEN_R3_LESS_OK'
        ),
        "TOKEN_R3_LESS_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.greater",
        (
            "/bin/printf 'V\\n'>/tmp/r3g; exec 3</tmp/r3g; "
            "IFS= read -r value <&3; exec 3<&-; "
            'test "$value" = V && /bin/printf TOKEN_R3_GREATER_OK'
        ),
        "TOKEN_R3_GREATER_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.clobber",
        (
            "/bin/printf old >/tmp/r3c; set -C; "
            "/bin/printf new >|/tmp/r3c; status=$?; set +C; "
            "exec 3</tmp/r3c; IFS= read -r value <&3; exec 3<&-; "
            'test "$status" -eq 0 && test "$value" = new && '
            "/bin/printf TOKEN_R3_CLOBBER_OK"
        ),
        "TOKEN_R3_CLOBBER_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.here_document",
        (
            "/bin/printf 'IFS= read -r value <<EOF\\n"
            "V\\nEOF\\n"
            'test "$value" = V && /bin/printf TOKEN_R3_DLESS_OK\\n\' '
            ">/tmp/r3h; /bin/sh /tmp/r3h"
        ),
        "TOKEN_R3_DLESS_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.append",
        (
            "/bin/printf A >/tmp/r3a; /bin/printf B>>/tmp/r3a; "
            "exec 3</tmp/r3a; IFS= read -r value <&3; exec 3<&-; "
            'test "$value" = AB && /bin/printf TOKEN_R3_APPEND_OK'
        ),
        "TOKEN_R3_APPEND_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.duplicate_input",
        (
            "/bin/printf 'V\\n' >/tmp/r3i; exec 3</tmp/r3i; "
            "IFS= read -r value <&3; exec 3<&-; "
            'test "$value" = V && /bin/printf TOKEN_R3_LESSAND_OK'
        ),
        "TOKEN_R3_LESSAND_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.duplicate_output",
        (
            "exec 3>/tmp/r3o; /bin/printf 'V\\n' >&3; exec 3>&-; "
            "exec 3</tmp/r3o; IFS= read -r value <&3; exec 3<&-; "
            'test "$value" = V && /bin/printf TOKEN_R3_GREATAND_OK'
        ),
        "TOKEN_R3_GREATAND_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.tab_stripping_here_document",
        (
            "/bin/printf 'IFS= read -r value <<-EOF\\n"
            "\\tV\\n\\tEOF\\n"
            'test "$value" = V && /bin/printf TOKEN_R3_DLESSDASH_OK\\n\' '
            ">/tmp/r3t; /bin/sh /tmp/r3t"
        ),
        "TOKEN_R3_DLESSDASH_OK",
    ),
    (
        "tag_18_03.rule03_operator_delimitation.read_write",
        (
            "/bin/printf 'V\\n' >/tmp/r3w; exec 3<>/tmp/r3w; "
            "IFS= read -r value <&3; exec 3>&-; "
            'test "$value" = V && /bin/printf TOKEN_R3_LESSGREAT_OK'
        ),
        "TOKEN_R3_LESSGREAT_OK",
    ),
    (
        "tag_18_03.rule04_quoted_text.backslash_membership",
        (
            "set -- pre\\ \\|\\&\\;\\<\\>\\(\\)\\#post; "
            'test "$#" -eq 1 && test "$1" = \'pre |&;<>()#post\' && '
            "/bin/printf TOKEN_R4_BACKSLASH_OK"
        ),
        "TOKEN_R4_BACKSLASH_OK",
    ),
    (
        "tag_18_03.rule04_quoted_text.single_quote_membership",
        (
            "set -- pre' a;|$()# 'post; "
            'test "$#" -eq 1 && test "$1" = \'pre a;|$()# post\' && '
            "/bin/printf TOKEN_R4_SINGLE_QUOTE_OK"
        ),
        "TOKEN_R4_SINGLE_QUOTE_OK",
    ),
    (
        "tag_18_03.rule04_quoted_text.double_quote_membership",
        (
            'value=middle; set -- pre" $value ;|# "post; '
            'test "$#" -eq 1 && test "$1" = \'pre middle ;|# post\' && '
            "/bin/printf TOKEN_R4_DOUBLE_QUOTE_OK"
        ),
        "TOKEN_R4_DOUBLE_QUOTE_OK",
    ),
    (
        "tag_18_03.rule04_quoted_text.substitution_is_deferred",
        (
            "value=';|&<>()'; "
            "set -- pre${value}post pre$(/bin/printf '|;&')post "
            "pre$((40 + 2))post; "
            'test "$#" -eq 3 && test "$1" = \'pre;|&<>()post\' && '
            'test "$2" = \'pre|;&post\' && test "$3" = pre42post && '
            "/bin/printf TOKEN_R4_DEFERRED_OK"
        ),
        "TOKEN_R4_DEFERRED_OK",
    ),
    (
        "tag_18_03.rule04_quoted_text.newline_joining_exception",
        (
            "/bin/printf 'set -- a\\134\\nb \\047c\\nd\\047\\n"
            "e=\\047c\\nd\\047\\n"
            'test "$1" = ab && test "$2" = "$e" && '
            "/bin/printf TOKEN_R4_NEWLINE_OK\\n' >/tmp/r4; /bin/sh /tmp/r4"
        ),
        "TOKEN_R4_NEWLINE_OK",
    ),
    (
        "tag_18_03.rule05_substitution_recursion.parameter_introducers",
        (
            "name=value; set -- pre$name-post pre${name}post; "
            'test "$#" -eq 2 && test "$1" = prevalue-post && '
            'test "$2" = prevaluepost && /bin/printf TOKEN_R5_PARAMETER_OK'
        ),
        "TOKEN_R5_PARAMETER_OK",
    ),
    (
        "tag_18_03.rule05_substitution_recursion.dollar_command_substitution",
        (
            "set -- pre$(/bin/printf middle)post; "
            'test "$#" -eq 1 && test "$1" = premiddlepost && '
            "/bin/printf TOKEN_R5_DOLLAR_COMMAND_OK"
        ),
        "TOKEN_R5_DOLLAR_COMMAND_OK",
    ),
    (
        "tag_18_03.rule05_substitution_recursion.backquote_substitution",
        (
            "/bin/printf 'set -- pre\\140/bin/printf middle\\140post\\n"
            'test "$#" -eq 1 && test "$1" = premiddlepost && '
            "/bin/printf TOKEN_R5_BACKQUOTE_OK\\n' >/tmp/r5b; /bin/sh /tmp/r5b"
        ),
        "TOKEN_R5_BACKQUOTE_OK",
    ),
    (
        "tag_18_03.rule05_substitution_recursion.arithmetic_with_parameter",
        (
            "delta=2; set -- pre$((40 + ${delta}))post; "
            'test "$#" -eq 1 && test "$1" = pre42post && '
            "/bin/printf TOKEN_R5_ARITHMETIC_OK"
        ),
        "TOKEN_R5_ARITHMETIC_OK",
    ),
    (
        "tag_18_03.rule05_substitution_recursion.parameter_command_arithmetic",
        (
            "unset missing; "
            "result=${missing:-$(/bin/printf '<%s>' \"$((20 + 22)) )\")}; "
            "test \"$result\" = '<42 )>' && "
            "/bin/printf TOKEN_R5_PARAMETER_RECURSION_OK"
        ),
        "TOKEN_R5_PARAMETER_RECURSION_OK",
    ),
    (
        "tag_18_03.rule05_substitution_recursion.nested_command_quotes",
        (
            "result=$(/bin/printf '<%s>' \"$(/bin/printf 'inner )')\"); "
            "test \"$result\" = '<inner )>' && "
            "/bin/printf TOKEN_R5_COMMAND_RECURSION_OK"
        ),
        "TOKEN_R5_COMMAND_RECURSION_OK",
    ),
    (
        "tag_18_03.rule05_substitution_recursion.physical_alias_boundary",
        (
            '/bin/sh -c \'alias x="printf hostile )"; '
            'eval "print -- \\"\\$(x physical_tail)\\""\' ; '
            'status=$?; test "$status" -eq 1 && '
            "/bin/printf TOKEN_R5_PHYSICAL_BOUNDARY_OK"
        ),
        "TOKEN_R5_PHYSICAL_BOUNDARY_OK",
    ),
    (
        "tag_18_03.rule06_operator_start.ampersand",
        "/bin/true&wait; /bin/printf TOKEN_R6_AMPERSAND_OK",
        "TOKEN_R6_AMPERSAND_OK",
    ),
    (
        "tag_18_03.rule06_operator_start.semicolon",
        "/bin/true;/bin/printf TOKEN_R6_SEMICOLON_OK",
        "TOKEN_R6_SEMICOLON_OK",
    ),
    (
        "tag_18_03.rule06_operator_start.pipe",
        (
            "/bin/printf 'V\\n'|while IFS= read -r value; do "
            'test "$value" = V && /bin/printf TOKEN_R6_PIPE_OK; done'
        ),
        "TOKEN_R6_PIPE_OK",
    ),
    (
        "tag_18_03.rule06_operator_start.less",
        (
            "/bin/printf 'V\\n' >/tmp/r6l; /bin/true</tmp/r6l&&"
            "/bin/printf TOKEN_R6_LESS_OK"
        ),
        "TOKEN_R6_LESS_OK",
    ),
    (
        "tag_18_03.rule06_operator_start.greater",
        "/bin/true>/tmp/r6g&&/bin/printf TOKEN_R6_GREATER_OK",
        "TOKEN_R6_GREATER_OK",
    ),
    (
        "tag_18_03.rule06_operator_start.left_parenthesis",
        "r6_function(){ /bin/printf TOKEN_R6_LEFT_PARENTHESIS_OK; }; r6_function",
        "TOKEN_R6_LEFT_PARENTHESIS_OK",
    ),
    (
        "tag_18_03.rule06_operator_start.right_parenthesis",
        "case x in x)/bin/printf TOKEN_R6_RIGHT_PARENTHESIS_OK;;esac",
        "TOKEN_R6_RIGHT_PARENTHESIS_OK",
    ),
    (
        "tag_18_03.rule06_operator_start.newline",
        (
            "/bin/printf '/bin/printf TOKEN_R6_NEWLINE_OK\\n' >/tmp/r6n; "
            "/bin/sh /tmp/r6n"
        ),
        "TOKEN_R6_NEWLINE_OK",
    ),
    (
        "tag_18_03.rule07_blank_delimitation.space_delimits_word",
        (
            'set -- left right; test "$#" -eq 2 && test "$1" = left && '
            'test "$2" = right && /bin/printf TOKEN_R7_SPACE_OK'
        ),
        "TOKEN_R7_SPACE_OK",
    ),
    (
        "tag_18_03.rule07_blank_delimitation.tab_delimits_word",
        (
            "/bin/printf 'set -- left\\011right\\n"
            'test "$#" -eq 2 && test "$1" = left && '
            'test "$2" = right && /bin/printf TOKEN_R7_TAB_OK\\n\' '
            ">/tmp/r7t; /bin/sh /tmp/r7t"
        ),
        "TOKEN_R7_TAB_OK",
    ),
    (
        "tag_18_03.rule07_blank_delimitation.repeated_blanks_without_token",
        (
            ' \t  set -- value; test "$#" -eq 1 && test "$1" = value && '
            "/bin/printf TOKEN_R7_REPEATED_OK"
        ),
        "TOKEN_R7_REPEATED_OK",
    ),
    (
        "tag_18_03.rule07_blank_delimitation.quoted_space_is_data",
        (
            "set -- 'left right' \"left right\" left\\ right; "
            'test "$#" -eq 3 && test "$1" = "$2" && test "$2" = "$3" && '
            "/bin/printf TOKEN_R7_QUOTED_SPACE_OK"
        ),
        "TOKEN_R7_QUOTED_SPACE_OK",
    ),
    (
        "tag_18_03.rule07_blank_delimitation.quoted_tab_is_data",
        (
            "set -- 'left\tright' \"left\tright\" left\\\tright; "
            'test "$#" -eq 3 && test "$1" = "$2" && test "$2" = "$3" && '
            "/bin/printf TOKEN_R7_QUOTED_TAB_OK"
        ),
        "TOKEN_R7_QUOTED_TAB_OK",
    ),
    (
        "tag_18_03.rule08_word_continuation.alphanumeric_and_filename",
        (
            "set -- aZ09_b-C.d/e; "
            'test "$#" -eq 1 && test "$1" = aZ09_b-C.d/e && '
            "/bin/printf TOKEN_R8_ALNUM_OK"
        ),
        "TOKEN_R8_ALNUM_OK",
    ),
    (
        "tag_18_03.rule08_word_continuation.ordinary_punctuation",
        (
            "set -f; set -- a!#%*+,-./:=?@[]^_{}~z; "
            "punctuation_count=$#; punctuation_value=$1; set +f; "
            'test "$punctuation_count" -eq 1 && '
            'test "$punctuation_value" = \'a!#%*+,-./:=?@[]^_{}~z\' && '
            "/bin/printf TOKEN_R8_PUNCTUATION_OK"
        ),
        "TOKEN_R8_PUNCTUATION_OK",
    ),
    (
        "tag_18_03.rule08_word_continuation.nonblank_controls",
        (
            "/bin/printf 'set -- a\\007\\010\\013\\014\\015z\\n"
            "expected=\\047a\\007\\010\\013\\014\\015z\\047\\n"
            'test "$#" -eq 1 && test "$1" = "$expected" && '
            "/bin/printf TOKEN_R8_CONTROL_OK\\n' >/tmp/r8c; "
            "/bin/sh /tmp/r8c"
        ),
        "TOKEN_R8_CONTROL_OK",
    ),
    (
        "tag_18_03.rule08_word_continuation.after_quoted_text",
        (
            "set -- 'left'right \"middle\"right escaped\\ valuetail; "
            'test "$#" -eq 3 && test "$1" = leftright && '
            'test "$2" = middleright && test "$3" = "escaped valuetail" && '
            "/bin/printf TOKEN_R8_QUOTED_BOUNDARY_OK"
        ),
        "TOKEN_R8_QUOTED_BOUNDARY_OK",
    ),
    (
        "tag_18_03.rule08_word_continuation.after_substitution",
        (
            "value=left; set -- ${value}right $(/bin/printf middle)right "
            "$((40 + 2))right; "
            'test "$#" -eq 3 && test "$1" = leftright && '
            'test "$2" = middleright && test "$3" = 42right && '
            "/bin/printf TOKEN_R8_SUBSTITUTION_BOUNDARY_OK"
        ),
        "TOKEN_R8_SUBSTITUTION_BOUNDARY_OK",
    ),
    (
        "tag_18_03.rule09_comment.column_zero_discards_syntax",
        (
            "/bin/printf '# \\047 \\042 $() \\134 & | ; < > text\\011tail\\n"
            "/bin/printf TOKEN_R9_COLUMN_ZERO_OK\\n' >/tmp/r9c; "
            "/bin/sh /tmp/r9c"
        ),
        "TOKEN_R9_COLUMN_ZERO_OK",
    ),
    (
        "tag_18_03.rule09_comment.leading_blanks",
        (
            "/bin/printf ' \\011  #; exit 99\\n"
            "/bin/printf TOKEN_R9_LEADING_BLANK_OK\\n' >/tmp/r9b; "
            "/bin/sh /tmp/r9b"
        ),
        "TOKEN_R9_LEADING_BLANK_OK",
    ),
    (
        "tag_18_03.rule09_comment.after_operator",
        (
            "/bin/printf ':;#; exit 99\\n"
            "/bin/printf TOKEN_R9_OPERATOR_OK\\n' >/tmp/r9o; "
            "/bin/sh /tmp/r9o"
        ),
        "TOKEN_R9_OPERATOR_OK",
    ),
    (
        "tag_18_03.rule09_comment.newline_is_not_comment",
        (
            "/bin/printf '/bin/true # ignored\\n"
            "/bin/printf TOKEN_R9_NEWLINE_OK\\n' >/tmp/r9n; "
            "/bin/sh /tmp/r9n"
        ),
        "TOKEN_R9_NEWLINE_OK",
    ),
    (
        "tag_18_03.rule09_comment.quoted_and_escaped_hash",
        (
            "set -- '#' \"#\" \\#; "
            'test "$#" -eq 3 && test "$1" = "$2" && test "$2" = "$3" && '
            "/bin/printf TOKEN_R9_QUOTED_OK"
        ),
        "TOKEN_R9_QUOTED_OK",
    ),
    (
        "tag_18_03.rule09_comment.midword_hash",
        (
            "set -- left#right name=#value; "
            'test "$#" -eq 2 && test "$1" = left#right && '
            'test "$2" = name=#value && /bin/printf TOKEN_R9_MIDWORD_OK'
        ),
        "TOKEN_R9_MIDWORD_OK",
    ),
    (
        "tag_18_03.rule10_word_start.at_input_start",
        (
            "/bin/printf '/bin/printf TOKEN_R10_INPUT_START_OK\\n' "
            ">/tmp/r10s; /bin/sh /tmp/r10s"
        ),
        "TOKEN_R10_INPUT_START_OK",
    ),
    (
        "tag_18_03.rule10_word_start.after_space",
        (
            "set -- left right; "
            'test "$#" -eq 2 && test "$2" = right && '
            "/bin/printf TOKEN_R10_SPACE_OK"
        ),
        "TOKEN_R10_SPACE_OK",
    ),
    (
        "tag_18_03.rule10_word_start.after_tab",
        (
            "/bin/printf 'set -- left\\011right\\n"
            'test "$#" -eq 2 && test "$2" = right && '
            "/bin/printf TOKEN_R10_TAB_OK\\n' >/tmp/r10t; "
            "/bin/sh /tmp/r10t"
        ),
        "TOKEN_R10_TAB_OK",
    ),
    (
        "tag_18_03.rule10_word_start.after_control_operator",
        "/bin/true;/bin/printf TOKEN_R10_CONTROL_OPERATOR_OK",
        "TOKEN_R10_CONTROL_OPERATOR_OK",
    ),
    (
        "tag_18_03.rule10_word_start.after_redirection_operator",
        (
            "/bin/printf 'V\\n'>/tmp/r10r; "
            "IFS= read -r value </tmp/r10r; "
            'test "$value" = V && /bin/printf TOKEN_R10_REDIRECTION_OK'
        ),
        "TOKEN_R10_REDIRECTION_OK",
    ),
    (
        "tag_18_03.rule10_word_start.after_newline",
        (
            "/bin/printf '/bin/true\\n"
            "/bin/printf TOKEN_R10_NEWLINE_OK\\n' >/tmp/r10n; "
            "/bin/sh /tmp/r10n"
        ),
        "TOKEN_R10_NEWLINE_OK",
    ),
    (
        "tag_18_03.rule10_word_start.after_comment_newline",
        (
            "/bin/printf '/bin/true # ignored\\n"
            "/bin/printf TOKEN_R10_COMMENT_NEWLINE_OK\\n' >/tmp/r10m; "
            "/bin/sh /tmp/r10m"
        ),
        "TOKEN_R10_COMMENT_NEWLINE_OK",
    ),
    (
        "tag_18_03.rule10_word_start.portable_punctuation",
        (
            "set -f; "
            "set -- !x %x *x +x ,x -x .x /x :x =x ?x @x [x ]x ^x _x {x }x ~x; "
            "set +f; "
            "/bin/printf '<%s>' \"$@\""
        ),
        "<!x><%x><*x><+x><,x><-x><.x></x><:x><=x><?x><@x><[x><]x><^x><_x><{x><}x><~x>",
    ),
    (
        "tag_18_03.rule10_word_start.alert_control",
        (
            "/bin/printf 'set -- \\007x\\nexpected=\\047\\007x\\047\\n"
            'test "$#" -eq 1 && test "$1" = "$expected" && '
            "/bin/printf TOKEN_R10_ALERT_OK\\n' >/tmp/r10a; /bin/sh /tmp/r10a"
        ),
        "TOKEN_R10_ALERT_OK",
    ),
    (
        "tag_18_03.rule10_word_start.backspace_control",
        (
            "/bin/printf 'set -- \\010x\\nexpected=\\047\\010x\\047\\n"
            'test "$#" -eq 1 && test "$1" = "$expected" && '
            "/bin/printf TOKEN_R10_BACKSPACE_OK\\n' >/tmp/r10b; /bin/sh /tmp/r10b"
        ),
        "TOKEN_R10_BACKSPACE_OK",
    ),
    (
        "tag_18_03.rule10_word_start.vertical_tab_control",
        (
            "/bin/printf 'set -- \\013x\\nexpected=\\047\\013x\\047\\n"
            'test "$#" -eq 1 && test "$1" = "$expected" && '
            "/bin/printf TOKEN_R10_VERTICAL_TAB_OK\\n' >/tmp/r10v; "
            "/bin/sh /tmp/r10v"
        ),
        "TOKEN_R10_VERTICAL_TAB_OK",
    ),
    (
        "tag_18_03.rule10_word_start.form_feed_control",
        (
            "/bin/printf 'set -- \\014x\\nexpected=\\047\\014x\\047\\n"
            'test "$#" -eq 1 && test "$1" = "$expected" && '
            "/bin/printf TOKEN_R10_FORM_FEED_OK\\n' >/tmp/r10f; "
            "/bin/sh /tmp/r10f"
        ),
        "TOKEN_R10_FORM_FEED_OK",
    ),
    (
        "tag_18_03.rule10_word_start.carriage_return_control",
        (
            "/bin/printf 'set -- \\015x\\nexpected=\\047\\015x\\047\\n"
            'test "$#" -eq 1 && test "$1" = "$expected" && '
            "/bin/printf TOKEN_R10_CARRIAGE_RETURN_OK\\n' >/tmp/r10c; "
            "/bin/sh /tmp/r10c"
        ),
        "TOKEN_R10_CARRIAGE_RETURN_OK",
    ),
    (
        "tag_18_03_01.a01_command_name_substitution.command_name_is_eligible",
        ("alias a01cmd='/bin/printf TOKEN_A01_COMMAND_OK'; eval a01cmd"),
        "TOKEN_A01_COMMAND_OK",
    ),
    (
        "tag_18_03_01.a01_command_name_substitution.argument_is_not_eligible",
        (
            "alias a01arg='/bin/printf TOKEN_A01_ARGUMENT_BAD'; "
            'eval \'set -- a01arg; test "$1" = a01arg && '
            "/bin/printf TOKEN_A01_ARGUMENT_OK'"
        ),
        "TOKEN_A01_ARGUMENT_OK",
    ),
    (
        "tag_18_03_01.a01_command_name_substitution.quoted_name_is_not_eligible",
        (
            "alias a01quoted='/bin/printf TOKEN_A01_QUOTED_BAD'; "
            "eval '\"a01quoted\"'; status=$?; "
            'test "$status" -ne 0 && /bin/printf TOKEN_A01_QUOTED_OK'
        ),
        "TOKEN_A01_QUOTED_OK",
    ),
    (
        "tag_18_03_01.a01_command_name_substitution.reserved_word_is_not_eligible",
        (
            "alias if='/bin/printf TOKEN_A01_RESERVED_BAD'; "
            "eval 'if /bin/true; then "
            "/bin/printf TOKEN_A01_RESERVED_OK; fi'"
        ),
        "TOKEN_A01_RESERVED_OK",
    ),
    (
        "tag_18_03_01.a01_command_name_substitution.alias_unalias_lifecycle",
        (
            "alias a01life=/bin/true; eval a01life; before=$?; "
            "unalias a01life; removed=$?; eval a01life; after=$?; "
            'test "$before" -eq 0 && test "$removed" -eq 0 && '
            'test "$after" -ne 0 && /bin/printf TOKEN_A01_LIFECYCLE_OK'
        ),
        "TOKEN_A01_LIFECYCLE_OK",
    ),
    (
        "tag_18_03_01.a01_command_name_substitution.portable_name_characters",
        ("alias 'A_9!%,@=/bin/printf TOKEN_A01_VALID_NAME_OK'; eval 'A_9!%,@'"),
        "TOKEN_A01_VALID_NAME_OK",
    ),
    (
        "tag_18_03_01.a01_command_name_substitution.recursive_loop_is_suppressed",
        (
            "alias a01loop='/bin/printf x >/tmp/alias; a01loop'; "
            "eval a01loop 2>/dev/null; status=$?; "
            "IFS= read -r value </tmp/alias; "
            'test "$status" -ne 0 && test "$value" = x && '
            "/bin/printf TOKEN_A01_RECURSION_OK"
        ),
        "TOKEN_A01_RECURSION_OK",
    ),
    (
        "tag_18_03_01.a02_trailing_blank_chain.successive_command_words",
        (
            "alias h02c='/bin/printf \"%s:%s\\n\" '; "
            "alias o02c='ONE '; alias t02c=TWO; "
            "eval 'h02c o02c t02c' >/tmp/alias; "
            "IFS= read -r value </tmp/alias; "
            'test "$value" = ONE:TWO && /bin/printf TOKEN_A02_CHAIN_OK'
        ),
        "TOKEN_A02_CHAIN_OK",
    ),
    (
        "tag_18_03_01.a02_trailing_blank_chain.invalid_name_stops_chain",
        (
            "alias h02i='/bin/printf \"%s:%s\\n\" '; alias l02i=BAD; "
            "eval 'h02i /literal l02i' >/tmp/alias; "
            "IFS= read -r value </tmp/alias; "
            'test "$value" = /literal:l02i && '
            "/bin/printf TOKEN_A02_INVALID_STOP_OK"
        ),
        "TOKEN_A02_INVALID_STOP_OK",
    ),
    (
        "tag_18_03_01.a02_trailing_blank_chain.nonblank_value_stops_chain",
        (
            "alias h02s='/bin/printf \"%s:%s\\n\" '; "
            "alias o02s=ONE; alias t02s=TWO; "
            "eval 'h02s o02s t02s' >/tmp/alias; "
            "IFS= read -r value </tmp/alias; "
            'test "$value" = ONE:t02s && '
            "/bin/printf TOKEN_A02_NONBLANK_STOP_OK"
        ),
        "TOKEN_A02_NONBLANK_STOP_OK",
    ),
    (
        "tag_18_03_01.a03_environment_noninheritance.defining_shell_retains_alias",
        ("alias a03parent='/bin/printf TOKEN_A03_PARENT_OK'; eval a03parent"),
        "TOKEN_A03_PARENT_OK",
    ),
    (
        "tag_18_03_01.a03_environment_noninheritance.separate_shell_excludes_alias",
        (
            "alias a03shell=/bin/true; "
            "/bin/sh -c 'alias a03shell >/dev/null 2>&1'; child=$?; "
            "eval a03shell; parent=$?; "
            'test "$child" -ne 0 && test "$parent" -eq 0 && '
            "/bin/printf TOKEN_A03_SEPARATE_SHELL_OK"
        ),
        "TOKEN_A03_SEPARATE_SHELL_OK",
    ),
    (
        "tag_18_03_01.a03_environment_noninheritance.utility_excludes_alias",
        (
            "/bin/printf '#!/bin/sh\nalias a03utility >/dev/null 2>&1\n"
            'test "$?" -ne 0\n\' >/tmp/alias; '
            "/bin/chmod 755 /tmp/alias; alias a03utility=/bin/false; "
            "/tmp/alias && /bin/rm -f /tmp/alias && "
            "/bin/printf TOKEN_A03_UTILITY_OK"
        ),
        "TOKEN_A03_UTILITY_OK",
    ),
    (
        "tag_18_02_01.backslash_preserves_portable_lexer_classes",
        (
            "set -- \\| \\& \\; \\< \\> \\( \\) \\$ \\` \\\\ \\\" \\' "
            "a\\ b \\# \\* \\? \\[ \\~ name\\=value \\%; "
            "/bin/printf '<%s>\\n' \"$#\"; "
            "for quoted_value; do "
            "/bin/printf '<%s>\\n' \"$quoted_value\"; done"
        ),
        (
            "<20>\n<|>\n<&>\n<;>\n<<>\n<>>\n<(>\n<)>\n<$>\n<`>\n"
            "<\\>\n<\">\n<'>\n<a b>\n<#>\n<*>\n<?>\n<[>\n<~>\n"
            "<name=value>\n<%>"
        ),
    ),
    (
        "tag_18_02_01.backslash_preserves_literal_tab",
        (
            "/bin/printf 'set -- a\\134\\011b\\n"
            'expected="a\\011b"\\n'
            'test "$1" = "$expected" && '
            '/bin/printf "%%s\\\\n" SH_2_2_1_LITERAL_TAB_OK\\n\' '
            "> /tmp/posix-bs-tab.sh; /bin/sh /tmp/posix-bs-tab.sh"
        ),
        "SH_2_2_1_LITERAL_TAB_OK",
    ),
    (
        "tag_18_02_01.backslash_newline_removal_precedes_tokenization",
        (
            "set -- left\\"
            "\n"
            'right; test "$#" -eq 1 && test "$1" = leftright && '
            "printf '%s\\n' SH_2_2_1_CONTINUATION_TOKEN_OK"
        ),
        "SH_2_2_1_CONTINUATION_TOKEN_OK",
    ),
    (
        "tag_18_02_02.single_quotes_preserve_special_and_empty_fields",
        (
            "set -- '' ' |&;<>()$`\\\" *?[#~=% '; "
            "/bin/printf '<%s>\\n' \"$#\"; "
            "/bin/printf '<%s>\\n' \"$1\"; "
            "/bin/printf '<%s>\\n' \"$2\""
        ),
        '<2>\n<>\n< |&;<>()$`\\" *?[#~=% >',
    ),
    (
        "tag_18_02_02.single_quotes_preserve_tab_and_newline",
        (
            "/bin/printf 'value=\\047left\\011middle\\012right\\047\\012"
            'expected="left\\011middle\\012right"\\012'
            'test "$value" = "$expected" && '
            '/bin/printf "%%s\\\\n" SH_2_2_2_CONTROLS_OK\\012\' '
            "> /tmp/posix-sq-controls.sh; /bin/sh /tmp/posix-sq-controls.sh"
        ),
        "SH_2_2_2_CONTROLS_OK",
    ),
    (
        "tag_18_02_02.single_quote_literal_requires_quote_boundary",
        "/bin/printf '<%s>\\n' 'left'\\''right'",
        "<left'right>",
    ),
    (
        "tag_18_02_02.single_quote_cannot_nest_syntax_rejection",
        (
            "/bin/printf '%s\\n' \"printf X 'a\\\\'b'\" >/tmp/sqi; "
            "/bin/sh /tmp/sqi >/tmp/sqo 2>/tmp/sqe; status=$?; "
            'test "$status" -ne 0 && test ! -s /tmp/sqo && '
            "/bin/printf '%s\\n' SINGLE_QUOTE_NEST_REJECT_OK"
        ),
        "SINGLE_QUOTE_NEST_REJECT_OK",
    ),
    (
        "tag_18_02_03.double_quotes_preserve_nonexception_lexer_classes",
        (
            'set -- " |&;<>()\'#{*?[~=% "; '
            "/bin/printf '<%s>\\n' \"$#\"; "
            "/bin/printf '<%s>\\n' \"$1\""
        ),
        "<1>\n< |&;<>()'#{*?[~=% >",
    ),
    (
        "tag_18_02_03.double_quotes_preserve_literal_tab_and_newline",
        (
            "/bin/printf 'set -- \\042a\\011b\\012c\\042\\012"
            "expected=\\042a\\011b\\012c\\042\\012"
            'test "$#" = 1 && test "$1" = "$expected" && '
            '/bin/printf "%%s\\\\n" DOUBLE_QUOTE_CONTROLS_OK\\012\' '
            ">/tmp/dqc; /bin/sh /tmp/dqc"
        ),
        "DOUBLE_QUOTE_CONTROLS_OK",
    ),
    (
        "tag_18_02_03.double_quote_dollar_introduces_three_expansions",
        (
            'parameter=parameter; set -- "x${parameter}y'
            '$(/bin/printf command)$((6 * 7))z"; '
            "/bin/printf '<%s>\\n' \"$#\"; "
            "/bin/printf '<%s>\\n' \"$1\""
        ),
        "<1>\n<xparameterycommand42z>",
    ),
    (
        "tag_18_02_03.double_quote_dollar_paren_recursive_matching",
        (
            "value=\"$(/bin/printf '<%s>' "
            '"$(/bin/printf \'inner ) text\')")"; '
            "/bin/printf '<%s>\\n' \"$value\""
        ),
        "<<inner ) text>>",
    ),
    (
        "tag_18_02_03.double_quote_parameter_even_quote_pairs",
        (
            'unset parameter; double_pair="${parameter:-"}"}"; '
            "single_pair=\"${parameter:-''}\"; "
            "/bin/printf '<%s>\\n' \"$double_pair\"; "
            "/bin/printf '<%s>\\n' \"$single_pair\""
        ),
        "<}>\n<''>",
    ),
    (
        "tag_18_02_03.double_quote_parameter_escaped_braces",
        (
            'unset parameter; escaped_close="${parameter:-\\}suffix}"; '
            'escaped_open="${parameter:-\\{suffix}"; '
            "/bin/printf '<%s>\\n' \"$escaped_close\"; "
            "/bin/printf '<%s>\\n' \"$escaped_open\""
        ),
        "<}suffix>\n<\\{suffix>",
    ),
    (
        "tag_18_02_03.backquote_substitution_honors_escaped_delimiter",
        ("value=\"`/bin/printf '%s' '\\`'`\"; /bin/printf '<%s>\\n' \"$value\""),
        "<`>",
    ),
    (
        "tag_18_02_03.double_quote_backslash_whitelist_and_other",
        (
            'set -- "\\$" "\\`" "\\"" "\\\\" "\\q"; '
            "/bin/printf '<%s>\\n' \"$#\"; "
            "for value; do /bin/printf '<%s>\\n' \"$value\"; done"
        ),
        '<5>\n<$>\n<`>\n<">\n<\\>\n<\\q>',
    ),
    (
        "tag_18_02_03.double_quote_backslash_newline_continuation",
        (
            'set -- "left\\'
            "\n"
            'right"; test "$#" -eq 1 && test "$1" = leftright && '
            "/bin/printf '%s\\n' DOUBLE_QUOTE_CONTINUATION_OK"
        ),
        "DOUBLE_QUOTE_CONTINUATION_OK",
    ),
    (
        "tag_18_02_03.double_quote_special_at_field_cardinality",
        (
            'set --; set -- "$@"; /bin/printf \'<zero:%s>\\n\' "$#"; '
            'set -- "a b" "" c; set -- "$@"; '
            "/bin/printf '<many:%s>\\n' \"$#\"; "
            'for value in "$@"; do /bin/printf \'<%s>\\n\' "$value"; done'
        ),
        "<zero:0>\n<many:3>\n<a b>\n<>\n<c>",
    ),
)

SHELL_GRAMMAR_COMMAND_CASES = (
    (
        "tag_18_10_01.lex01_operator.complete_operator_identifiers",
        (
            ": >/tmp/grammar-operator.sh\n"
            "/bin/printf '%s\\n' "
            "'if /bin/true && ! false; then :' "
            "'else exit 1; fi' "
            "'false || /bin/true' "
            "'/bin/printf A | { IFS= read -r pipe_value; "
            'test "$pipe_value" = A; }\' '
            "'case x in x) :;; esac' "
            ">>/tmp/grammar-operator.sh\n"
            "/bin/printf '%s\\n' "
            "'/bin/true & background_pid=$!; wait \"$background_pid\"' "
            "'(/bin/true)' "
            "'/bin/printf first >/tmp/grammar-operator-file' "
            "'/bin/printf second >>/tmp/grammar-operator-file' "
            ">>/tmp/grammar-operator.sh\n"
            "/bin/printf '%s\\n' "
            "'exec 3</tmp/grammar-operator-file' "
            "'IFS= read -r input_value <&3; exec 3<&-' "
            "'exec 4>/tmp/grammar-operator-dup' "
            "'/bin/printf duplicate >&4; exec 4>&-' "
            ">>/tmp/grammar-operator.sh\n"
            "/bin/printf '%s\\n' "
            "'exec 5<>/tmp/grammar-operator-file' "
            "'IFS= read -r read_write_value <&5; exec 5>&-' "
            "'set -C; /bin/printf clobber >|/tmp/grammar-operator-file; set +C' "
            ">>/tmp/grammar-operator.sh\n"
            "/bin/printf '%s\\n' "
            "'IFS= read -r here_value <<END_NORMAL' "
            "'normal' 'END_NORMAL' "
            "'IFS= read -r tab_value <<-END_TAB' "
            ">>/tmp/grammar-operator.sh\n"
            "/bin/printf '\\011%s\\n' tabbed END_TAB "
            ">>/tmp/grammar-operator.sh\n"
            "/bin/printf '%s\\n' "
            "'test \"$input_value\" = firstsecond' "
            "'test \"$read_write_value\" = clobber' "
            "'test \"$here_value\" = normal' "
            "'test \"$tab_value\" = tabbed' "
            "'/bin/printf GRAMMAR_LEX_OPERATOR_OK' "
            ">>/tmp/grammar-operator.sh\n"
            "/bin/sh /tmp/grammar-operator.sh"
        ),
        "GRAMMAR_LEX_OPERATOR_OK",
    ),
    (
        "tag_18_10_01.lex02_io_number.redirection_delimiter_matrix",
        (
            ": >/tmp/grammar-io-number.sh\n"
            "/bin/printf '%s\\n' "
            "'/bin/printf input >/tmp/grammar-io-input' "
            "'exec 3</tmp/grammar-io-input' "
            "'IFS= read -r less_value <&3; exec 3<&-' "
            "'exec 4>/tmp/grammar-io-output' "
            "'/bin/printf greater >&4; exec 4>&-' "
            ">>/tmp/grammar-io-number.sh\n"
            "/bin/printf '%s\\n' "
            "'{ IFS= read -r dless_value <&5; } 5<<END_DLESS' "
            "'dless' 'END_DLESS' "
            "'exec 6>>/tmp/grammar-io-append' "
            "'/bin/printf append >&6; exec 6>&-' "
            ">>/tmp/grammar-io-number.sh\n"
            "/bin/printf '%s\\n' "
            "'exec 3</tmp/grammar-io-input' "
            "'IFS= read -r lessand_value 7<&3 <&7; exec 7<&-; exec 3<&-' "
            "'exec 4>/tmp/grammar-io-dup-output' "
            "'/bin/printf greatand 8>&4 >&8; exec 8>&-; exec 4>&-' "
            ">>/tmp/grammar-io-number.sh\n"
            "/bin/printf '%s\\n' "
            "'exec 9<>/tmp/grammar-io-input' "
            "'IFS= read -r lessgreat_value <&9; exec 9>&-' "
            "'{ IFS= read -r dlessdash_value <&10; } 10<<-END_DLESSDASH' "
            ">>/tmp/grammar-io-number.sh\n"
            "/bin/printf '\\011%s\\n' dlessdash END_DLESSDASH "
            ">>/tmp/grammar-io-number.sh\n"
            "/bin/printf '%s\\n' "
            "'set -C; /bin/printf clobber 11>|/tmp/grammar-io-output; set +C' "
            "'/bin/printf \"<%s>\" 12 >/tmp/grammar-io-separated' "
            "'/bin/printf \"<%s>\" 12x>/tmp/grammar-io-nondigit' "
            ">>/tmp/grammar-io-number.sh\n"
            "/bin/printf '%s\\n' "
            "'exec 12</tmp/grammar-io-separated' "
            "'IFS= read -r separated_value <&12; exec 12<&-' "
            "'exec 13</tmp/grammar-io-nondigit' "
            "'IFS= read -r nondigit_value <&13; exec 13<&-' "
            ">>/tmp/grammar-io-number.sh\n"
            "/bin/printf '%s\\n' "
            "'test \"$less_value\" = input' "
            "'test \"$dless_value\" = dless' "
            "'test \"$lessand_value\" = input' "
            "'test \"$lessgreat_value\" = input' "
            "'test \"$dlessdash_value\" = dlessdash' "
            ">>/tmp/grammar-io-number.sh\n"
            "/bin/printf '%s\\n' "
            '\'test "$separated_value" = "<12>"\' '
            '\'test "$nondigit_value" = "<12x>"\' '
            "'/bin/printf GRAMMAR_LEX_IO_NUMBER_OK' "
            ">>/tmp/grammar-io-number.sh\n"
            "/bin/sh /tmp/grammar-io-number.sh"
        ),
        "GRAMMAR_LEX_IO_NUMBER_OK",
    ),
    (
        "tag_18_10_01.lex03_token.nonoperator_token_matrix",
        (
            "set -- word _name9 A=value '&&' 123x 456; "
            'test "$#" -eq 6 && test "$1" = word && '
            'test "$2" = _name9 && test "$3" = A=value && '
            'test "$4" = "&&" && test "$5" = 123x && '
            'test "$6" = 456 && /bin/printf GRAMMAR_LEX_TOKEN_OK'
        ),
        "GRAMMAR_LEX_TOKEN_OK",
    ),
    (
        "tag_18_10_02.rule01_command_name.reserved_word_contexts",
        (
            ": >/tmp/grammar-rule1.sh\n"
            "/bin/printf '%s\\n' "
            "'if false; then exit 1; elif /bin/true; then :' "
            "'else exit 1; fi' "
            "'set -- item; for value in item; do test \"$value\" = item; done' "
            "'case item in item) :;; esac' "
            ">>/tmp/grammar-rule1.sh\n"
            "/bin/printf '%s\\n' "
            "'count=0; while test \"$count\" -lt 1; do count=$((count + 1)); done' "
            "'until test \"$count\" -eq 1; do exit 1; done' "
            "'{ :; }' "
            "'! false' "
            ">>/tmp/grammar-rule1.sh\n"
            "/bin/printf '%s\\n' "
            "'set -- if then else elif fi do done case esac while until for { } ! in' "
            '\'test "$#" -eq 16 && test "$1" = if\' '
            "'shift 15; test \"$1\" = in' "
            "'/bin/printf GRAMMAR_CONTEXT_RULE1_RESERVED_OK' "
            ">>/tmp/grammar-rule1.sh\n"
            "/bin/sh /tmp/grammar-rule1.sh"
        ),
        "GRAMMAR_CONTEXT_RULE1_RESERVED_OK",
    ),
    (
        "tag_18_10_02.rule01_command_name.quoted_reserved_word_is_word",
        (
            "/bin/sh -c '\"if\"' >/tmp/grammar-rule1-error 2>&1; "
            'quoted_status=$?; test "$quoted_status" -ne 0 && '
            "/bin/printf GRAMMAR_CONTEXT_RULE1_QUOTED_OK"
        ),
        "GRAMMAR_CONTEXT_RULE1_QUOTED_OK",
    ),
    (
        "tag_18_10_02.rule01_command_name.line_joining_precedes_classification",
        (
            r"/bin/printf 'i\\\012f /bin/true; then /bin/printf "
            "GRAMMAR_CONTEXT_RULE1_JOIN_OK; fi\\n' "
            ">/tmp/grammar-rule1-join.sh; /bin/sh /tmp/grammar-rule1-join.sh"
        ),
        "GRAMMAR_CONTEXT_RULE1_JOIN_OK",
    ),
    (
        "tag_18_10_02.rule02_redirection_filename."
        "expansion_and_noninteractive_pathname",
        (
            ": >/tmp/grammar-rule2.sh\n"
            "/bin/printf '%s\\n' "
            "'HOME=/tmp' "
            "'parameter_path=\"/tmp/grammar-rule2 parameter\"' "
            "': >~/grammar-rule2-tilde' "
            "': >$parameter_path' "
            ">>/tmp/grammar-rule2.sh\n"
            "/bin/printf '%s\\n' "
            "': >$(/bin/printf /tmp/grammar-rule2-command)' "
            "': >/tmp/grammar-rule2-$((6 * 7))' "
            "': >\"/tmp/grammar-rule2-quoted\"' "
            ">>/tmp/grammar-rule2.sh\n"
            "/bin/printf '%s\\n' "
            "': >/tmp/grammar-rule2-glob-a' "
            "': >/tmp/grammar-rule2-glob-b' "
            "': >/tmp/grammar-rule2-glob-*' "
            ">>/tmp/grammar-rule2.sh\n"
            "/bin/printf '%s\\n' "
            "'test -e /tmp/grammar-rule2-tilde' "
            "'test -e \"$parameter_path\"' "
            "'test -e /tmp/grammar-rule2-command' "
            ">>/tmp/grammar-rule2.sh\n"
            "/bin/printf '%s\\n' "
            "'test -e /tmp/grammar-rule2-42' "
            "'test -e /tmp/grammar-rule2-quoted' "
            "'test -e \"/tmp/grammar-rule2-glob-*\"' "
            "'/bin/printf GRAMMAR_CONTEXT_RULE2_OK' "
            ">>/tmp/grammar-rule2.sh\n"
            "/bin/sh /tmp/grammar-rule2.sh"
        ),
        "GRAMMAR_CONTEXT_RULE2_OK",
    ),
    (
        "tag_18_10_02.rule03_here_document.quote_removed_delimiter_matrix",
        (
            ": >/tmp/grammar-rule3.sh\n"
            "/bin/printf '%s\\n' "
            "'IFS= read -r value_one <<\\END_ONE' 'one' 'END_ONE' "
            "'IFS= read -r value_two <<\"END_TWO\"' 'two' 'END_TWO' "
            "'IFS= read -r value_three <<E\"ND_THREE\"' 'three' 'END_THREE' "
            ">>/tmp/grammar-rule3.sh\n"
            "/bin/printf '%s\\n' "
            "'test \"$value_one\" = one' 'test \"$value_two\" = two' "
            "'test \"$value_three\" = three' "
            "'/bin/printf GRAMMAR_CONTEXT_RULE3_OK' "
            ">>/tmp/grammar-rule3.sh\n"
            "/bin/sh /tmp/grammar-rule3.sh"
        ),
        "GRAMMAR_CONTEXT_RULE3_OK",
    ),
    (
        "tag_18_10_02.rule04_case_termination.esac_context_boundary",
        (
            "/bin/rm -f /tmp/grammar-rule4-bad; "
            "/bin/sh -c 'case esac in esac) "
            "/bin/printf bad >/tmp/grammar-rule4-bad;; esac' "
            ">/tmp/grammar-rule4-error 2>&1; rule4_invalid_status=$?\n"
            "case esac in esac; case esac in 'esac') printf Q;; esac; "
            "case esac in e\\sac) printf E;; esac; "
            "case esac in no|esac) printf A;; esac; "
            'test "$rule4_invalid_status" -ne 0 && '
            "test ! -e /tmp/grammar-rule4-bad && "
            "printf GRAMMAR_CONTEXT_RULE4_OK"
        ),
        "QEAGRAMMAR_CONTEXT_RULE4_OK",
    ),
    (
        "tag_18_10_02.rule05_for_name.valid_and_invalid_name_matrix",
        (
            'for _ in value; do test "$_" = value; done; '
            'for _A9 in value; do test "$_A9" = value; done; '
            'for a0 in value; do test "$a0" = value; done; '
            "\n"
            "/bin/sh -c 'for 9bad in x; do :; done' "
            ">/tmp/grammar-rule5-error 2>&1; status_digit=$?; "
            "\n"
            "/bin/sh -c 'for bad-name in x; do :; done' "
            ">/tmp/grammar-rule5-error 2>&1; status_hyphen=$?; "
            "\n"
            "/bin/sh -c 'for \"good\" in x; do :; done' "
            ">/tmp/grammar-rule5-error 2>&1; status_quoted=$?; "
            "\n"
            'test "$status_digit" -ne 0 && test "$status_hyphen" -ne 0 && '
            'test "$status_quoted" -ne 0 && '
            "/bin/printf GRAMMAR_CONTEXT_RULE5_OK"
        ),
        "GRAMMAR_CONTEXT_RULE5_OK",
    ),
    (
        "tag_18_10_02.rule06a_case_in.in_context_boundary",
        (
            "/bin/printf '%s\\n' 'case value' 'in' "
            "'value) /bin/printf GRAMMAR_CONTEXT_RULE6A_VALID_OK;;' 'esac' "
            ">/tmp/grammar-rule6a.sh; /bin/sh /tmp/grammar-rule6a.sh; "
            "\n"
            "/bin/sh -c 'case value inx value) :;; esac' "
            ">/tmp/grammar-rule6a-error 2>&1; status_word=$?; "
            "\n"
            "/bin/sh -c 'case value \"in\" value) :;; esac' "
            ">/tmp/grammar-rule6a-error 2>&1; status_quoted=$?; "
            "\n"
            'test "$status_word" -ne 0 && test "$status_quoted" -ne 0 && '
            "/bin/printf GRAMMAR_CONTEXT_RULE6A_INVALID_OK"
        ),
        "GRAMMAR_CONTEXT_RULE6A_VALID_OKGRAMMAR_CONTEXT_RULE6A_INVALID_OK",
    ),
    (
        "tag_18_10_02.rule06b_for_in_do.in_do_context_boundary",
        (
            'set -- positional; for value do test "$value" = positional; done; '
            "\n"
            "/bin/printf '%s\\n' 'for named' 'in one two' 'do' "
            "'test -n \"$named\"' 'done' "
            "'/bin/printf GRAMMAR_CONTEXT_RULE6B_VALID_OK' "
            ">/tmp/grammar-rule6b.sh; /bin/sh /tmp/grammar-rule6b.sh; "
            "\n"
            "/bin/sh -c 'for value inx one; do :; done' "
            ">/tmp/grammar-rule6b-error 2>&1; status_word=$?; "
            "\n"
            "/bin/sh -c 'for value \"in\" one; do :; done' "
            ">/tmp/grammar-rule6b-error 2>&1; status_in=$?; "
            "\n"
            "/bin/sh -c 'for value \"do\" :; done' "
            ">/tmp/grammar-rule6b-error 2>&1; status_do=$?; "
            "\n"
            'test "$status_word" -ne 0 && test "$status_in" -ne 0 && '
            'test "$status_do" -ne 0 && '
            "/bin/printf GRAMMAR_CONTEXT_RULE6B_INVALID_OK"
        ),
        "GRAMMAR_CONTEXT_RULE6B_VALID_OKGRAMMAR_CONTEXT_RULE6B_INVALID_OK",
    ),
    (
        "tag_18_10_02.rule07a_assignment_first.first_word_dispatch",
        (
            "GRAMMAR_RULE7A_VALUE=outer; GRAMMAR_RULE7A_VALUE=prefix; "
            'test "$GRAMMAR_RULE7A_VALUE" = prefix; '
            "GRAMMAR_RULE7A_VALUE=environment /bin/sh -c "
            "'test \"$GRAMMAR_RULE7A_VALUE\" = environment'; "
            'test "$GRAMMAR_RULE7A_VALUE" = prefix; '
            "\n"
            "/bin/sh -c '=invalid' >/tmp/grammar-rule7a-error 2>&1; "
            "status_leading=$?; "
            "\n"
            "/bin/sh -c 'NAME\"=\"invalid' "
            ">/tmp/grammar-rule7a-error 2>&1; status_quoted=$?; "
            "\n"
            'test "$status_leading" -ne 0 && test "$status_quoted" -ne 0 && '
            "/bin/printf GRAMMAR_CONTEXT_RULE7A_OK"
        ),
        "GRAMMAR_CONTEXT_RULE7A_OK",
    ),
    (
        "tag_18_10_02.rule07b_assignment_later.assignment_and_word_boundary",
        (
            "GRAMMAR_RULE7B_VALUE=outer; "
            ">/tmp/grammar-rule7b-output GRAMMAR_RULE7B_VALUE=inner "
            '/bin/sh -c \'/bin/printf "%s" "$GRAMMAR_RULE7B_VALUE"\'; '
            "\n"
            "exec 3</tmp/grammar-rule7b-output; "
            "IFS= read -r environment_value <&3; exec 3<&-; "
            'test "$environment_value" = inner && '
            'test "$GRAMMAR_RULE7B_VALUE" = outer; '
            "\n"
            "set -- GRAMMAR_RULE7B_VALUE=argument; "
            'test "$1" = GRAMMAR_RULE7B_VALUE=argument; '
            "\n"
            "unset GRAMMAR_RULE7B_COMMAND; "
            "${GRAMMAR_RULE7B_COMMAND:=/bin/true}; expansion_status=$?; "
            "\n"
            'test "$expansion_status" -eq 0 && '
            'test "$GRAMMAR_RULE7B_COMMAND" = /bin/true && '
            "/bin/printf GRAMMAR_CONTEXT_RULE7B_OK"
        ),
        "GRAMMAR_CONTEXT_RULE7B_OK",
    ),
    (
        "tag_18_10_02.rule08_function_name.name_dispatch_matrix",
        (
            "_(){ :; }; _A9(){ :; }; a0(){ :; }; _; _A9; a0; "
            "\n"
            "/bin/sh -c '9bad() { :; }' >/tmp/grammar-rule8-error 2>&1; "
            "status_digit=$?; "
            "\n"
            "/bin/sh -c 'bad-name() { :; }' >/tmp/grammar-rule8-error 2>&1; "
            "status_hyphen=$?; "
            "\n"
            "/bin/sh -c '\"good\"() { :; }' >/tmp/grammar-rule8-error 2>&1; "
            "status_quoted=$?; "
            "\n"
            "/bin/sh -c 'if() { :; }' >/tmp/grammar-rule8-error 2>&1; "
            "status_reserved=$?; "
            "\n"
            "/bin/sh -c 'name=value() { :; }' "
            ">/tmp/grammar-rule8-error 2>&1; status_assignment=$?; "
            "\n"
            'test "$status_digit" -ne 0 && test "$status_hyphen" -ne 0 && '
            'test "$status_quoted" -ne 0 && test "$status_reserved" -ne 0 && '
            'test "$status_assignment" -ne 0 && '
            "/bin/printf GRAMMAR_CONTEXT_RULE8_OK"
        ),
        "GRAMMAR_CONTEXT_RULE8_OK",
    ),
    (
        "tag_18_10_02.rule09_function_body.deferred_expansion_and_assignment",
        (
            "GRAMMAR_RULE9_VALUE=parse; rm -f /tmp/grammar-rule9-side; "
            "grammar_rule9_function(){ "
            "GRAMMAR_RULE9_RESULT=$(/bin/printf side >/tmp/grammar-rule9-side; "
            "/bin/printf '%s' \"$GRAMMAR_RULE9_VALUE\"); }; "
            "\n"
            "test ! -e /tmp/grammar-rule9-side; "
            "GRAMMAR_RULE9_VALUE=execute; grammar_rule9_function; "
            "test -e /tmp/grammar-rule9-side && "
            'test "$GRAMMAR_RULE9_RESULT" = execute && '
            "/bin/printf GRAMMAR_CONTEXT_RULE9_DEFERRED_OK"
        ),
        "GRAMMAR_CONTEXT_RULE9_DEFERRED_OK",
    ),
    (
        "tag_18_10_02.rule09_function_body.compound_body_matrix",
        (
            "GRAMMAR_RULE9_BODY=parse; "
            'grammar_rule9_brace(){ test "$GRAMMAR_RULE9_BODY" = execute; }; '
            "\n"
            'grammar_rule9_subshell()(test "$GRAMMAR_RULE9_BODY" = execute); '
            "\n"
            "grammar_rule9_for() for value in one; do "
            'test "$GRAMMAR_RULE9_BODY" = execute; done; '
            "\n"
            "grammar_rule9_case() case value in value) "
            'test "$GRAMMAR_RULE9_BODY" = execute;; esac; '
            "\n"
            "grammar_rule9_if() if /bin/true; then "
            'test "$GRAMMAR_RULE9_BODY" = execute; fi; '
            "\n"
            "grammar_rule9_while() while "
            'test "$GRAMMAR_RULE9_BODY" = parse; do return 1; done; '
            "\n"
            "grammar_rule9_until() until "
            'test "$GRAMMAR_RULE9_BODY" = execute; do return 1; done; '
            "\n"
            "GRAMMAR_RULE9_BODY=execute; "
            "grammar_rule9_brace && grammar_rule9_subshell && "
            "grammar_rule9_for && grammar_rule9_case && grammar_rule9_if && "
            "grammar_rule9_while && grammar_rule9_until && "
            "/bin/printf GRAMMAR_CONTEXT_RULE9_BODY_OK"
        ),
        "GRAMMAR_CONTEXT_RULE9_BODY_OK",
    ),
    (
        "tag_18_10_02.grammar.program.alt01.leading_and_trailing_linebreak",
        "/bin/printf '\\n/bin/printf GRAMMAR_PROGRAM_ALT01_OK\\n\\n' "
        ">/tmp/grammar-program-alt01; /bin/sh /tmp/grammar-program-alt01",
        "GRAMMAR_PROGRAM_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.program.alt02.linebreak_only",
        "/bin/printf '\\n\\n' >/tmp/grammar-program-alt02; "
        "/bin/sh /tmp/grammar-program-alt02 && "
        "/bin/printf GRAMMAR_PROGRAM_ALT02_OK",
        "GRAMMAR_PROGRAM_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.complete_commands.alt01.recursive_newline_list",
        "/bin/printf ':\\n/bin/printf GRAMMAR_COMPLETE_COMMANDS_ALT01_OK\\n' "
        ">/tmp/grammar-complete-commands-alt01; "
        "/bin/sh /tmp/grammar-complete-commands-alt01",
        "GRAMMAR_COMPLETE_COMMANDS_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.complete_commands.alt02.single_complete_command",
        "/bin/printf '/bin/printf GRAMMAR_COMPLETE_COMMANDS_ALT02_OK\\n' "
        ">/tmp/grammar-complete-commands-alt02; "
        "/bin/sh /tmp/grammar-complete-commands-alt02",
        "GRAMMAR_COMPLETE_COMMANDS_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.complete_command.alt01.trailing_separator",
        "/bin/printf '/bin/printf GRAMMAR_COMPLETE_COMMAND_ALT01_OK;' "
        ">/tmp/grammar-complete-command-alt01; "
        "/bin/sh /tmp/grammar-complete-command-alt01",
        "GRAMMAR_COMPLETE_COMMAND_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.complete_command.alt02.list_at_end_of_input",
        "/bin/printf '/bin/printf GRAMMAR_COMPLETE_COMMAND_ALT02_OK' "
        ">/tmp/grammar-complete-command-alt02; "
        "/bin/sh /tmp/grammar-complete-command-alt02",
        "GRAMMAR_COMPLETE_COMMAND_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.list.alt01.recursive_separator_and_or",
        "/bin/true; /bin/printf GRAMMAR_LIST_ALT01_OK",
        "GRAMMAR_LIST_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.list.alt02.single_and_or",
        "/bin/printf GRAMMAR_LIST_ALT02_OK",
        "GRAMMAR_LIST_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.and_or.alt01.single_pipeline",
        "/bin/printf GRAMMAR_AND_OR_ALT01_OK",
        "GRAMMAR_AND_OR_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.and_or.alt02.and_if_with_linebreak",
        "/bin/printf '/bin/true &&\\n/bin/printf GRAMMAR_AND_OR_ALT02_OK\\n' "
        ">/tmp/grammar-and-or-alt02; /bin/sh /tmp/grammar-and-or-alt02",
        "GRAMMAR_AND_OR_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.and_or.alt03.or_if_with_linebreak",
        "/bin/printf 'false ||\\n/bin/printf GRAMMAR_AND_OR_ALT03_OK\\n' "
        ">/tmp/grammar-and-or-alt03; /bin/sh /tmp/grammar-and-or-alt03",
        "GRAMMAR_AND_OR_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.pipeline.alt01.single_pipe_sequence",
        "/bin/printf GRAMMAR_PIPELINE_ALT01_OK",
        "GRAMMAR_PIPELINE_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.pipeline.alt02.negated_pipe_sequence",
        "! false && /bin/printf GRAMMAR_PIPELINE_ALT02_OK",
        "GRAMMAR_PIPELINE_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.pipe_sequence.alt01.single_command",
        "/bin/printf GRAMMAR_PIPE_SEQUENCE_ALT01_OK",
        "GRAMMAR_PIPE_SEQUENCE_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.pipe_sequence.alt02.recursive_pipe_linebreak",
        "/bin/printf '/bin/printf value |\\n{ IFS= read -r value; "
        "test \"$value\" = value; } && "
        "/bin/printf GRAMMAR_PIPE_SEQUENCE_ALT02_OK\\n' "
        ">/tmp/grammar-pipe-sequence-alt02; "
        "/bin/sh /tmp/grammar-pipe-sequence-alt02",
        "GRAMMAR_PIPE_SEQUENCE_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.compound_list.alt01.linebreak_term",
        "/bin/printf '(\\n/bin/printf GRAMMAR_COMPOUND_LIST_ALT01_OK)' "
        ">/tmp/grammar-compound-list-alt01; "
        "/bin/sh /tmp/grammar-compound-list-alt01",
        "GRAMMAR_COMPOUND_LIST_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.compound_list.alt02.term_separator",
        "/bin/printf '(\\n/bin/printf GRAMMAR_COMPOUND_LIST_ALT02_OK;\\n)' "
        ">/tmp/grammar-compound-list-alt02; "
        "/bin/sh /tmp/grammar-compound-list-alt02",
        "GRAMMAR_COMPOUND_LIST_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.term.alt01.recursive_separator_and_or",
        "/bin/printf '(/bin/true; "
        "/bin/printf GRAMMAR_TERM_ALT01_OK)' "
        ">/tmp/grammar-term-alt01; /bin/sh /tmp/grammar-term-alt01",
        "GRAMMAR_TERM_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.term.alt02.single_and_or",
        "/bin/printf '(/bin/printf GRAMMAR_TERM_ALT02_OK)' "
        ">/tmp/grammar-term-alt02; /bin/sh /tmp/grammar-term-alt02",
        "GRAMMAR_TERM_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.newline_list.alt01.single_newline",
        "/bin/printf ':\\n/bin/printf GRAMMAR_NEWLINE_LIST_ALT01_OK\\n' "
        ">/tmp/grammar-newline-list-alt01; "
        "/bin/sh /tmp/grammar-newline-list-alt01",
        "GRAMMAR_NEWLINE_LIST_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.newline_list.alt02.multiple_newlines",
        "/bin/printf ':\\n\\n/bin/printf GRAMMAR_NEWLINE_LIST_ALT02_OK\\n' "
        ">/tmp/grammar-newline-list-alt02; "
        "/bin/sh /tmp/grammar-newline-list-alt02",
        "GRAMMAR_NEWLINE_LIST_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.linebreak.alt01.newline_list",
        "/bin/printf '/bin/true &&\\n/bin/printf GRAMMAR_LINEBREAK_ALT01_OK\\n' "
        ">/tmp/grammar-linebreak-alt01; /bin/sh /tmp/grammar-linebreak-alt01",
        "GRAMMAR_LINEBREAK_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.linebreak.alt02.empty_linebreak",
        "/bin/true && /bin/printf GRAMMAR_LINEBREAK_ALT02_OK",
        "GRAMMAR_LINEBREAK_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.separator_op.alt01.ampersand",
        "/bin/true & background_pid=$!; wait \"$background_pid\"; "
        "/bin/printf GRAMMAR_SEPARATOR_OP_ALT01_OK",
        "GRAMMAR_SEPARATOR_OP_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.separator_op.alt02.semicolon",
        "/bin/true; /bin/printf GRAMMAR_SEPARATOR_OP_ALT02_OK",
        "GRAMMAR_SEPARATOR_OP_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.separator.alt01.operator_linebreak",
        "/bin/printf '(/bin/true;\\n"
        "/bin/printf GRAMMAR_SEPARATOR_ALT01_OK)' "
        ">/tmp/grammar-separator-alt01; /bin/sh /tmp/grammar-separator-alt01",
        "GRAMMAR_SEPARATOR_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.separator.alt02.newline_separator",
        "/bin/printf '(/bin/true\\n"
        "/bin/printf GRAMMAR_SEPARATOR_ALT02_OK)' "
        ">/tmp/grammar-separator-alt02; /bin/sh /tmp/grammar-separator-alt02",
        "GRAMMAR_SEPARATOR_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.sequential_sep.alt01.semicolon_linebreak",
        "/bin/printf 'for value in one;\\ndo\\n"
        "/bin/printf GRAMMAR_SEQUENTIAL_SEP_ALT01_OK\\ndone\\n' "
        ">/tmp/grammar-sequential-sep-alt01; "
        "/bin/sh /tmp/grammar-sequential-sep-alt01",
        "GRAMMAR_SEQUENTIAL_SEP_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.sequential_sep.alt02.newline_list",
        "/bin/printf 'for value in one\\ndo\\n"
        "/bin/printf GRAMMAR_SEQUENTIAL_SEP_ALT02_OK\\ndone\\n' "
        ">/tmp/grammar-sequential-sep-alt02; "
        "/bin/sh /tmp/grammar-sequential-sep-alt02",
        "GRAMMAR_SEQUENTIAL_SEP_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.command.alt01.simple_command",
        "/bin/printf GRAMMAR_COMMAND_ALT01_OK",
        "GRAMMAR_COMMAND_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.command.alt02.compound_command",
        "{ /bin/printf GRAMMAR_COMMAND_ALT02_OK; }",
        "GRAMMAR_COMMAND_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.command.alt03.compound_command_redirect_list",
        "{ /bin/printf '%s\\n' payload; } >/tmp/grammar-command-alt03; "
        "IFS= read -r value </tmp/grammar-command-alt03; "
        "test \"$value\" = payload && /bin/printf GRAMMAR_COMMAND_ALT03_OK",
        "GRAMMAR_COMMAND_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.command.alt04.function_definition",
        "grammar_command_alt04(){ :; }; grammar_command_alt04 && "
        "/bin/printf GRAMMAR_COMMAND_ALT04_OK",
        "GRAMMAR_COMMAND_ALT04_OK",
    ),
    (
        "tag_18_10_02.grammar.compound_command.alt01.brace_group",
        "{ /bin/printf GRAMMAR_COMPOUND_COMMAND_ALT01_OK; }",
        "GRAMMAR_COMPOUND_COMMAND_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.compound_command.alt02.subshell",
        "(/bin/printf GRAMMAR_COMPOUND_COMMAND_ALT02_OK)",
        "GRAMMAR_COMPOUND_COMMAND_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.compound_command.alt03.for_clause",
        "for value in one; do test \"$value\" = one && "
        "/bin/printf GRAMMAR_COMPOUND_COMMAND_ALT03_OK; done",
        "GRAMMAR_COMPOUND_COMMAND_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.compound_command.alt04.case_clause",
        "case one in one) /bin/printf GRAMMAR_COMPOUND_COMMAND_ALT04_OK;; esac",
        "GRAMMAR_COMPOUND_COMMAND_ALT04_OK",
    ),
    (
        "tag_18_10_02.grammar.compound_command.alt05.if_clause",
        "if /bin/true; then "
        "/bin/printf GRAMMAR_COMPOUND_COMMAND_ALT05_OK; fi",
        "GRAMMAR_COMPOUND_COMMAND_ALT05_OK",
    ),
    (
        "tag_18_10_02.grammar.compound_command.alt06.while_clause",
        "count=0; while test \"$count\" -eq 0; do count=1; "
        "/bin/printf GRAMMAR_COMPOUND_COMMAND_ALT06_OK; done",
        "GRAMMAR_COMPOUND_COMMAND_ALT06_OK",
    ),
    (
        "tag_18_10_02.grammar.compound_command.alt07.until_clause",
        "count=0; until test \"$count\" -eq 1; do count=1; "
        "/bin/printf GRAMMAR_COMPOUND_COMMAND_ALT07_OK; done",
        "GRAMMAR_COMPOUND_COMMAND_ALT07_OK",
    ),
    (
        "tag_18_10_02.grammar.subshell.alt01.environment_isolation",
        "subshell_value=outer; "
        "(subshell_value=inner; test \"$subshell_value\" = inner) && "
        "test \"$subshell_value\" = outer && "
        "/bin/printf GRAMMAR_SUBSHELL_ALT01_OK",
        "GRAMMAR_SUBSHELL_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.for_clause.alt01.default_positional_list",
        "set -- default; for value do test \"$value\" = default && "
        "/bin/printf GRAMMAR_FOR_CLAUSE_ALT01_OK; done",
        "GRAMMAR_FOR_CLAUSE_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.for_clause.alt02.omitted_in_semicolon",
        "set -- default; for value; do test \"$value\" = default && "
        "/bin/printf GRAMMAR_FOR_CLAUSE_ALT02_OK; done",
        "GRAMMAR_FOR_CLAUSE_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.for_clause.alt03.empty_in_wordlist",
        "grammar_for_alt03_ran=no; for value in; do "
        "grammar_for_alt03_ran=yes; done; "
        "test \"$grammar_for_alt03_ran\" = no && "
        "/bin/printf GRAMMAR_FOR_CLAUSE_ALT03_OK",
        "GRAMMAR_FOR_CLAUSE_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.for_clause.alt04.explicit_wordlist",
        "grammar_for_alt04_count=0; for value in one two; do "
        "grammar_for_alt04_count=$((grammar_for_alt04_count + 1)); done; "
        "test \"$grammar_for_alt04_count\" -eq 2 && "
        "/bin/printf GRAMMAR_FOR_CLAUSE_ALT04_OK",
        "GRAMMAR_FOR_CLAUSE_ALT04_OK",
    ),
    (
        "tag_18_10_02.grammar.name.alt01.valid_name",
        "for grammar_name_alt01 in value; do "
        "test \"$grammar_name_alt01\" = value && "
        "/bin/printf GRAMMAR_NAME_ALT01_OK; done",
        "GRAMMAR_NAME_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.in.alt01.reserved_word",
        "for value in selected; do test \"$value\" = selected && "
        "/bin/printf GRAMMAR_IN_ALT01_OK; done",
        "GRAMMAR_IN_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.wordlist.alt01.recursive_words",
        "grammar_wordlist_alt01_count=0; for value in one two; do "
        "grammar_wordlist_alt01_count=$((grammar_wordlist_alt01_count + 1)); "
        "done; test \"$grammar_wordlist_alt01_count\" -eq 2 && "
        "/bin/printf GRAMMAR_WORDLIST_ALT01_OK",
        "GRAMMAR_WORDLIST_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.wordlist.alt02.single_word",
        "for value in single; do test \"$value\" = single && "
        "/bin/printf GRAMMAR_WORDLIST_ALT02_OK; done",
        "GRAMMAR_WORDLIST_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.case_clause.alt01.terminated_case_list",
        "case selected in selected) "
        "/bin/printf GRAMMAR_CASE_CLAUSE_ALT01_OK;; esac",
        "GRAMMAR_CASE_CLAUSE_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.case_clause.alt02.final_unterminated_item",
        "case selected in selected) "
        "/bin/printf GRAMMAR_CASE_CLAUSE_ALT02_OK; esac",
        "GRAMMAR_CASE_CLAUSE_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.case_clause.alt03.empty_case_list",
        "case selected in esac; /bin/printf GRAMMAR_CASE_CLAUSE_ALT03_OK",
        "GRAMMAR_CASE_CLAUSE_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.case_list_ns.alt01.mixed_final_item",
        "case final in first) :;; final) "
        "/bin/printf GRAMMAR_CASE_LIST_NS_ALT01_OK; esac",
        "GRAMMAR_CASE_LIST_NS_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.case_list_ns.alt02.single_final_item",
        "case final in final) "
        "/bin/printf GRAMMAR_CASE_LIST_NS_ALT02_OK; esac",
        "GRAMMAR_CASE_LIST_NS_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.case_list.alt01.multiple_terminated_items",
        "case second in first) :;; second) "
        "/bin/printf GRAMMAR_CASE_LIST_ALT01_OK;; esac",
        "GRAMMAR_CASE_LIST_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.case_list.alt02.single_terminated_item",
        "case selected in selected) "
        "/bin/printf GRAMMAR_CASE_LIST_ALT02_OK;; esac",
        "GRAMMAR_CASE_LIST_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.case_item_ns.alt01.empty_body",
        "case selected in selected) esac; "
        "/bin/printf GRAMMAR_CASE_ITEM_NS_ALT01_OK",
        "GRAMMAR_CASE_ITEM_NS_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.case_item_ns.alt02.compound_body",
        "case selected in selected) "
        "/bin/printf GRAMMAR_CASE_ITEM_NS_ALT02_OK; esac",
        "GRAMMAR_CASE_ITEM_NS_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.case_item_ns.alt03.leading_parenthesis_empty",
        "case selected in (selected) esac; "
        "/bin/printf GRAMMAR_CASE_ITEM_NS_ALT03_OK",
        "GRAMMAR_CASE_ITEM_NS_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.case_item_ns.alt04.leading_parenthesis_body",
        "case selected in (selected) "
        "/bin/printf GRAMMAR_CASE_ITEM_NS_ALT04_OK; esac",
        "GRAMMAR_CASE_ITEM_NS_ALT04_OK",
    ),
    (
        "tag_18_10_02.grammar.case_item.alt01.empty_body",
        "case selected in selected) ;; esac; "
        "/bin/printf GRAMMAR_CASE_ITEM_ALT01_OK",
        "GRAMMAR_CASE_ITEM_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.case_item.alt02.compound_body",
        "case selected in selected) "
        "/bin/printf GRAMMAR_CASE_ITEM_ALT02_OK;; esac",
        "GRAMMAR_CASE_ITEM_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.case_item.alt03.leading_parenthesis_empty",
        "case selected in (selected) ;; esac; "
        "/bin/printf GRAMMAR_CASE_ITEM_ALT03_OK",
        "GRAMMAR_CASE_ITEM_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.case_item.alt04.leading_parenthesis_body",
        "case selected in (selected) "
        "/bin/printf GRAMMAR_CASE_ITEM_ALT04_OK;; esac",
        "GRAMMAR_CASE_ITEM_ALT04_OK",
    ),
    (
        "tag_18_10_02.grammar.pattern.alt01.single_word",
        "case selected in selected) "
        "/bin/printf GRAMMAR_PATTERN_ALT01_OK;; esac",
        "GRAMMAR_PATTERN_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.pattern.alt02.alternative_words",
        "case second in first|second) "
        "/bin/printf GRAMMAR_PATTERN_ALT02_OK;; esac",
        "GRAMMAR_PATTERN_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.if_clause.alt01.with_else_part",
        "if false; then exit 1; else "
        "/bin/printf GRAMMAR_IF_CLAUSE_ALT01_OK; fi",
        "GRAMMAR_IF_CLAUSE_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.if_clause.alt02.without_else_part",
        "if /bin/true; then /bin/printf GRAMMAR_IF_CLAUSE_ALT02_OK; fi",
        "GRAMMAR_IF_CLAUSE_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.else_part.alt01.elif_without_tail",
        "if false; then exit 1; elif /bin/true; then "
        "/bin/printf GRAMMAR_ELSE_PART_ALT01_OK; fi",
        "GRAMMAR_ELSE_PART_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.else_part.alt02.recursive_elif",
        "if false; then exit 1; elif false; then exit 1; "
        "elif /bin/true; then /bin/printf GRAMMAR_ELSE_PART_ALT02_OK; fi",
        "GRAMMAR_ELSE_PART_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.else_part.alt03.else_branch",
        "if false; then exit 1; else "
        "/bin/printf GRAMMAR_ELSE_PART_ALT03_OK; fi",
        "GRAMMAR_ELSE_PART_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.while_clause.alt01.repeated_condition",
        "grammar_while_alt01_count=0; "
        "while test \"$grammar_while_alt01_count\" -lt 2; do "
        "grammar_while_alt01_count=$((grammar_while_alt01_count + 1)); done; "
        "test \"$grammar_while_alt01_count\" -eq 2 && "
        "/bin/printf GRAMMAR_WHILE_CLAUSE_ALT01_OK",
        "GRAMMAR_WHILE_CLAUSE_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.until_clause.alt01.inverted_condition",
        "grammar_until_alt01_count=0; "
        "until test \"$grammar_until_alt01_count\" -eq 2; do "
        "grammar_until_alt01_count=$((grammar_until_alt01_count + 1)); done; "
        "test \"$grammar_until_alt01_count\" -eq 2 && "
        "/bin/printf GRAMMAR_UNTIL_CLAUSE_ALT01_OK",
        "GRAMMAR_UNTIL_CLAUSE_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.function_definition.alt01.name_parens_body",
        "grammar_function_definition_alt01(){ "
        "/bin/printf GRAMMAR_FUNCTION_DEFINITION_ALT01_OK; }; "
        "grammar_function_definition_alt01",
        "GRAMMAR_FUNCTION_DEFINITION_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.function_body.alt01.compound_command",
        "grammar_function_body_alt01(){ "
        "/bin/printf GRAMMAR_FUNCTION_BODY_ALT01_OK; }; "
        "grammar_function_body_alt01",
        "GRAMMAR_FUNCTION_BODY_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.function_body.alt02.compound_body_redirect",
        "grammar_function_body_alt02(){ /bin/printf '%s\\n' payload; } "
        ">/tmp/grammar-function-body-alt02; "
        "grammar_function_body_alt02; "
        "IFS= read -r value </tmp/grammar-function-body-alt02; "
        "test \"$value\" = payload && "
        "/bin/printf GRAMMAR_FUNCTION_BODY_ALT02_OK",
        "GRAMMAR_FUNCTION_BODY_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.fname.alt01.valid_name",
        "grammar_fname_alt01(){ /bin/true; }; grammar_fname_alt01 && "
        "/bin/printf GRAMMAR_FNAME_ALT01_OK",
        "GRAMMAR_FNAME_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.brace_group.alt01.current_environment",
        "grammar_brace_alt01=outer; { grammar_brace_alt01=inner; }; "
        "test \"$grammar_brace_alt01\" = inner && "
        "/bin/printf GRAMMAR_BRACE_GROUP_ALT01_OK",
        "GRAMMAR_BRACE_GROUP_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.do_group.alt01.do_done_body",
        "for value in selected; do test \"$value\" = selected && "
        "/bin/printf GRAMMAR_DO_GROUP_ALT01_OK; done",
        "GRAMMAR_DO_GROUP_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.simple_command.alt01.prefix_word_suffix",
        ">/tmp/grammar-simple-command-alt01 /bin/printf payload; "
        "IFS= read -r value </tmp/grammar-simple-command-alt01; "
        "test \"$value\" = payload && "
        "/bin/printf GRAMMAR_SIMPLE_COMMAND_ALT01_OK",
        "GRAMMAR_SIMPLE_COMMAND_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.simple_command.alt02.prefix_word_no_suffix",
        "GRAMMAR_SIMPLE_COMMAND_ALT02=value /bin/true && "
        "/bin/printf GRAMMAR_SIMPLE_COMMAND_ALT02_OK",
        "GRAMMAR_SIMPLE_COMMAND_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.simple_command.alt03.prefix_only",
        "GRAMMAR_SIMPLE_COMMAND_ALT03=value; "
        "test \"$GRAMMAR_SIMPLE_COMMAND_ALT03\" = value && "
        "/bin/printf GRAMMAR_SIMPLE_COMMAND_ALT03_OK",
        "GRAMMAR_SIMPLE_COMMAND_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.simple_command.alt04.name_suffix",
        "/bin/printf GRAMMAR_SIMPLE_COMMAND_ALT04_OK",
        "GRAMMAR_SIMPLE_COMMAND_ALT04_OK",
    ),
    (
        "tag_18_10_02.grammar.simple_command.alt05.name_only",
        "/bin/true && /bin/printf GRAMMAR_SIMPLE_COMMAND_ALT05_OK",
        "GRAMMAR_SIMPLE_COMMAND_ALT05_OK",
    ),
    (
        "tag_18_10_02.grammar.cmd_name.alt01.command_word",
        "/bin/true && /bin/printf GRAMMAR_CMD_NAME_ALT01_OK",
        "GRAMMAR_CMD_NAME_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.cmd_word.alt01.word_after_prefix",
        "GRAMMAR_CMD_WORD_ALT01=value /bin/true && "
        "/bin/printf GRAMMAR_CMD_WORD_ALT01_OK",
        "GRAMMAR_CMD_WORD_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.cmd_prefix.alt01.single_io_redirect",
        ">/tmp/grammar-cmd-prefix-alt01; "
        "test -e /tmp/grammar-cmd-prefix-alt01 && "
        "/bin/printf GRAMMAR_CMD_PREFIX_ALT01_OK",
        "GRAMMAR_CMD_PREFIX_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.cmd_prefix.alt02.multiple_prefix_redirects",
        "3>/tmp/grammar-cmd-prefix-alt02-a "
        "4>/tmp/grammar-cmd-prefix-alt02-b /bin/true; "
        "test -e /tmp/grammar-cmd-prefix-alt02-a && "
        "test -e /tmp/grammar-cmd-prefix-alt02-b && "
        "/bin/printf GRAMMAR_CMD_PREFIX_ALT02_OK",
        "GRAMMAR_CMD_PREFIX_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.cmd_prefix.alt03.assignment_word",
        "GRAMMAR_CMD_PREFIX_ALT03=value; "
        "test \"$GRAMMAR_CMD_PREFIX_ALT03\" = value && "
        "/bin/printf GRAMMAR_CMD_PREFIX_ALT03_OK",
        "GRAMMAR_CMD_PREFIX_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.cmd_prefix.alt04.recursive_assignments",
        "GRAMMAR_CMD_PREFIX_ALT04_A=one GRAMMAR_CMD_PREFIX_ALT04_B=two "
        "/bin/true && /bin/printf GRAMMAR_CMD_PREFIX_ALT04_OK",
        "GRAMMAR_CMD_PREFIX_ALT04_OK",
    ),
    (
        "tag_18_10_02.grammar.cmd_suffix.alt01.single_io_redirect",
        "/bin/true >/tmp/grammar-cmd-suffix-alt01; "
        "test -e /tmp/grammar-cmd-suffix-alt01 && "
        "/bin/printf GRAMMAR_CMD_SUFFIX_ALT01_OK",
        "GRAMMAR_CMD_SUFFIX_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.cmd_suffix.alt02.multiple_io_redirects",
        "/bin/true 3>/tmp/grammar-cmd-suffix-alt02-a "
        "4>/tmp/grammar-cmd-suffix-alt02-b; "
        "test -e /tmp/grammar-cmd-suffix-alt02-a && "
        "test -e /tmp/grammar-cmd-suffix-alt02-b && "
        "/bin/printf GRAMMAR_CMD_SUFFIX_ALT02_OK",
        "GRAMMAR_CMD_SUFFIX_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.cmd_suffix.alt03.single_word",
        "/bin/printf GRAMMAR_CMD_SUFFIX_ALT03_OK",
        "GRAMMAR_CMD_SUFFIX_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.cmd_suffix.alt04.recursive_words",
        "/bin/printf '%s' GRAMMAR_CMD_SUFFIX_ALT04_OK",
        "GRAMMAR_CMD_SUFFIX_ALT04_OK",
    ),
    (
        "tag_18_10_02.grammar.redirect_list.alt01.single_compound_redirect",
        "{ /bin/printf '%s\\n' payload; } "
        ">/tmp/grammar-redirect-list-alt01; "
        "IFS= read -r value </tmp/grammar-redirect-list-alt01; "
        "test \"$value\" = payload && "
        "/bin/printf GRAMMAR_REDIRECT_LIST_ALT01_OK",
        "GRAMMAR_REDIRECT_LIST_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.redirect_list.alt02.multiple_compound_redirects",
        "{ /bin/printf '%s\\n' payload; } "
        "3>/tmp/grammar-redirect-list-alt02-a "
        ">/tmp/grammar-redirect-list-alt02-b; "
        "\n"
        "IFS= read -r value </tmp/grammar-redirect-list-alt02-b; "
        "test \"$value\" = payload && "
        "test -e /tmp/grammar-redirect-list-alt02-a && "
        "/bin/printf GRAMMAR_REDIRECT_LIST_ALT02_OK",
        "GRAMMAR_REDIRECT_LIST_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.io_redirect.alt01.unnumbered_io_file",
        ": >/tmp/grammar-io-redirect-alt01; "
        "test -e /tmp/grammar-io-redirect-alt01 && "
        "/bin/printf GRAMMAR_IO_REDIRECT_ALT01_OK",
        "GRAMMAR_IO_REDIRECT_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.io_redirect.alt02.numbered_io_file",
        ": 3>/tmp/grammar-io-redirect-alt02; "
        "test -e /tmp/grammar-io-redirect-alt02 && "
        "/bin/printf GRAMMAR_IO_REDIRECT_ALT02_OK",
        "GRAMMAR_IO_REDIRECT_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.io_redirect.alt03.unnumbered_io_here",
        "/bin/printf 'IFS= read -r value <<END\\nbody\\nEND\\n"
        "test \"$value\" = body && "
        "/bin/printf GRAMMAR_IO_REDIRECT_ALT03_OK\\n' "
        ">/tmp/grammar-io-redirect-alt03; "
        "/bin/sh /tmp/grammar-io-redirect-alt03",
        "GRAMMAR_IO_REDIRECT_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.io_redirect.alt04.numbered_io_here",
        "/bin/printf '{ IFS= read -r value <&3; } 3<<END\\nbody\\nEND\\n"
        "test \"$value\" = body && "
        "/bin/printf GRAMMAR_IO_REDIRECT_ALT04_OK\\n' "
        ">/tmp/grammar-io-redirect-alt04; "
        "/bin/sh /tmp/grammar-io-redirect-alt04",
        "GRAMMAR_IO_REDIRECT_ALT04_OK",
    ),
    (
        "tag_18_10_02.grammar.io_file.alt01.input_file",
        "/bin/printf '%s\\n' payload >/tmp/grammar-io-file-alt01; "
        "IFS= read -r value </tmp/grammar-io-file-alt01; "
        "test \"$value\" = payload && /bin/printf GRAMMAR_IO_FILE_ALT01_OK",
        "GRAMMAR_IO_FILE_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.io_file.alt02.duplicate_input",
        "/bin/printf '%s\\n' payload >/tmp/grammar-io-file-alt02; "
        "exec 3</tmp/grammar-io-file-alt02; IFS= read -r value <&3; "
        "exec 3<&-; test \"$value\" = payload && "
        "/bin/printf GRAMMAR_IO_FILE_ALT02_OK",
        "GRAMMAR_IO_FILE_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.io_file.alt03.output_file",
        "/bin/printf payload >/tmp/grammar-io-file-alt03; "
        "test -s /tmp/grammar-io-file-alt03 && "
        "/bin/printf GRAMMAR_IO_FILE_ALT03_OK",
        "GRAMMAR_IO_FILE_ALT03_OK",
    ),
    (
        "tag_18_10_02.grammar.io_file.alt04.duplicate_output",
        "exec 3>/tmp/grammar-io-file-alt04; /bin/printf payload >&3; "
        "exec 3>&-; test -s /tmp/grammar-io-file-alt04 && "
        "/bin/printf GRAMMAR_IO_FILE_ALT04_OK",
        "GRAMMAR_IO_FILE_ALT04_OK",
    ),
    (
        "tag_18_10_02.grammar.io_file.alt05.append_output",
        "/bin/printf first >/tmp/grammar-io-file-alt05; "
        "/bin/printf second >>/tmp/grammar-io-file-alt05; "
        "IFS= read -r value </tmp/grammar-io-file-alt05; "
        "test \"$value\" = firstsecond && /bin/printf GRAMMAR_IO_FILE_ALT05_OK",
        "GRAMMAR_IO_FILE_ALT05_OK",
    ),
    (
        "tag_18_10_02.grammar.io_file.alt06.read_write_file",
        "/bin/printf '%s\\n' payload >/tmp/grammar-io-file-alt06; "
        "exec 3<>/tmp/grammar-io-file-alt06; IFS= read -r value <&3; "
        "exec 3>&-; test \"$value\" = payload && "
        "/bin/printf GRAMMAR_IO_FILE_ALT06_OK",
        "GRAMMAR_IO_FILE_ALT06_OK",
    ),
    (
        "tag_18_10_02.grammar.io_file.alt07.clobber_override",
        "/bin/printf old >/tmp/grammar-io-file-alt07; set -C; "
        "/bin/printf new >|/tmp/grammar-io-file-alt07; status=$?; set +C; "
        "IFS= read -r value </tmp/grammar-io-file-alt07; "
        "test \"$status\" -eq 0 && test \"$value\" = new && "
        "/bin/printf GRAMMAR_IO_FILE_ALT07_OK",
        "GRAMMAR_IO_FILE_ALT07_OK",
    ),
    (
        "tag_18_10_02.grammar.filename.alt01.single_field_expansion_boundary",
        "grammar_filename_alt01='/tmp/grammar-filename-alt01 space'; "
        ": >$grammar_filename_alt01; "
        "test -e \"$grammar_filename_alt01\" && "
        "/bin/printf GRAMMAR_FILENAME_ALT01_OK",
        "GRAMMAR_FILENAME_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.io_here.alt01.here_document",
        "/bin/printf 'IFS= read -r value <<END\\nbody\\nEND\\n"
        "test \"$value\" = body && /bin/printf GRAMMAR_IO_HERE_ALT01_OK\\n' "
        ">/tmp/grammar-io-here-alt01; /bin/sh /tmp/grammar-io-here-alt01",
        "GRAMMAR_IO_HERE_ALT01_OK",
    ),
    (
        "tag_18_10_02.grammar.io_here.alt02.tab_stripping_here_document",
        ": >/tmp/grammar-io-here-alt02; "
        "/bin/printf 'IFS= read -r value <<-END\\n' "
        ">>/tmp/grammar-io-here-alt02; "
        "\n"
        "/bin/printf '\\011%s\\n' tabbed END >>/tmp/grammar-io-here-alt02; "
        "\n"
        "/bin/printf 'test \"$value\" = tabbed && "
        "/bin/printf GRAMMAR_IO_HERE_ALT02_OK\\n' "
        ">>/tmp/grammar-io-here-alt02; /bin/sh /tmp/grammar-io-here-alt02",
        "GRAMMAR_IO_HERE_ALT02_OK",
    ),
    (
        "tag_18_10_02.grammar.here_end.alt01.quote_removed_word",
        "/bin/printf '%s\\n' \"IFS= read -r value <<'END'\" body END "
        "'test \"$value\" = body && "
        "/bin/printf GRAMMAR_HERE_END_ALT01_OK' "
        ">/tmp/grammar-here-end-alt01; /bin/sh /tmp/grammar-here-end-alt01",
        "GRAMMAR_HERE_END_ALT01_OK",
    ),
)

PRINTF_UTILITY_COMMAND_CASES = (
    (
        "printf.format_reuse_partial_final_cycle",
        "/bin/printf '%5d%4d\\n' 1 21 321 4321 54321",
        "    1  21\n  3214321\n54321   0\n",
    ),
    (
        "printf.missing_operands_use_specified_defaults",
        "/bin/printf '<%s>|<%c>|<%d>|<%i>|<%u>|<%o>|<%x>|<%X>\\n'",
        "<>|<>|<0>|<0>|<0>|<0>|<0>|<0>\n",
    ),
    (
        "printf.flags_width_and_precision",
        "/bin/printf '[%+05d][%-5s][%.3s][%#x][%#o][%.0d]\\n' 42 hi abcde 16 8 0",
        "[+0042][hi   ][abc][0x10][010][]\n",
    ),
    (
        "printf.mandatory_conversion_set",
        "/bin/printf '[%d][%i][%u][%o][%x][%X][%c][%s][%%]\\n' -2 +3 4 8 31 31 AB text",
        "[-2][3][4][10][1f][1F][A][text][%]\n",
    ),
    (
        "printf.bare_numeric_has_no_implicit_padding",
        "/bin/printf '<%d><%u><%o>\\n' 1 1 1",
        "<1><1><1>\n",
    ),
    (
        "printf.field_width_does_not_truncate",
        "/bin/printf '[%2s][%2d]\\n' wider 12345",
        "[wider][12345]\n",
    ),
    (
        "printf.percent_b_precision_and_width",
        "/bin/printf '[%6.3b]\\n' 'a\\tbcd'",
        "[   a\tb]\n",
    ),
    (
        "printf.percent_b_c_stops_utility_output",
        "/bin/printf 'A%bZ%s' 'left\\cright' ignored; /bin/printf '|NEXT|\\n'",
        "Aleft|NEXT|\n",
    ),
    (
        "printf.c_integer_constant_extensions",
        "/bin/printf '%d %d %d\\n' 010 0x10 \"'A\"",
        "8 16 65\n",
    ),
    (
        "printf.format_octal_escape",
        "/bin/printf '<\\141>\\n'",
        "<a>\n",
    ),
    (
        "printf.numeric_conversion_diagnostic_continues",
        "/bin/printf '%d:%d' 12x 7 2>/tmp/printf-error; status=$?; "
        'test "$status" -ne 0 && test -s /tmp/printf-error && '
        "/bin/printf '|STATUS_%d|\\n' \"$status\"",
        "12:7|STATUS_1|\n",
    ),
    (
        "printf.format_escape_binary_bytes",
        r"/bin/printf '\000\134\a\b\f\n\r\t\v\377' "
        "> /tmp/printf-format-bytes; "
        "/bin/byte-oracle /tmp/printf-format-bytes 005c07080c0a0d090bff && "
        "/bin/printf 'PRINTF_FORMAT_BYTES_OK\\n'",
        "PRINTF_FORMAT_BYTES_OK\n",
    ),
    (
        "printf.byte_oracle_rejects_mismatch",
        "/bin/byte-oracle /tmp/printf-format-bytes 00 2>/tmp/printf-error; "
        "status=$?; /bin/printf 'BYTE_MISMATCH_STATUS_%d\\n' \"$status\"",
        "BYTE_MISMATCH_STATUS_1\n",
    ),
    (
        "printf.percent_b_binary_bytes",
        r"/bin/printf '%b' '\0\\\a\b\f\n\r\t\v\07\012\0101Z' "
        "> /tmp/printf-b-bytes; "
        "/bin/byte-oracle /tmp/printf-b-bytes 005c07080c0a0d090b070a415a && "
        "/bin/printf 'PRINTF_B_BYTES_OK\\n'",
        "PRINTF_B_BYTES_OK\n",
    ),
    (
        "printf.integer_overflow_boundaries",
        "/bin/printf '%d:%u:%u' 9223372036854775808 "
        "18446744073709551615 18446744073709551616 "
        "2>/tmp/printf-overflow-error; status=$?; "
        'test "$status" -ne 0 && test -s /tmp/printf-overflow-error && '
        "/bin/printf '|STATUS_%d|\\n' \"$status\"",
        "9223372036854775807:18446744073709551615:18446744073709551615|STATUS_1|\n",
    ),
    (
        "printf.missing_format_diagnostic",
        "/bin/printf 2>/tmp/printf-error; status=$?; "
        'test "$status" -ne 0 && test -s /tmp/printf-error && '
        "/bin/printf 'MISSING_FORMAT_STATUS_%d\\n' \"$status\"",
        "MISSING_FORMAT_STATUS_1\n",
    ),
    (
        "printf.standard_input_not_read",
        "/bin/printf 'stdin-marker\\n' >/tmp/printf-format-bytes; "
        "exec 3</tmp/printf-format-bytes; /bin/printf 'OUTPUT|' <&3; "
        "IFS= read -r printf_stdin_value <&3; exec 3<&-; "
        "/bin/printf '%s\\n' \"$printf_stdin_value\"",
        "OUTPUT|stdin-marker\n",
    ),
    (
        "printf.failed_standard_output_status",
        "/bin/printf output >&-; status=$?; "
        "/bin/printf 'FAILED_STDOUT_STATUS_%d\\n' \"$status\"",
        "FAILED_STDOUT_STATUS_1\n",
    ),
    (
        "printf.locale_lang_default",
        "LANG=POSIX LC_ALL= LC_CTYPE= /bin/printf '<%c><%.1s>\\n' AB XY",
        "<A><X>\n",
    ),
    (
        "printf.locale_lc_all_overrides_lc_ctype",
        "LANG=C LC_ALL=POSIX LC_CTYPE=C /bin/printf '<%c><%.1s>\\n' CD ZW",
        "<C><Z>\n",
    ),
    (
        "printf.locale_lc_messages_diagnostic",
        "LANG=POSIX LC_ALL=C LC_MESSAGES=POSIX /bin/printf '%d' bad "
        "2>/tmp/printf-error; status=$?; "
        "/bin/printf 'LOCALE_STATUS_%d\\n' \"$status\"",
        "0LOCALE_STATUS_1\n",
    ),
    (
        "printf.default_signal_action",
        "/bin/printf-signal-oracle && /bin/printf 'PRINTF_SIGNAL_OK\\n'",
        "PRINTF_SIGNAL_OK\n",
    ),
)


def start_qemu():
    cmd = [
        QEMU_BIN,
        "-machine",
        "pc-q35-11.1",
        "-cpu",
        "qemu64",
        "-m",
        "512M",
        "-smp",
        "1",
        "-cdrom",
        BOOT_IMAGE,
        "-boot",
        "d",
        "-serial",
        f"file:{COM1_LOG}",
        "-serial",
        f"tcp::{SHELL_PORT},server,nowait",
        "-nodefaults",
        "-vga",
        "none",
        "-nic",
        "none",
        "-nographic",
        "-monitor",
        "none",
        "-no-reboot",
        "-no-shutdown",
    ]
    return subprocess.Popen(
        cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def connect_shell(retries=20, delay=0.5):
    for attempt in range(retries):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(CMD_TIMEOUT)
            sock.connect(("127.0.0.1", SHELL_PORT))
            return sock
        except (ConnectionRefusedError, OSError):
            if attempt == retries - 1:
                raise
            time.sleep(delay)
    raise RuntimeError("shell port was never reachable")


def recv_until_any_prompt(sock, timeout=CMD_TIMEOUT):
    data = b""
    end_time = time.time() + timeout
    while time.time() < end_time:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            if SHELL_PROMPT.encode("utf-8") in data:
                break
        except TimeoutError:
            continue
    return data.decode("utf-8", errors="replace")


def send_command(sock, command):
    sock.sendall((command + "\r").encode("utf-8"))
    response = recv_until_any_prompt(sock)
    execution_boundary = response.find("\r\r\n")
    if execution_boundary >= 0:
        response = response[execution_boundary + len("\r\r\n") :]
    return response.replace("\r\n", "\n")


def send_command_sequence(sock, command):
    responses = []
    for physical_line in command.split("\n"):
        encoded_line = physical_line.encode("ascii")
        if len(encoded_line) > SHELL_SERIAL_RX_PAYLOAD_LIMIT:
            raise ValueError(
                "shell test physical line exceeds the guest serial RX payload "
                f"limit: {len(encoded_line)} > {SHELL_SERIAL_RX_PAYLOAD_LIMIT}"
            )
        response = send_command(sock, physical_line)
        if response.endswith(SHELL_PROMPT):
            response = response[: -len(SHELL_PROMPT)]
        responses.append(response)
    return "".join(responses)


def require_contains(name, response, expected):
    if expected not in response:
        print(f"FAIL: {name} response missing {expected!r}")
        print(f"  Response: {response!r}")
        return False
    print(f"PASS: {name}")
    return True


def require_exact_output(name, response, expected):
    actual = response
    if actual.endswith(SHELL_PROMPT):
        actual = actual[: -len(SHELL_PROMPT)]
    if actual != expected:
        print(f"FAIL: {name} output mismatch")
        print(f"  Actual:   {actual!r}")
        print(f"  Expected: {expected!r}")
        return False
    print(f"PASS: {name}")
    return True


def require_repeated_process_lifecycle(sock, repetitions=70):
    for iteration in range(repetitions):
        response = send_command(sock, "/bin/true")
        if SHELL_PROMPT not in response:
            print(
                f"FAIL: repeated process lifecycle stalled at iteration {iteration + 1}"
            )
            print(f"  Response: {response!r}")
            return False
    print(f"PASS: repeated process lifecycle ({repetitions} cycles)")
    return True


def require_repeated_pipeline_lifecycle(sock, repetitions=40):
    for iteration in range(repetitions):
        response = send_command(
            sock,
            "printf '%s\\n' pipeline-cycle | "
            "{ read value; printf '%s\\n' \"$value\" > /tmp/cycle; }",
        )
        if SHELL_PROMPT not in response:
            print(
                "FAIL: repeated pipeline lifecycle stalled at "
                f"iteration {iteration + 1}"
            )
            print(f"  Response: {response!r}")
            return False
    print(f"PASS: repeated pipeline lifecycle ({repetitions} cycles)")
    return True


def require_timer_preemption(sock):
    response = send_command(sock, "preempt-check")
    child_position = response.find("PREEMPT_CHILD_RAN")
    parent_position = response.find("PREEMPT_PARENT_DONE")
    if child_position < 0 or parent_position < 0 or child_position > parent_position:
        print("FAIL: timer preemption did not run the child during the parent spin")
        print(f"  Response: {response!r}")
        return False
    print("PASS: timer preemption")
    return True


def require_signal_job_control(sock):
    response = send_command(sock, "signal-check")
    markers = (
        "SIGNAL_CONTEXT_OK",
        "SIGNAL_MASK_OK",
        "SIGNAL_EXIT_STATUS_OK",
        "JOB_CONTROL_OK",
        "TTY_PGRP_OK",
    )
    missing = [marker for marker in markers if marker not in response]
    if missing:
        print(f"FAIL: signal and job-control markers missing: {missing!r}")
        print(f"  Response: {response!r}")
        return False
    print("PASS: signal delivery, masks, wait states, job control, and TTY pgrp")
    return True


def require_shell_language_command_cases(sock):
    results = []
    results.append(
        require_contains(
            "shell language scratch directory setup",
            send_command(
                sock,
                "/bin/rm -f /tmp/redirection /tmp/pipeline /tmp/order "
                "/tmp/sh-redirection /tmp/cycle; "
                f"/bin/rm -rf {SHELL_LANGUAGE_SCRATCH_DIRECTORY}; "
                f"/bin/mkdir {SHELL_LANGUAGE_SCRATCH_DIRECTORY} && "
                "/bin/printf SHELL_LANGUAGE_SCRATCH_OK",
            ),
            "SHELL_LANGUAGE_SCRATCH_OK",
        )
    )
    for case_id, command, expected in SHELL_LANGUAGE_COMMAND_CASES:
        isolated_command = command.replace(
            "/tmp/", f"{SHELL_LANGUAGE_SCRATCH_DIRECTORY}/"
        )
        results.append(
            require_contains(case_id, send_command(sock, isolated_command), expected)
        )
    results.append(
        require_contains(
            "shell language temporary-file cleanup",
            send_command(
                sock,
                f"/bin/rm -rf {SHELL_LANGUAGE_SCRATCH_DIRECTORY} && "
                f"test ! -e {SHELL_LANGUAGE_SCRATCH_DIRECTORY} && "
                "/bin/printf SHELL_LANGUAGE_CLEANUP_OK",
            ),
            "SHELL_LANGUAGE_CLEANUP_OK",
        )
    )
    results.append(
        require_contains(
            "bootfs dynamic-node create unlink dup fork exit and reuse",
            send_command(sock, "/bin/bootfs-reuse-check"),
            "BOOTFS_DYNAMIC_NODE_REUSE_OK",
        )
    )
    return all(results)


def require_shell_grammar_scratch_cleanup(sock, cleanup_id):
    return require_contains(
        cleanup_id,
        send_command_sequence(
            sock,
            "/bin/rm -f /tmp/grammar-* /tmp/sh*.tmp\n"
            "/bin/printf probe >/tmp/grammar-reclaim-one && "
            "/bin/printf probe >/tmp/grammar-reclaim-two; "
            "grammar_reclaim_status=$?\n"
            "/bin/rm -f /tmp/grammar-reclaim-one "
            "/tmp/grammar-reclaim-two && "
            'test "$grammar_reclaim_status" -eq 0 && '
            "test ! -e /tmp/grammar-reclaim-one && "
            "test ! -e /tmp/grammar-reclaim-two && "
            "/bin/printf GRAMMAR_CASE_CLEANUP_OK",
        ),
        "GRAMMAR_CASE_CLEANUP_OK",
    )


def require_shell_grammar_command_cases(sock):
    results = []
    scratch_cases_since_cleanup = 0
    last_scratch_case_id = ""
    for case_id, command, expected in SHELL_GRAMMAR_COMMAND_CASES:
        results.append(
            require_contains(case_id, send_command_sequence(sock, command), expected)
        )
        grammar_case_uses_scratch = (
            "/tmp/grammar-" in command or "/tmp/sh" in command
        )
        if not grammar_case_uses_scratch:
            continue
        scratch_cases_since_cleanup += 1
        last_scratch_case_id = case_id
        if scratch_cases_since_cleanup < 4:
            continue
        results.append(
            require_shell_grammar_scratch_cleanup(sock, f"{case_id}.cleanup")
        )
        scratch_cases_since_cleanup = 0

    if scratch_cases_since_cleanup:
        results.append(
            require_shell_grammar_scratch_cleanup(
                sock, f"{last_scratch_case_id}.cleanup"
            )
        )
    return all(results)


def require_printf_utility_command_cases(sock):
    results = []
    for case_id, command, expected in PRINTF_UTILITY_COMMAND_CASES:
        results.append(
            require_exact_output(case_id, send_command(sock, command), expected)
        )
    return all(results)


def require_command_substitution_alias_boundary(sock):
    benign = send_command(
        sock,
        '/bin/sh -c \'alias x="printf benign"; '
        'eval "print -- \\"\\$(x physical_tail)\\""\'',
    )
    hostile = send_command(
        sock,
        '/bin/sh -c \'alias x="printf hostile )"; '
        'eval "print -- \\"\\$(x physical_tail)\\""\'; '
        "status=$?; /bin/printf 'BOUNDARY_HOSTILE_STATUS_%s' \"$status\"",
    )
    return (
        require_contains(
            "tag_18_02_03.command_substitution_alias_boundary_benign",
            benign,
            "benign",
        )
        and require_contains(
            "tag_18_02_03.command_substitution_alias_cannot_move_physical_boundary",
            hostile,
            "BOUNDARY_HOSTILE_STATUS_1",
        )
        and require_contains(
            "tag_18_02_03.command_substitution_alias_syntax_diagnostic",
            hostile,
            "syntax error: unexpected ')'",
        )
    )


def main():
    if not os.path.isfile(BOOT_IMAGE):
        print(f"SKIP: Boot image not found: {BOOT_IMAGE}")
        sys.exit(77)

    qemu = start_qemu()

    try:
        shell = connect_shell(retries=int(BOOT_TIMEOUT / 0.5))
        initial = recv_until_any_prompt(shell, timeout=BOOT_TIMEOUT)
        if SHELL_PROMPT not in initial:
            shell.sendall(b"\r")
            initial += recv_until_any_prompt(shell, timeout=BOOT_TIMEOUT)
        if SHELL_PROMPT not in initial:
            print("FAIL: did not receive the Ring 3 POSIX sh prompt")
            print(f"  Received: {initial!r}")
            sys.exit(1)

        if RING3_SENTINEL not in initial:
            print("FAIL: POSIX sh prompt arrived without the Ring 3 startup marker")
            print(f"  Received: {initial!r}")
            sys.exit(1)

        results = [
            require_contains(
                "POSIX sh PID 1 identity",
                send_command(shell, '/bin/printf "pid=%s\\n" "$$"'),
                "pid=1",
            ),
            require_contains("pwd", send_command(shell, "pwd"), "/"),
            require_contains(
                "primary shell path",
                send_command(shell, 'printf "%s\\n" "$SHELL"'),
                "/bin/sh",
            ),
            require_contains(
                "mksh executable",
                send_command(
                    shell,
                    "test -x /bin/mksh && printf '%s\\n' MKSH_EXECUTABLE_OK",
                ),
                "MKSH_EXECUTABLE_OK",
            ),
            require_contains(
                "lksh executable",
                send_command(
                    shell,
                    "test -x /bin/lksh && printf '%s\\n' LKSH_EXECUTABLE_OK",
                ),
                "LKSH_EXECUTABLE_OK",
            ),
            require_contains(
                "xash rescue executable",
                send_command(
                    shell,
                    "test -x /bin/xash && printf '%s\\n' XASH_RESCUE_OK",
                ),
                "XASH_RESCUE_OK",
            ),
            require_contains(
                "mkdir executable",
                send_command(
                    shell,
                    "test -x /bin/mkdir && printf '%s\\n' MKDIR_EXECUTABLE_OK",
                ),
                "MKDIR_EXECUTABLE_OK",
            ),
            require_contains(
                "read motd",
                send_command(
                    shell,
                    'IFS= read -r motd_line < /etc/motd; printf "%s\\n" "$motd_line"',
                ),
                MOTD_SENTINEL,
            ),
            require_contains(
                "environment PATH",
                send_command(shell, 'printf "PATH=%s\\n" "$PATH"'),
                "PATH=/bin",
            ),
        ]

        send_command(
            shell,
            'IFS= read -r motd_line < /etc/motd; printf "%s\\n" "$motd_line" > /tmp/motd_copy',
        )
        send_command(shell, "printf '%s\\n' redirection > /tmp/redirection")
        send_command(shell, "printf '%s\\n' append >> /tmp/redirection")
        send_command(
            shell,
            "printf '%s\\n' pipeline-file | "
            "{ IFS= read -r value; printf '%s\\n' \"$value\" > /tmp/pipeline; }",
        )
        send_command(shell, "printf '%s\\n' explicit > /tmp/order | :")
        results.extend(
            [
                require_contains(
                    "copied motd",
                    send_command(
                        shell,
                        'IFS= read -r value < /tmp/motd_copy; printf "%s\\n" "$value"',
                    ),
                    MOTD_SENTINEL,
                ),
                require_contains(
                    "C++23 rm unlink cleanup",
                    send_command(
                        shell,
                        "/bin/rm -f /tmp/motd_copy && "
                        "test ! -e /tmp/motd_copy && "
                        "/bin/printf RM_UNLINK_CLEANUP_OK",
                    ),
                    "RM_UNLINK_CLEANUP_OK",
                ),
                require_contains(
                    "external true return",
                    send_command(shell, "/bin/true"),
                    SHELL_PROMPT,
                ),
                require_contains(
                    "external true status",
                    send_command(shell, "printf '%s\\n' $?"),
                    "0",
                ),
                require_contains(
                    "command separator",
                    send_command(shell, "printf '%s\\n' first; printf '%s\\n' second"),
                    "second",
                ),
                require_contains(
                    "external argv",
                    send_command(shell, "/bin/echo ring3 argv"),
                    "ring3 argv",
                ),
                require_contains(
                    "per-process brk mapping",
                    send_command(shell, "brk-check"),
                    "BRK_MAP_SHRINK_OK",
                ),
                require_contains(
                    "anonymous mapping lifecycle and nonfixed mremap address",
                    send_command(shell, "mmap-check"),
                    "MMAP_REMAP_UNMAP_OK",
                ),
                require_contains(
                    "x86_64 filesystem ABI",
                    send_command(shell, "fs-abi-check"),
                    "FS_ABI_FCNTL_DIRENT_OK",
                ),
                require_contains(
                    "time, sleep, resources, and sigsuspend",
                    send_command(shell, "runtime-check"),
                    "TIME_SLEEP_RESOURCE_SUSPEND_OK",
                ),
                require_contains(
                    "duplicated TTY descriptor and ioctl ABI",
                    send_command(shell, "tty-abi-check"),
                    "TTY_DUP_FLUSH_WINDOW_OK",
                ),
                require_contains(
                    "event-driven select readiness, timeout, and EINTR",
                    send_command(shell, "select-check"),
                    "SELECT_EVENT_TIMEOUT_EINTR_OK",
                ),
                require_contains(
                    "file truncation, ownership, flock, and symlink traversal",
                    send_command(shell, "file-operations-check"),
                    "FILE_TRUNCATE_CHOWN_FLOCK_SYMLINK_OK",
                ),
                require_contains(
                    "symlink-safe file-operation cleanup",
                    send_command(
                        shell,
                        "/bin/rm -rf /tmp/file-operations-check "
                        "/tmp/absolute-link-check /tmp/relative-link-check "
                        "/tmp/bin-link-check /tmp/loop-a-check /tmp/loop-b-check "
                        "&& test -x /bin/true && "
                        "/bin/printf FILE_OPERATIONS_CLEANUP_OK",
                    ),
                    "FILE_OPERATIONS_CLEANUP_OK",
                ),
                require_contains(
                    "file-operation paths absent after cleanup",
                    send_command(
                        shell,
                        "set -- /tmp/file-operations-check "
                        "/tmp/absolute-link-check /tmp/relative-link-check "
                        "/tmp/bin-link-check /tmp/loop-a-check /tmp/loop-b-check; "
                        'for path do test ! -L "$path" && test ! -e "$path" '
                        "|| exit 1; done; "
                        "/bin/printf FILE_OPERATIONS_PATHS_ABSENT_OK",
                    ),
                    "FILE_OPERATIONS_PATHS_ABSENT_OK",
                ),
                require_contains(
                    "rm option terminator and force error semantics",
                    send_command(
                        shell,
                        "cd /tmp && : > ./-f && /bin/rm -- ./-f && "
                        "test ! -e ./-f && if /bin/rm -f /bin; "
                        "then exit 1; fi; /bin/printf RM_OPTION_FORCE_OK",
                    ),
                    "RM_OPTION_FORCE_OK",
                ),
                require_contains(
                    "sh image is lksh",
                    send_command(
                        shell,
                        "/bin/sh -c 'case ${KSH_VERSION-} in "
                        '*LEGACY*KSH*) printf "%s\\n" SH_IS_LKSH_OK;; esac\'',
                    ),
                    "SH_IS_LKSH_OK",
                ),
                require_contains(
                    "lksh image entry",
                    send_command(
                        shell,
                        "/bin/lksh -o posix -c 'printf \"%s\\n\" LKSH_COMMAND_OK'",
                    ),
                    "LKSH_COMMAND_OK",
                ),
                require_contains(
                    "mksh image entry",
                    send_command(
                        shell,
                        "/bin/mksh -c 'printf \"%s\\n\" MKSH_COMMAND_OK'",
                    ),
                    "MKSH_COMMAND_OK",
                ),
                require_contains(
                    "C locale",
                    send_command(
                        shell,
                        'test "$LC_ALL" = C && test "$LANG" = C && '
                        "printf '%s\\n' POSIX_C_LOCALE_OK",
                    ),
                    "POSIX_C_LOCALE_OK",
                ),
                require_contains(
                    "host long arithmetic",
                    send_command(
                        shell,
                        "value=$((2147483647 + 1)); "
                        'test "$value" -eq 2147483648 && '
                        "printf '%s\\n' POSIX_LONG_ARITHMETIC_OK",
                    ),
                    "POSIX_LONG_ARITHMETIC_OK",
                ),
                require_contains(
                    "POSIX mode disables brace expansion",
                    send_command(
                        shell,
                        'set -- brace{a,b}; test "$#" -eq 1 && '
                        "printf '%s\\n' POSIX_BRACE_MODE_OK",
                    ),
                    "POSIX_BRACE_MODE_OK",
                ),
                require_contains(
                    "POSIX byte string length",
                    send_command(
                        shell,
                        "value=$(/bin/printf '\\303\\251'); "
                        'test "${#value}" -eq 2 && '
                        "printf '%s\\n' POSIX_BYTE_LENGTH_OK",
                    ),
                    "POSIX_BYTE_LENGTH_OK",
                ),
                require_contains(
                    "quoting and parameter expansion",
                    send_command(
                        shell,
                        'value="two words"; test "$value" = "two words" && '
                        "printf '%s\\n' SH_QUOTING_PARAMETER_OK",
                    ),
                    "SH_QUOTING_PARAMETER_OK",
                ),
                require_contains(
                    "command substitution and arithmetic",
                    send_command(
                        shell,
                        "value=$(/bin/echo nested); number=$((6 * 7)); "
                        'test "$value" = nested && test "$number" -eq 42 && '
                        "printf '%s\\n' SH_SUBSTITUTION_ARITHMETIC_OK",
                    ),
                    "SH_SUBSTITUTION_ARITHMETIC_OK",
                ),
                require_contains(
                    "functions, loops, and case",
                    send_command(
                        shell,
                        'check_item() { for item in a b; do case "$item" in '
                        "b) printf '%s\\n' SH_FUNCTION_LOOP_CASE_OK;; esac; "
                        "done; }; check_item",
                    ),
                    "SH_FUNCTION_LOOP_CASE_OK",
                ),
                require_contains(
                    "pipeline and redirection",
                    send_command(
                        shell,
                        "printf '%s\\n' sh-redirection > /tmp/sh-redirection; "
                        "read value < /tmp/sh-redirection; "
                        'test "$value" = sh-redirection && '
                        "/bin/echo pipeline | { read pipe_value; "
                        'test "$pipe_value" = pipeline && '
                        "printf '%s\\n' SH_PIPE_REDIRECT_OK; }",
                    ),
                    "SH_PIPE_REDIRECT_OK",
                ),
                require_contains(
                    "asynchronous list and wait",
                    send_command(
                        shell,
                        '/bin/true & child=$!; wait "$child"; '
                        "test \"$?\" -eq 0 && printf '%s\\n' SH_ASYNC_WAIT_OK",
                    ),
                    "SH_ASYNC_WAIT_OK",
                ),
                require_contains(
                    "monitor mode and foreground job",
                    send_command(
                        shell,
                        "/bin/preempt-check & fg %1; status=$?; "
                        'test "$status" -eq 0 && '
                        "printf '%s\\n' SH_JOB_CONTROL_OK",
                    ),
                    "SH_JOB_CONTROL_OK",
                ),
                require_contains(
                    "pipeline",
                    send_command(
                        shell,
                        "printf '%s\\n' pipeline | "
                        "{ read value; printf '%s\\n' \"$value\"; }",
                    ),
                    "pipeline",
                ),
                require_contains(
                    "truncate redirection",
                    send_command(
                        shell,
                        'read value < /tmp/redirection; printf "%s\\n" "$value"',
                    ),
                    "redirection",
                ),
                require_contains(
                    "append redirection",
                    send_command(
                        shell,
                        "while read value; do printf '%s\\n' \"$value\"; "
                        "done < /tmp/redirection",
                    ),
                    "append",
                ),
                require_contains(
                    "pipeline redirection",
                    send_command(
                        shell,
                        'read value < /tmp/pipeline; printf "%s\\n" "$value"',
                    ),
                    "pipeline-file",
                ),
                require_contains(
                    "redirection overrides pipe",
                    send_command(
                        shell,
                        'read value < /tmp/order; printf "%s\\n" "$value"',
                    ),
                    "explicit",
                ),
                require_contains(
                    "large pipe EOF",
                    send_command(
                        shell,
                        'count=0; while test "$count" -lt 128; do '
                        "echo 0123456789abcdef; "
                        "count=$((count + 1)); done | "
                        "while IFS= read -r value; do :; done; "
                        "printf '%s\\n' LARGE_PIPE_EOF_OK",
                    ),
                    "LARGE_PIPE_EOF_OK",
                ),
                require_contains(
                    "background wait",
                    send_command(
                        shell,
                        '/bin/true & child=$!; wait "$child"; status=$?; '
                        'printf "BACKGROUND_WAIT_STATUS_%s\\n" "$status"',
                    ),
                    "BACKGROUND_WAIT_STATUS_0",
                ),
                require_timer_preemption(shell),
                require_signal_job_control(shell),
                require_shell_language_command_cases(shell),
                require_shell_grammar_command_cases(shell),
                require_printf_utility_command_cases(shell),
                require_command_substitution_alias_boundary(shell),
                require_repeated_process_lifecycle(shell),
                require_repeated_pipeline_lifecycle(shell),
            ]
        )

        shell.close()

        if not all(results):
            sys.exit(1)
        sys.exit(0)
    except (ConnectionRefusedError, OSError) as exc:
        print(f"FAIL: could not connect to x86_64 shell: {exc}")
        sys.exit(2)
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()


if __name__ == "__main__":
    main()
