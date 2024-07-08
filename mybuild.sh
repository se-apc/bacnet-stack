#!/bin/sh
OECORE_NATIVE_SYSROOT=/opt/poky/rzn1/sysroots/x86_64-pokysdk-linux

sed -e 's#.*<INSERT_CMAKE_INSTALL_PREFIX_HERE>#set\(CMAKE_INSTALL_PREFIX "/opt/poky/rzn1/sysroots/x86_64-pokysdk-linux/usr")#' -i ./CMakeLists.txt
rm -rf _build
mkdir _build
cd _build

export BACDL_ALL=1
export BACDL_BIP6=1
#export BACDL_DEFINE="-DBACDL_ALL=1"
export BACDL_DEFINE="-DBACDL_BIP6=1"
export BACNET_DEFINES+="-D_WIN32=1"
#export BACNET_DEFINES+="-DBACDL_ALL=1 "
#export BACNET_DEFINES+="-DBACDL_BIP=1 "
export BACNET_DEFINES+="-DBACDL_BIP6=1 "
export BACNET_DEFINES+="-DBBMD_CLIENT_ENABLED=1 "
export BACNET_DEFINES+="-D__INSIDE_CYGWIN_NET__=1 "
export BACNET_DEFINES+="-DPRINT_ENABLED=1 "
#export BACNET_DEFINES+="-I/usr/x86_64-w64-mingw32/sys-root/mingw/include/ "
#export BACNET_DEFINES+="-I/usr/x86_64-w64-mingw32/sys-root/mingw/include/"

cmake ../ -G"CodeBlocks - Unix Makefiles"

#cmake ..
VERBOSE=1 cmake --build ../ --target all --
cd -

#make -C ../
make clean
make win32
#make install
# cp -rfv _build/include/* /opt/poky/rzn1/sysroots/x86_64-pokysdk-linux/usr/include/
# cp -fv _build/lib/libbacnet-stack.so /opt/poky/rzn1/sysroots/x86_64-pokysdk-linux/usr/lib/libbacnet-stack.so
