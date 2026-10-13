#!/usr/bin/env bash
set -euo pipefail
vm_project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
vm_runtime_dir="$vm_project_dir/.vm"
mkdir -p "$vm_runtime_dir"
if command -v qemu-system-x86_64 >/dev/null 2>&1; then
    vm_qemu=$(command -v qemu-system-x86_64)
    vm_firmware_args=()
else
    vm_bundle="$vm_runtime_dir/qemu/usr"
    vm_qemu="$vm_bundle/bin/qemu-system-x86_64"
    if [[ ! -x "$vm_qemu" ]]; then
        echo 'QEMU is unavailable. Install qemu-system-x86 and qemu-system-gui.' >&2
        exit 1
    fi
    export LD_LIBRARY_PATH="$vm_bundle/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export QEMU_MODULE_DIR="$vm_bundle/lib/x86_64-linux-gnu/qemu"
    vm_firmware_args=(-L "$vm_runtime_dir/firmware" -bios "$vm_runtime_dir/firmware/bios-256k.bin")
fi
vm_iso=${1:-"$vm_project_dir/ivseznayfews.iso"}
if [[ ! -f "$vm_iso" ]]; then
    echo "ISO not found: $vm_iso" >&2
    exit 1
fi
if [[ ! -f "$vm_runtime_dir/disk-v2.img" ]]; then
    python3 "$vm_project_dir/tools/make_disk.py" "$vm_runtime_dir/disk-v2.img"
fi
exec "$vm_qemu" "${vm_firmware_args[@]}" \
    -name 'Ivseznayfews OS 64-bit - ISO test' \
    -accel tcg -cpu qemu64 -smp "${VM_CPUS:-4}" -m 128M \
    -boot order=d -cdrom "$vm_iso" \
    -display gtk,gl=off \
    -drive "file=$vm_runtime_dir/disk-v2.img,format=raw,if=ide,snapshot=on" \
    -netdev user,id=net0 -device e1000,netdev=net0,romfile= \
    -serial "file:$vm_runtime_dir/serial.log" \
    -monitor "unix:$vm_runtime_dir/monitor.sock,server=on,wait=off"
