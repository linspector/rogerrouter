MXE=$(pwd)/mxe
MXE_SYSROOT=$MXE/usr/i686-w64-mingw32.shared.posix

echo Creating setup with files from $MXE_SYSROOT .

cd ../../
makensis -dMXE_SYSROOT=$MXE_SYSROOT -DPREFIX=$MXE_SYSROOT platform/windows/build/platform/windows/roger.nsi
