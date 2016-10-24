#!/bin/sh

echo "----"
echo "Itron Riva Edge Board - Debian - Firmware UPDATE"
echo "Version 2016-08-11"
echo "WARNING: YOU MUST BE IN initramfs to run this script after completing U-BOOT install"
echo "Be sure your FAT32 formatted usb drive is mount-"
echo "ed in lower USB port and contains this firmware (not in a folder)."
echo "----"

#
# Confirm user wants to install
#

echo -n "Do you wish to continue (y/n): "
read answer

if [ x$answer == "x" ]; then
	echo "Answer with y or n"
	exit 0
fi
if [ x$answer != "xy" ]; then
	echo "No changes made."
	exit 0
fi

#
# Do the install
#

echo "Wiping the eMMC partition table"
dd if=/dev/zero of=/dev/mmcblk0 bs=1M count=1

echo "Partitioning eMMC"
fdisk /dev/mmcblk0 <<EOF
n
p
1

+256M
n
p
2


w
EOF

echo "Formatting partitions"
mke2fs -L boot -t ext2 /dev/mmcblk0p1
mke2fs -L rw-root -t ext4 /dev/mmcblk0p2

echo "Mounting boot partition and creating boot folder"
mount /dev/mmcblk0p1 /mnt/rawboot
mkdir /mnt/rawboot/boot

echo "Mounting usb drive"
mkdir /mnt/usb
mount /dev/sda1 /mnt/usb

echo "Copying root.squashfs file to the eMMC"
cp /mnt/usb/root.squashfs /mnt/rawboot/boot/

echo "Unmounting both partitions"
umount /mnt/rawboot
umount /mnt/usb



#
# Final Message
#
echo "----"
echo "All done!"
echo "----"

#
# Confirm user wants to reboot
#
echo -n "Press ENTER to reboot now."
read answer
echo b > /proc/sysrq-trigger
