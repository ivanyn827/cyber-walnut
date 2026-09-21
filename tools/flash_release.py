"""Portable share-package flasher. Never reads or writes device NVS."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--port',required=True)
    parser.add_argument('--build',action='store_true')
    parser.add_argument('--dry-run',action='store_true')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    folder=root/('build' if args.build else 'release')
    names=['salary_counter.ino.bootloader.bin','salary_counter.ino.partitions.bin','salary_counter.ino.bin','boot_app0.bin']
    if not args.build:
        expected=dict(line.split('  ',1)[::-1] for line in (folder/'SHA256SUMS').read_text().splitlines())
        for name in names:
            if hashlib.sha256((folder/name).read_bytes()).hexdigest()!=expected.get(name):
                raise SystemExit('Checksum mismatch: '+name)
    for name in names:
        if not (folder/name).is_file():raise SystemExit('Missing '+name)
    command=[sys.executable,'-m','esptool','--chip','esp32c6','--port',args.port,'write_flash',
             '0x0',str(folder/names[0]),'0x8000',str(folder/names[1]),'0xe000',str(folder/names[3]),'0x10000',str(folder/names[2])]
    if args.dry_run:
        print('Validated images; writes bootloader @0, partition table @0x8000, boot selection @0xe000, app @0x10000; no NVS erase')
    else:
        from serial_lock import port_lock
        with port_lock(timeout=15):subprocess.run(command,check=True)

if __name__=='__main__':main()
