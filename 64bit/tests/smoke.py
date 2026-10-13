#!/usr/bin/env python3
"""Boot the real ISO; verify scheduling, PS/2 shell commands, and shutdown."""
import json
import os
from pathlib import Path
import shlex
import socket
import subprocess
import tempfile
import time

iso = Path(__file__).resolve().parents[1] / 'ivseznayfews.iso'
with tempfile.TemporaryDirectory(prefix='os-smoke-') as directory:
    root = Path(directory)
    command = [os.environ.get('QEMU', 'qemu-system-x86_64')]
    command += shlex.split(os.environ.get('QEMU_EXTRA', ''))
    command += ['-accel', 'tcg', '-m', '128M', '-cdrom', str(iso),
                '-display', 'none', '-nic', 'none', '-serial', f'file:{root}/serial.log',
                '-qmp', f'unix:{root}/qmp,server=on,wait=off', '-no-reboot']
    process = subprocess.Popen(command, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + 15
        while not (root / 'qmp').exists():
            if process.poll() is not None:
                raise RuntimeError(process.stderr.read().decode())
            if time.monotonic() > deadline:
                raise TimeoutError('QMP did not start')
            time.sleep(0.05)
        connection = socket.socket(socket.AF_UNIX)
        while True:
            try:
                connection.connect(str(root / 'qmp'))
                break
            except ConnectionRefusedError:
                if process.poll() is not None:
                    raise RuntimeError(process.stderr.read().decode())
                if time.monotonic() > deadline:
                    raise TimeoutError('QMP connection failed')
                time.sleep(0.05)
        connection.settimeout(10)
        stream = connection.makefile('rwb')
        json.loads(stream.readline())

        def qmp(name, arguments=None):
            stream.write((json.dumps({'execute': name, 'arguments': arguments or {}}) + '\n').encode())
            stream.flush()
            while True:
                response = json.loads(stream.readline())
                if 'error' in response:
                    raise RuntimeError(response)
                if 'return' in response:
                    return response['return']

        def monitor(command):
            return qmp('human-monitor-command', {'command-line': command})

        def screen():
            dump = root / 'vga.bin'
            result = monitor(f'pmemsave 0xb8000 4000 "{dump}"')
            if result:
                raise RuntimeError(result)
            return dump.read_bytes()[::2].decode('ascii', errors='replace')

        def wait_for(check, timeout=15):
            end = time.monotonic() + timeout
            while time.monotonic() < end:
                if process.poll() is not None:
                    raise RuntimeError('Guest exited unexpectedly')
                if check():
                    return
                time.sleep(0.1)
            raise AssertionError('Guest check timed out\n' + (root / 'serial.log').read_text() + '\n' + screen())

        def type_keys(keys):
            for key in keys:
                monitor(f'sendkey {key}')
                time.sleep(0.08)

        qmp('qmp_capabilities')
        wait_for(lambda: '[READY]' in (root / 'serial.log').read_text())
        wait_for(lambda: 'Ivseznayfews> ' in screen())
        type_keys(['h', 'e', 'l', 'p', 'ret'])
        wait_for(lambda: 'show this' in screen())
        type_keys(['e', 'c', 'h', 'o', 'spc', 'o', 'k', 'ret'])
        wait_for(lambda: 'ok' in [screen()[i:i+80].strip() for i in range(0, 2000, 80)])
        for command, expected in [('mem', 'PMM: managed='), ('tasks', 'monitor  ready'),
                                  ('uptime', 'Uptime:'), ('memtest', 'memtest: PASS')]:
            type_keys(list(command) + ['ret'])
            wait_for(lambda: expected in screen())
        # Editing: erase an extra character and execute the corrected command.
        type_keys(['e', 'c', 'h', 'o', 'spc', 'o', 'x', 'backspace', 'k', 'ret'])
        wait_for(lambda: 'ok' in [screen()[i:i+80].strip() for i in range(0, 2000, 80)])
        duration = int(os.environ.get('SMOKE_SECONDS', '65'))
        end = time.monotonic() + duration
        while time.monotonic() < end:
            log = (root / 'serial.log').read_text()
            assert '[EXC]' not in log, log
            assert process.poll() is None, 'Guest exited during scheduling'
            time.sleep(0.2)
        log = (root / 'serial.log').read_text()
        counts = [int(line.split('=')[1], 16) for line in log.splitlines() if line.startswith('[SCHED]')]
        assert len(counts) >= duration - 3, counts
        assert all(b > a for a, b in zip(counts, counts[1:])), counts
        type_keys(['s', 'h', 'u', 't', 'd', 'o', 'w', 'n', 'ret'])
        assert process.wait(timeout=10) == 0
        print(f'PASS: Shell help/echo/edit/mem/tasks/uptime/memtest/shutdown; {counts[-1]} switches; {duration}s stability')
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
