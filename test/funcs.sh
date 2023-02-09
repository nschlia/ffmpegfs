PATH=$PWD/../src:$PATH
export LC_ALL=C

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
    echo "File: ${FILE}"
    if [ ${MIN} -eq ${MAX} ]
    then
        echo "Size: ${SIZE} (expected ${MIN})"
    else
        echo "Size: ${SIZE} (expected ${MIN}...${MAX})"
    fi

    if [ ${SIZE} -ge ${MIN} -a ${SIZE} -le ${MAX} ]
    then
        echo "Pass"
    else
        echo "FAIL!"
        exit 1
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

SRCDIR="$( cd "${BASH_SOURCE%/*}/srcdir" && pwd )"
DIRNAME="$(mktemp -d)"
CACHEPATH="$(mktemp -d)"
TMPPATH="$(mktemp -d)"

#--disable_cache
( ffmpegfs -f "${SRCDIR}" "${DIRNAME}" --logfile=${0##*/}${EXTRANAME}.builtin.log --log_maxlevel=TRACE --cachepath="${CACHEPATH}" --desttype=${DESTTYPE} ${ADDOPT} > /dev/null || kill -USR1 $$ ) &
while ! mount | grep -q "${DIRNAME}" ; do
    sleep 0.1
done
