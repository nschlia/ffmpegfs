PATH=$PWD/../src:$PATH
export LC_ALL=C

TMP_BASE="${TMPDIR:-/tmp}"
TMP_BASE="$(cd "${TMP_BASE}" && pwd -P)"

DIRNAME=""
CACHEPATH=""
TMPPATH=""
TEST_TMPDIRS=()

EXITCODE="${2:-99}"

register_test_tmpdir() {
    local path="${1:-}"

    [ -n "${path}" ] || return 0
    TEST_TMPDIRS+=("${path}")
}

make_test_tmpdir() {
    local varname="${1:?missing temporary directory variable name}"
    local kind="${2:?missing temporary directory kind}"
    local path

    case "${varname}" in
        ''|*[!A-Za-z0-9_]*)
            echo "ERROR: invalid temporary directory variable name '${varname}'" >&2
            return 1
            ;;
    esac

    case "${kind}" in
        mnt|cache|tmp)
            ;;
        *)
            echo "ERROR: invalid temporary directory kind '${kind}'" >&2
            return 1
            ;;
    esac

    path="$(mktemp -d "${TMP_BASE}/ffmpegfs-${kind}.${0##*/}.XXXXXXXXXX")"
    register_test_tmpdir "${path}"
    printf -v "${varname}" '%s' "${path}"
}

remove_own_tmpdir() {
    local path="${1:-}"

    [ -n "${path}" ] || return 0
    path="${path%/}"
    [ -d "${path}" ] || return 0

    case "${path}" in
        "${TMP_BASE}"/ffmpegfs-mnt.*|"${TMP_BASE}"/ffmpegfs-cache.*|"${TMP_BASE}"/ffmpegfs-tmp.*)
            rm -rf --one-file-system -- "${path}"
            ;;
        *)
            echo "WARNING: refusing to remove unexpected temp path: ${path}" >&2
            ;;
    esac
}
remove_registered_tmpdirs() {
    local path

    for path in "${TEST_TMPDIRS[@]}"
    do
        remove_own_tmpdir "${path}"
    done
}

unmount_own_mountpoint() {
    local path="${1:-}"

    [ -n "${path}" ] || return 0
    path="${path%/}"
    [ -d "${path}" ] || return 0

    case "${path}" in
        "${TMP_BASE}"/ffmpegfs-mnt.*)
            ;;
        *)
            echo "WARNING: refusing to unmount unexpected path: ${path}" >&2
            return 0
            ;;
    esac

    if mountpoint -q -- "${path}"
    then
        if hash fusermount 2>/dev/null
        then
            fusermount -u "${path}" 2>/dev/null || fusermount -uz "${path}" 2>/dev/null || true
        else
            umount "${path}" 2>/dev/null || umount -l "${path}" 2>/dev/null || true
        fi
    fi
}

cleanup () {
    EXIT=$?
    echo "Return code: ${EXIT}"
    # Errors are no longer fatal
    set +e
    # Unmount ffmpegfs first.  If the mount is still busy, use a lazy
    # unmount so the temporary mount directory can still be removed from
    # this test process' namespace.
    unmount_own_mountpoint "${DIRNAME:-}"
    # Remove only the temporary directories created by this test run.
    # This also removes directories whose variable was reassigned later.
    remove_registered_tmpdirs
    # Arrividerci
    exit ${EXIT}
}

ffmpegfserr () {
    echo "***TEST FAILED***"
    echo "Return code: ${EXITCODE}"
    exit ${EXITCODE}
}

check_filesize() {
    MIN="${2:?missing minimum file size}"
    MAX="${3:-}"
    SHORT="${5:-}"

    if [ "${FILEEXT}" != "hls" ]
    then
        FILE="$1.${FILEEXT}"
    else
        FILE="$1"
    fi

    if [ -z "${MAX}" ]
    then
        MAX=${MIN}
    fi

    if [ -z "${4:-}" ]
    then
        SIZE=$(stat -c %s "${DIRNAME}/${FILE}")
    else
        SIZE=$(stat -c %s "${4:-}/${FILE}")
    fi

    if [ ${SIZE} -ge ${MIN} -a ${SIZE} -le ${MAX} ]
    then
        RESULT="Pass"
        RETURN=0
    else
        RESULT="FAIL"
        RETURN=1
    fi

    if [ -z "${SHORT}" ]
    then
        if [ ${MIN} -eq ${MAX} ]
        then
            printf "> %s -> File: %-20s Size: %8i (expected %8i)\n" ${RESULT} ${FILE} ${SIZE} ${MIN}
        else
            printf "> %s -> File: %-20s Size: %8i (expected %8i...%8i)\n" ${RESULT} ${FILE} ${SIZE} ${MIN} ${MAX}
        fi
    else
    if [ ${MIN} -eq ${MAX} ]
    then
        printf "%s -> Size: %8i (expected %8i)\n" ${RESULT} ${SIZE} ${MIN}
    else
        printf "%s -> Size: %8i (expected %8i...%8i)\n" ${RESULT} ${SIZE} ${MIN} ${MAX}
    fi
    fi

    if [ ${RETURN} != 0 ];
    then
            exit ${RETURN}
    fi
}

set -euo pipefail
trap cleanup EXIT
trap ffmpegfserr USR1
DESTTYPE="${1:?missing destination type}"
EXTRANAME="${3:-}"
# Map filenames
if [ "${DESTTYPE}" == "prores" ];
then
# ProRes uses the MOV container
    FILEEXT="mov"
elif [ "${DESTTYPE}" == "alac" ];
then
# ALAC (Apple Lossless Coding) uses the mp4 container
    FILEEXT="m4a"
else
    FILEEXT=${DESTTYPE}
fi
if [ -z "${EXTRANAME}" ];
then
    EXTRANAME=$DESTTYPE
fi
if [ "${EXTRANAME}" == "hls" ] && [[ "${0##*/}" == *_hls ]];
then
    EXTRANAME=
fi
if [ -n "${EXTRANAME}" ];
then
    EXTRANAME=_$EXTRANAME
fi

SRCDIR="$( cd "${BASH_SOURCE%/*}/srcdir" && pwd )"
make_test_tmpdir DIRNAME mnt
make_test_tmpdir CACHEPATH cache
make_test_tmpdir TMPPATH tmp

#--disable_cache
( ffmpegfs -f "${SRCDIR}" "${DIRNAME}" --logfile=${0##*/}${EXTRANAME}_builtin.log --log_maxlevel=TRACE --cachepath="${CACHEPATH}" --desttype=${DESTTYPE} ${ADDOPT:-} > /dev/null || kill -USR1 $$ ) &
while ! mount | grep -q "${DIRNAME}" ; do
    sleep 0.1
done
