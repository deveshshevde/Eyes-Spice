#!/bin/bash

DEVICE="/dev/sda"
PART="${DEVICE}1"
MOUNT_DIR="$HOME/sdcard_test"

echo "⚠️ WARNING: This will ERASE ALL DATA on $DEVICE"
read -p "Type 'YES' to continue: " confirm

if [ "$confirm" != "YES" ]; then
    echo "❌ Aborted."
    exit 1
fi

echo "🚫 Unmounting..."
sudo umount ${DEVICE}? 2>/dev/null

echo "🧹 Wiping existing partition table..."
sudo wipefs -a $DEVICE

echo "🧱 Creating new partition..."
sudo parted -s $DEVICE mklabel msdos
sudo parted -s $DEVICE mkpart primary fat32 1MiB 100%

echo "⌛ Waiting for kernel to register partition..."
sleep 2

echo "🧾 Formatting partition as FAT32..."
sudo mkfs.vfat -F 32 $PART

echo "📂 Creating mount point: $MOUNT_DIR"
mkdir -p $MOUNT_DIR

echo "🔗 Mounting $PART to $MOUNT_DIR"
sudo mount $PART $MOUNT_DIR

echo "✅ Writing test file to SD card..."
echo "Hello from ESP32 script!" | sudo tee $MOUNT_DIR/test.txt > /dev/null

echo "✅ Done! File saved at: $MOUNT_DIR/test.txt"

echo "⚠️ Don't forget to 'sudo umount $MOUNT_DIR' when done."

