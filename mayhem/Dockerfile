# syntax=docker/dockerfile:1.7
#
# Commit image for crosspoint-reader (ESP32 e-ink reader firmware, C++20).
# The firmware itself only builds under PlatformIO for ESP32; the fuzz harnesses compile the
# same lib/ parsers on the host against the upstream host-test stubs (test/<suite>/stubs), and
# the oracle is upstream's own host gtest suite (test/CMakeLists.txt).
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# expat (system EXPAT for the content_opf_parser host test), zlib for inflate_reader's test.
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends libexpat1-dev zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

# The host test suite FetchContent's googletest v1.17.0, ArduinoJson v7.4.2 (BlePageTurner tests)
# and AnimatedGIF @67cfacaf; cache both
# from pinned tarballs so build.sh runs offline (SPEC §6.2 item 19). Source only: googletest is
# built by the test CMake project via FETCHCONTENT_SOURCE_DIR_GOOGLETEST; AnimatedGIF is copied
# into the build tree by build.sh (upstream's patch_animatedgif.py patches it in place).
ARG GTEST_VER=1.17.0
ARG GTEST_SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
ARG AGIF_REV=67cfacafe12ef4926c7713342c57d2a4af040e5e
ARG AGIF_SHA256=273fc39969c2c1144644c8d1d2f6c70b5506f5267c537692ecad57f4e6997e72
ARG ARDUINOJSON_VER=7.4.2
ARG ARDUINOJSON_SHA256=681f703dd237f5b7f1dc1d7009a9cf246e88676b349572e73eae9154e8994a55
# JPEGDEC @86282979 (platformio.ini lib_deps) for the fuzz_jpeg harness; build.sh copies it and
# applies upstream's scripts/jpegdec_patches, as scripts/patch_jpegdec.py does for the firmware.
ARG JPEGDEC_REV=86282979224c8a32fd51e091ed5a35b0c699a52b
ARG JPEGDEC_SHA256=ce1cb7641c64a338f1f8599ddde2335e9c00b80fafe580eaf3841fb7269a434b
RUN d=/opt/toolchains/deps/googletest-${GTEST_VER} && mkdir -p "$d/src" \
 && curl -fsSL -o /tmp/gt.tgz https://github.com/google/googletest/archive/refs/tags/v${GTEST_VER}.tar.gz \
 && echo "${GTEST_SHA256}  /tmp/gt.tgz" | sha256sum -c - \
 && tar -xzf /tmp/gt.tgz --strip-components=1 -C "$d/src" && rm /tmp/gt.tgz \
 && d=/opt/toolchains/deps/animatedgif-${AGIF_REV} && mkdir -p "$d/src" \
 && curl -fsSL -o /tmp/ag.tgz https://github.com/bitbank2/AnimatedGIF/archive/${AGIF_REV}.tar.gz \
 && echo "${AGIF_SHA256}  /tmp/ag.tgz" | sha256sum -c - \
 && tar -xzf /tmp/ag.tgz --strip-components=1 -C "$d/src" && rm /tmp/ag.tgz \
 && d=/opt/toolchains/deps/arduinojson-${ARDUINOJSON_VER} && mkdir -p "$d/src" \
 && curl -fsSL -o /tmp/aj.tgz https://github.com/bblanchon/ArduinoJson/archive/refs/tags/v${ARDUINOJSON_VER}.tar.gz \
 && echo "${ARDUINOJSON_SHA256}  /tmp/aj.tgz" | sha256sum -c - \
 && tar -xzf /tmp/aj.tgz --strip-components=1 -C "$d/src" && rm /tmp/aj.tgz \
 && d=/opt/toolchains/deps/jpegdec-${JPEGDEC_REV} && mkdir -p "$d/src" \
 && curl -fsSL -o /tmp/jd.tgz https://github.com/bitbank2/JPEGDEC/archive/${JPEGDEC_REV}.tar.gz \
 && echo "${JPEGDEC_SHA256}  /tmp/jd.tgz" | sha256sum -c - \
 && tar -xzf /tmp/jd.tgz --strip-components=1 -C "$d/src" && rm /tmp/jd.tgz \
 && chmod -R a+rX /opt/toolchains/deps
ENV ARDUINOJSON_SRC=/opt/toolchains/deps/arduinojson-7.4.2/src \
    GTEST_SRC=/opt/toolchains/deps/googletest-1.17.0/src \
    ANIMATEDGIF_SRC=/opt/toolchains/deps/animatedgif-67cfacafe12ef4926c7713342c57d2a4af040e5e/src \
    JPEGDEC_SRC=/opt/toolchains/deps/jpegdec-86282979224c8a32fd51e091ed5a35b0c699a52b/src

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
