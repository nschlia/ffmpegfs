FROM debian:trixie AS common

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -yq apt-utils \
 && apt-get install -yq \
    fuse3 libfuse3-4 libsqlite3-0 \
    libavcodec61 libavformat61 libswresample5 libavutil59 \
    libswscale8 libavfilter10 libcue2 libchardet1 \
    libdvdread8 libdvdnav4 libbluray2 \
 && rm -rf /var/lib/apt/lists/*

FROM common AS builder

RUN apt-get update \
 && apt-get install -yq \
    gcc g++ make pkg-config autoconf asciidoc-base w3m \
    libchromaprint-dev bc doxygen graphviz curl git xxd \
    libfuse3-dev libsqlite3-dev \
    libavcodec-dev libavformat-dev libswresample-dev libavutil-dev \
    libswscale-dev libavfilter-dev libcue-dev libchardet-dev \
    libdvdread-dev libdvdnav-dev libbluray-dev \
 && rm -rf /var/lib/apt/lists/*

ARG BRANCH=master

RUN git clone https://github.com/nschlia/ffmpegfs.git -b ${BRANCH}
WORKDIR /ffmpegfs

RUN chmod +x autogen.sh src/makehelp.sh
RUN ./autogen.sh
RUN ./configure
RUN make -s
RUN make -s install

FROM common

COPY --from=builder /usr/local/bin/ffmpegfs /usr/local/bin/ffmpegfs
COPY --from=builder /usr/local/share/man/man1/ffmpegfs.1 /usr/local/share/man/man1/ffmpegfs.1

ENTRYPOINT ["/usr/local/bin/ffmpegfs", "/src", "/dst"]
CMD ["--help"]
