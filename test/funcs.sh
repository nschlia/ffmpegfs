PATH=$PWD/../src:$PATH
export LC_ALL=C

if [ "$(expr substr $(uname -s) 1 6)" != "CYGWIN" ]; then
    CYGWIN=0
else
    CYGWIN=1
fi

if [ ! -z "$2" ]
then
    EXITCODE="$2"
else
    EXITCODE=99
fi

cleanup () {
    EXIT=$?
    echo "Return code: ${EXIT}"
    # Errors are no longer fatal
    set +e
    # Unmount all
    hash fusermount 2>&- && fusermount -u "${DIRNAME}" || umount -l "${DIRNAME}"
    # Remove temporary directories
    rmdir "${DIRNAME}"
    rm -Rf "${CACHEPATH}"
	rm -Rf "${TMPPATH}"
    # Arrividerci
    exit ${EXIT}
}

ffmpegfserr () {
    echo "***TEST FAILED***"
    echo "Return code: ${EXITCODE}"
    exit ${EXITCODE}
}

check_filesize() {
    MIN="$2"
    MAX="$3"
    SHORT="$5"

    if [ "${FILEEXT}" != "hls" ]
    then
        FILE="$1.${FILEEXT}"
    else
        FILE="$1"
    fi

    if [ -z ${MAX}  ]
    then
        MAX=${MIN}
    fi

    if [ -z "$4" ]
    then
    	SIZE=$(stat -c %s "${DIRNAME}/${FILE}")
    else
	SIZE=$(stat -c %s "${4}/${FILE}")
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

set -e
trap cleanup EXIT
trap ffmpegfserr USR1
DESTTYPE=$1
EXTRANAME=$3
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
if [ "${EXTRANAME}" == "hls" ];
then
    EXTRANAME=
fi
if [ ! -z "${EXTRANAME}" ];
then
    EXTRANAME=_$EXTRANAME
fi

if [ "$CYGWIN" != "1" ]; then
    DIRNAME="$(mktemp -d)"
else
    DIRNAME=A:
    CYGDIR=`cygpath -u "${DIRNAME}"`
fi

SRCDIR="$( cd "${BASH_SOURCE%/*}/srcdir" && pwd )"
CACHEPATH="$(mktemp -d)"
TMPPATH="$(mktemp -d)"

#--disable_cache
( ffmpegfs -f "${SRCDIR}" "${DIRNAME}" --logfile=${0##*/}${EXTRANAME}.builtin.log --log_maxlevel=TRACE --cachepath="${CACHEPATH}" --desttype=${DESTTYPE} ${ADDOPT} > /dev/null 2>&1 || kill -USR1 $$ ) &

if [ "$CYGWIN" != "1" ]; then
    while ! mount | grep -q "${DIRNAME}" ; do
        sleep 0.1
    done
else
    DIRNAME=${CYGDIR}
    until [ -d "${DIRNAME}" ]
    do
        sleep 0.1
    done
fi
