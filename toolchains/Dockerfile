ARG GCC_IMAGE=gcc:7.3.0
FROM ${GCC_IMAGE}

COPY .cache/tools/linux/7zzs /usr/local/bin/7zzs
COPY .cache/tools/cmake-3.16.9-Linux-x86_64.tar.gz /tmp/cmake.tar.gz
COPY .cache/qt-archives/qtbase-Linux-RHEL_7_4-GCC-Linux-RHEL_7_4-X86_64.7z /tmp/qtbase.7z
COPY .cache/qt-archives/icu-linux-Rhel7.2-x64.7z /tmp/icu.7z
COPY .cache/debian-packages/ /tmp/debian-packages/

RUN chmod 0755 /usr/local/bin/7zzs \
    && mkdir -p /opt/Qt \
    && tar -xzf /tmp/cmake.tar.gz -C /opt \
    && /usr/local/bin/7zzs x /tmp/qtbase.7z -o/opt/Qt -y \
    && /usr/local/bin/7zzs x /tmp/icu.7z -o/opt/Qt -y \
    && for package in /tmp/debian-packages/*.deb; do dpkg-deb -x "$package" /; done \
    && mkdir -p /usr/include/libdrm \
    && ln -s /usr/include/GL /usr/include/libdrm/GL \
    && ldconfig \
    && rm -rf /tmp/cmake.tar.gz /tmp/qtbase.7z /tmp/icu.7z /tmp/debian-packages

COPY toolchains/qt.conf /opt/Qt/5.12.8/gcc_64/bin/qt.conf

ENV PATH="/opt/cmake-3.16.9-Linux-x86_64/bin:/opt/Qt/5.12.8/gcc_64/bin:${PATH}"
ENV CMAKE_PREFIX_PATH="/opt/Qt/5.12.8/gcc_64"
ENV QT_QPA_PLATFORM="offscreen"
ENV QT_PLUGIN_PATH="/opt/Qt/5.12.8/gcc_64/plugins"
ENV LD_LIBRARY_PATH="/opt/Qt/5.12.8/gcc_64/lib"

WORKDIR /workspace
