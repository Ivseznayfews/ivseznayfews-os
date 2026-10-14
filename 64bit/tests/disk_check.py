#!/usr/bin/env python3
"""Independently inspect the generated FAT32 disk after guest writes."""
import struct
import sys
from pathlib import Path

def inspect(path):
    with Path(path).open('rb') as disk:
        def sector(lba):
            disk.seek(lba*512)
            result=disk.read(512)
            assert len(result)==512
            return result
        boot=sector(0)
        assert boot[510:]==b'\x55\xaa'
        reserved=struct.unpack_from('<H',boot,14)[0]
        spc=boot[13]
        copies=boot[16]
        fat_size=struct.unpack_from('<I',boot,36)[0]
        total=struct.unpack_from('<I',boot,32)[0]
        root=struct.unpack_from('<I',boot,44)[0]
        data_start=reserved+copies*fat_size
        clusters=(total-data_start)//spc
        disk.seek(reserved*512)
        raw=disk.read(fat_size*512)
        for i in range(1,copies):
            disk.seek((reserved+i*fat_size)*512)
            assert disk.read(fat_size*512)==raw, 'FAT mirrors differ'
        fat=[x&0x0fffffff for x in struct.unpack('<'+'I'*(len(raw)//4),raw)]
        free=sum(fat[i]==0 for i in range(2,clusters+2))
        fsi=struct.unpack_from('<H',boot,48)[0]
        backup=struct.unpack_from('<H',boot,50)[0]
        for lba in (fsi,backup+fsi):
            info=sector(lba)
            assert struct.unpack_from('<I',info,488)[0]==free, 'FSInfo free count differs'
        owned=set()
        def chain(first):
            result=[]
            c=first
            while 2<=c<clusters+2:
                assert c not in owned, 'Cluster cycle or shared allocation'
                owned.add(c);result.append(c);c=fat[c]
            if first:
                assert c>=0x0ffffff8, 'Invalid chain terminator'
            return result
        def contents(c):
            return b''.join(sector(data_start+(block-2)*spc+s) for block in chain(c) for s in range(spc))
        files={}
        def directory(c,parent):
            data=contents(c)
            for off in range(0,len(data),32):
                e=data[off:off+32]
                if e[0]==0:break
                if e[0] in (0xe5,ord('.')) or e[11]&8 or e[11]==15:continue
                name=e[:8].decode().rstrip()
                ext=e[8:11].decode().rstrip()
                if ext:name+='.'+ext
                cluster=struct.unpack_from('<H',e,20)[0]<<16|struct.unpack_from('<H',e,26)[0]
                path=parent+name
                if e[11]&16:directory(cluster,path+'/')
                else:
                    size=struct.unpack_from('<I',e,28)[0]
                    body=contents(cluster)
                    assert len(body)>=size
                    files[path]=body[:size]
        directory(root,'')
        allocated={i for i in range(2,clusters+2) if fat[i]}
        assert allocated==owned, 'Lost allocated clusters'
        assert files['PERSIST.TXT']==b'survives-reboot'
        assert files['GUI.TXT']==b'editor saved\nz'
        assert files['USER.TXT']==b'Ring 3 saved this file\n'
        assert files['WORK/TEST.TXT']==b'short'
        assert files['WORK/SUB/NEST.TXT']==b'nested'
        assert files['WORK/EMPTY.TXT']==b''
        for i in range(20):assert files[f'WORK/F{i:02}.TXT']==b''
        print(f'PASS: independent FAT32 inspection; {len(files)} files; mirrors, FSInfo, directory growth, no lost/shared clusters')

if __name__=='__main__':inspect(sys.argv[1])
