PLATFORM=arm64
O=b/c_$(PLATFORM)
IS_X64=
IS_X86=
IS_ARM64=1
CROSS_COMPILE=

MY_ARCH=-march=armv8-a
MY_ARCH=-march=armv8-a+crypto+crc
MY_ARCH=-march=armv8.3-a+crypto+crc
MY_ARCH=

USE_ASM=
USE_ASM=1
export CC=$(CROSS_COMPILE)clang
export CXX=$(CROSS_COMPILE)clang++
USE_CLANG=1
