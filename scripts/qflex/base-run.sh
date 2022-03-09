#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
DIR=$SCRIPT_DIR
QEMU_VER=$1
IMAGES=$DIR/$2
PORT=$3
SNAPSHOT=$3

# -singlestepping -d nochain \
# --accel tcg,thread=single -singlestep -d nochain \
# --accel tcg,thread=single \
# -icount shift=0,sleep=off \
# -qflex-gen-mem-trace core_count=4 \

CMD=$DIR/../../build/aarch64-softmmu/qemu-system-aarch64

gdb --args \
	$CMD \
  -cpu cortex-a57 -M virt,gic-version=3 \
  -smp 1 -m 8G \
  -nographic -rtc clock=vm \
  --accel tcg,thread=single \
  -D qemu-log \
  -global virtio-blk-device.scsi=off \
  -device virtio-scsi-device,id=scsi0 \
  -device virtio-scsi-device,id=scsi1 \
  -device virtio-scsi-pci,id=scsi2 \
  -device scsi-hd,drive=hd0 \
  -netdev user,id=net0,hostfwd=tcp::2240-:22 \
  -boot menu=on \
  -device virtio-net-device,netdev=net0,mac=52:54:00:00:01:00 \
  -drive file=$IMAGES/flash0.qcow2,format=qcow2,if=pflash \
  -drive file=$IMAGES/flash1.qcow2,format=qcow2,if=pflash \
  -drive file=$IMAGES/ubuntu.qcow2,format=qcow2,id=hd0,if=none \
  -loadvm $SNAPSHOT
