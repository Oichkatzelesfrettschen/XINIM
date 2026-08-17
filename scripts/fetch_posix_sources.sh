#!/bin/sh
# Acquire the official POSIX Issue 7, 2018 HTML archive for local use.

set -eu

SCRIPT_DIRECTORY=$(CDPATH= cd -P -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(dirname -- "$SCRIPT_DIRECTORY")
ARCHIVE_URL=https://pubs.opengroup.org/onlinepubs/9699919799/download/susv4-2018.tgz
ARCHIVE_NAME=susv4-2018.tgz
MOZILLA_USER_AGENT='Mozilla/5.0 (X11; Linux x86_64; rv:141.0) Gecko/20100101 Firefox/141.0'
DOWNLOAD_DIRECTORY=$REPOSITORY_ROOT/build/_state/downloads/posix
EXTRACT_ROOT=$REPOSITORY_ROOT/build/_state/cache/posix
TRACKED_ARCHIVE_PATH=$REPOSITORY_ROOT/data/external/posix/$ARCHIVE_NAME
ARCHIVE_PATH=$DOWNLOAD_DIRECTORY/$ARCHIVE_NAME
RESPONSE_LOG=$DOWNLOAD_DIRECTORY/susv4-2018.response.log
OFFLINE=0

usage() {
    printf '%s\n' \
        "usage: $0 [--offline] [--download-directory PATH] [--extract-root PATH]"
}

while [ "$#" -gt 0 ]; do
    case $1 in
        --offline)
            OFFLINE=1
            shift
            ;;
        --download-directory)
            [ "$#" -ge 2 ] || {
                usage >&2
                exit 2
            }
            DOWNLOAD_DIRECTORY=$2
            ARCHIVE_PATH=$DOWNLOAD_DIRECTORY/$ARCHIVE_NAME
            RESPONSE_LOG=$DOWNLOAD_DIRECTORY/susv4-2018.response.log
            shift 2
            ;;
        --extract-root)
            [ "$#" -ge 2 ] || {
                usage >&2
                exit 2
            }
            EXTRACT_ROOT=$2
            shift 2
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            printf 'unknown argument: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

mkdir -p -- "$DOWNLOAD_DIRECTORY" "$EXTRACT_ROOT"

if [ "$OFFLINE" -eq 1 ] && [ ! -f "$ARCHIVE_PATH" ] &&
   [ -f "$TRACKED_ARCHIVE_PATH" ]; then
    ARCHIVE_PATH=$TRACKED_ARCHIVE_PATH
fi

if [ "$OFFLINE" -eq 0 ]; then
    PARTIAL_ARCHIVE=$DOWNLOAD_DIRECTORY/.susv4-2018.tgz.partial.$$
    PARTIAL_RESPONSE=$DOWNLOAD_DIRECTORY/.susv4-2018.response.partial.$$
    cleanup_partial_files() {
        rm -f -- "$PARTIAL_ARCHIVE" "$PARTIAL_RESPONSE"
    }
    trap cleanup_partial_files EXIT HUP INT TERM

    wget \
        --user-agent="$MOZILLA_USER_AGENT" \
        --https-only \
        --max-redirect=5 \
        --timeout=30 \
        --tries=3 \
        --server-response \
        --output-document="$PARTIAL_ARCHIVE" \
        "$ARCHIVE_URL" \
        2>"$PARTIAL_RESPONSE"

    python3 "$SCRIPT_DIRECTORY/verify_posix_issue7_archive.py" \
        --repo-root "$REPOSITORY_ROOT" \
        --archive "$PARTIAL_ARCHIVE"

    mv -- "$PARTIAL_ARCHIVE" "$ARCHIVE_PATH"
    mv -- "$PARTIAL_RESPONSE" "$RESPONSE_LOG"
    trap - EXIT HUP INT TERM
fi

python3 "$SCRIPT_DIRECTORY/verify_posix_issue7_archive.py" \
    --repo-root "$REPOSITORY_ROOT" \
    --archive "$ARCHIVE_PATH" \
    --extract-root "$EXTRACT_ROOT" \
    --extract

printf '%s\n' \
    "Verified SUSv4 Issue 7 archive: $ARCHIVE_PATH" \
    "Verified extracted corpus: $EXTRACT_ROOT/susv4-2018"
