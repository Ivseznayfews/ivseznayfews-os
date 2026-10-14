#!/usr/bin/env python3
"""Create a new 64 MiB FAT32 test disk. Never overwrites an existing image."""
import struct
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
path=Path(sys.argv[1]) if len(sys.argv)>1 else root/'.vm'/'disk-v2.1.img'
files={'README.TXT':b'Ivseznayfews FAT32 test disk. Read through VFS and syscall.\n'}
for name in ['hello','badptr','probe','spin','badstack','sleep','worker','writer']:
    files[name.upper()+'.ELF']=(root/'user'/f'{name}.elf').read_bytes()
sectors=131072; reserved=32; fats=2; fat_size=1009; first_data=reserved+fats*fat_size
boot=bytearray(512);boot[:3]=b'\xeb\x58\x90';boot[3:11]=b'IVSE OS '
struct.pack_into('<HBHBHHBHHHII',boot,11,512,1,reserved,fats,0,0,0xf8,0,63,255,0,sectors)
struct.pack_into('<IHHIHH',boot,36,fat_size,0,0,2,1,6)
boot[64]=0x80;boot[66]=0x29;struct.pack_into('<I',boot,67,0x49565345)
boot[71:82]=b'IVSE DISK  ';boot[82:90]=b'FAT32   ';boot[510:]=b'\x55\xaa'
fat=bytearray(fat_size*512)
for index,value in enumerate([0x0ffffff8,0x0fffffff,0x0fffffff]):struct.pack_into('<I',fat,index*4,value)
root_dir=bytearray(512);root_dir[:11]=b'IVSE DISK  ';root_dir[11]=0x08;chunks=[];cluster=3
for index,(name,data) in enumerate(files.items()):
    first=cluster;count=(len(data)+511)//512
    for j in range(count):struct.pack_into('<I',fat,(cluster+j)*4,cluster+j+1 if j+1<count else 0x0fffffff)
    short,ext=name.split('.');offset=(index+1)*32
    root_dir[offset:offset+11]=(short.ljust(8)+ext.ljust(3)).encode()
    root_dir[offset+11]=0x20
    struct.pack_into('<H',root_dir,offset+20,first>>16);struct.pack_into('<H',root_dir,offset+26,first&65535)
    struct.pack_into('<I',root_dir,offset+28,len(data));chunks.append((first,data));cluster+=count
fsi=bytearray(512);struct.pack_into('<I',fsi,0,0x41615252);struct.pack_into('<I',fsi,484,0x61417272)
struct.pack_into('<II',fsi,488,sectors-first_data-(cluster-2),cluster);struct.pack_into('<I',fsi,508,0xaa550000)
path.parent.mkdir(parents=True,exist_ok=True)
with path.open('xb') as out:
    out.truncate(sectors*512)
    for lba,data in [(0,boot),(1,fsi),(6,boot),(7,fsi),(reserved,fat),(reserved+fat_size,fat),(first_data,root_dir)]:
        out.seek(lba*512);out.write(data)
    for c,data in chunks:out.seek((first_data+c-2)*512);out.write(data)
print(f'Created {path}: FAT32, 64 MiB, {len(files)} files')
