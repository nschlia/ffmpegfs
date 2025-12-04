FROM debian as common

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update
RUN apt-get install -yq apt-utils
RUN apt install fuse3 libfuse3-4 libsqlite3-0 libavcodec59 libavformat61 libswresample4 libavutil57 libswscale8 libavfilter10 libcue2 libchardet1
RUN apt-get install -yq libdvdread[48] libdvdnav4 libbluray2

FROM common as builder

RUN apt-get install -yq gcc g++ make pkg-config autoconf asciidoc-base w3m libchromaprint-dev bc doxygen graphviz curl git xxd
RUN apt-get install -yq libfuse3-dev libsqlite3-dev libavcodec-dev libavformat-dev libswresample-dev libavutil-dev libswscale-dev libavfilter-dev libcue-dev libchardet-dev
RUN apt-get install -yq libdvdread-dev libdvdnav-dev libbluray-dev

ARG BRANCH=master
# Ensure that we get a docker image cache invalidation when there's new content available
ADD https://api.github.com/repos/nschlia/ffmpegfs/compare/${BRANCH}...HEAD /dev/null

RUN git clone https://github.com/nschlia/ffmpegfs.git -b ${BRANCH}
WORKDIR ffmpegfs

RUN chmod +x autogen.sh src/makehelp.sh
RUN ./autogen.sh
RUN ./configure
RUN make -s
RUN make -s install

FROM common

COPY --from=builder /usr/local/bin/ffmpegfs /usr/local/bin/ffmpegfs
COPY --from=builder /usr/local/share/man/man1/ffmpegfs.1 /usr/local/share/man/man1/ffmpegfs.1

ENTRYPOINT ["/usr/local/bin/ffmpegfs", "/src", "/dst"]
CMD = --help
