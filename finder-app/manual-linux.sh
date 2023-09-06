#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
#KERNEL_VERSION=v5.1.10
#BUSYBOX_VERSION=1_33_1
KERNEL_VERSION=v5.19
BUSYBOX_VERSION=1_36_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-


if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}


    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make ARCH=${ARCH} -j4 CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
    # TODO: Add your kernel build steps here

fi

echo "Adding the Image in outdir"
cp "$OUTDIR/linux-stable/arch/arm64/boot/Image" "$OUTDIR"


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf "${OUTDIR}/rootfs"
    mkdir "${OUTDIR}/rootfs"
    cd "${OUTDIR}/rootfs"
    mkdir -p bin dev etc home lib lib64 proc sbin sys temp usr var
    mkdir -p usr/bin usr/lib usr/sbin
    mkdir -p var/log
fi


# TODO: Create necessary base directories


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX="${OUTDIR}/rootfs" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

cd "$OUTDIR/rootfs"
echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
TOOLCHAIN_DIR=/usr/aarch64-linux-gnu/lib
cp ${TOOLCHAIN_DIR}/ld-linux-aarch64.so.1 lib/
cp ${TOOLCHAIN_DIR}/libm.so.6 lib64/
cp ${TOOLCHAIN_DIR}/libresolv.so.2 lib64/
cp ${TOOLCHAIN_DIR}/libc.so.6 lib64/
cp ${TOOLCHAIN_DIR}/libm.so.6 lib/
cp ${TOOLCHAIN_DIR}/libresolv.so.2 lib/
cp ${TOOLCHAIN_DIR}/libc.so.6 lib/

# TODO: Make device nodes
if [ ! -c "${OUTDIR}/rootfs/dev/console" ]
then
    sudo mknod -m 600 dev/console c 5 1
    sudo mknod -m 666 dev/null c 1 3
    sudo mknod -m 666 dev/tty c 5 0
    # sudo chown root:tty dev/{console,tty}
fi

# TODO: Clean and build the writer utility
cd "$FINDER_APP_DIR"
make clean
make CROSS_COMPILE=${CROSS_COMPILE}


# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp finder.sh "$OUTDIR/rootfs/home"
cp finder-test.sh "$OUTDIR/rootfs/home"
cp writer "$OUTDIR/rootfs/home"
cp autorun-qemu.sh "$OUTDIR/rootfs/home"
cp -rL conf "$OUTDIR/rootfs/home/"

# TODO: Chown the root directory
sudo chown -R root:root "$OUTDIR/rootfs"

# TODO: Create initramfs.cpio.gz
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > "$OUTDIR/initramfs.cpio"
cd "$OUTDIR"
gzip -f initramfs.cpio