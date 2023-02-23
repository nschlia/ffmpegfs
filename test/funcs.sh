PATH=$PWD/../src:$PATH
export LC_ALL=C

if [ "$(expr substr $(uname -s) 1 6)" != "CYGWIN" ]; then
    CYGWIN=0
else
    CYGWIN=1
    . ../setenv
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

    if [ "$(expr substr $(uname -s) 1 6)" != "CYGWIN" ]; then
        # Unmount all
        hash fusermount 2>&- && fusermount -u "${DIRNAME}" || umount -l "${DIRNAME}"
        # Remove temporary directories
	rmdir "${DIRNAME}"
    else
    	# Cygwin: Kill all ffmpegfs processes, sorry, found no better way
    	PROCESS=`ps -W | grep ffmpegfs`
    	if [ ! -z "${PROCESS}" ];
    	then
    		echo ${PROCESS} | awk '{print $1}' | xargs kill -f;
    	fi
    fi

    # Remove temporary directories
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

SRCDIR="$( cd "${BASH_SOURCE%/*}/srcdir" && pwd )"
if [ "${CYGWIN}" != "1" ]; then
    DIRNAME="$(mktemp -d)"
else
    # Find first free drive
    drivelist=`ls /cygdrive/`
    for windrive in a b c d e f g h i j k l m n o p q r s t u v w x y z
    do
       # Just check if drive letter exists, does not need to be accessible
       if [[ "${drivelist}" != *"${windrive}"* ]];
       then
          DIRNAME=${windrive}:
          break;
       fi
    done

    if [ -z ${DIRNAME} ];
    then
    	echo "ERROR: No free Windows drive letter available."
    	exit 99
    fi

    CYGDIR=`cygpath -u "${DIRNAME}"`

    echo "Using drive letter ${DIRNAME} (Cygwin path ${CYGDIR})"
fi
CACHEPATH="$(mktemp -d)"
TMPPATH="$(mktemp -d)"

#--disable_cache
( ffmpegfs -f "${SRCDIR}" "${DIRNAME}" --logfile=${0##*/}${EXTRANAME}_builtin.log --log_maxlevel=TRACE --cachepath="${CACHEPATH}" --desttype=${DESTTYPE} ${ADDOPT} > /dev/null || kill -USR1 $$ ) &

if [ "${CYGWIN}" != "1" ]; then
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
