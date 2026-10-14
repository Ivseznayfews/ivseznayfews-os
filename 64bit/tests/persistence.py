#!/usr/bin/env python3
"""Run two real guest boots against a disposable writable disk, then inspect it."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
from disk_check import inspect
project=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='os-persistence-') as tmp:
    disk=Path(tmp)/'disk.img'
    subprocess.run(['python3',str(project/'tools/make_disk.py'),str(disk)],check=True)
    env=os.environ.copy()
    extra=env.get('QEMU_EXTRA','')
    if '-drive' in shlex.split(extra):raise SystemExit('QEMU_EXTRA must not contain a disk for persistence.py')
    for key in ('NETWORK_TEST','SYSTEM_TEST','PERSIST_WRITE','PERSIST_READ','EXPECT_CPUS','TEST_PAYLOAD'):
        env.pop(key,None)
    env['QEMU_EXTRA']=extra+' -smp 4 -drive '+shlex.quote(f'file={disk},format=raw,if=ide')
    env.setdefault('SMOKE_SECONDS','3')
    env['SYSTEM_TEST']='1';env['EXPECT_CPUS']='4';env['PERSIST_WRITE']='1'
    subprocess.run(['python3',str(project/'tests/smoke.py')],env=env,check=True)
    for key in ('SYSTEM_TEST','PERSIST_WRITE','EXPECT_CPUS'):env.pop(key,None)
    env['PERSIST_READ']='1'
    subprocess.run(['python3',str(project/'tests/smoke.py')],env=env,check=True)
    inspect(disk)
    if shutil.which('fsck.fat'):
        subprocess.run(['fsck.fat','-n',str(disk)],check=True)
    print('PASS: writable guest disk survives shutdown and cold boot')
