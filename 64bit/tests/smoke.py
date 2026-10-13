#!/usr/bin/env python3
"""Boot the real ISO; verify scheduling, PS/2 shell commands, and shutdown."""
import json
import os
from pathlib import Path
import shlex
import socket
import shutil
import subprocess
import tempfile
import time
import threading
import re

if os.environ.get('NETWORK_TEST'):
    def echo_server(tcp):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM if tcp else socket.SOCK_DGRAM) as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind(('127.0.0.1', 19001 if tcp else 19000))
            if tcp:
                server.listen(1)
                conn, _ = server.accept()
                with conn:
                    conn.sendall(conn.recv(512))
            else:
                data, address = server.recvfrom(512)
                server.sendto(data, address)
    for tcp in (False, True):
        threading.Thread(target=echo_server, args=(tcp,), daemon=True).start()

iso = Path(__file__).resolve().parents[1] / 'ivseznayfews.iso'
symbols = subprocess.check_output(['nm', '-g', str(iso.with_suffix('.elf'))], text=True)
console_address = next(int(line.split()[0], 16) for line in symbols.splitlines() if line.endswith(' kernel_console'))
with tempfile.TemporaryDirectory(prefix='os-smoke-') as directory:
    root = Path(directory)
    test_iso = iso
    payload = os.environ.get('TEST_PAYLOAD')
    if payload:
        staging = root / 'iso' / 'boot'
        (staging / 'grub').mkdir(parents=True)
        shutil.copy(iso.with_suffix('.elf'), staging / 'ivseznayfews.elf')
        args = ' text' if payload == 'text' else ''
        (staging / 'grub' / 'grub.cfg').write_text(
            f'set timeout=0\nmenuentry "Test" {{\n'
            f'multiboot /boot/ivseznayfews.elf{args}\nset gfxpayload={payload}\nboot\n}}\n')
        test_iso = root / 'test.iso'
        subprocess.run(['grub-mkrescue', '-o', str(test_iso), str(root / 'iso')],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    command = [os.environ.get('QEMU', 'qemu-system-x86_64')]
    command += shlex.split(os.environ.get('QEMU_EXTRA', ''))
    command += ['-boot', 'order=d', '-accel', 'tcg', '-m', '128M', '-cdrom', str(test_iso),
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
            result = monitor(f'pmemsave {console_address:#x} 4000 "{dump}"')
            if result:
                raise RuntimeError(result)
            return dump.read_bytes()[::2].decode('ascii', errors='replace')

        def screenshot(name):
            path = root / (name + '.ppm')
            qmp('screendump', {'filename': str(path)})
            with path.open('rb') as image:
                assert image.readline().strip() == b'P6'
                width, height = map(int, image.readline().split())
                assert image.readline().strip() == b'255'
                pixels = image.read()
            assert len(pixels) == width*height*3
            artifact_dir = os.environ.get('SMOKE_ARTIFACTS')
            if artifact_dir:
                Path(artifact_dir).mkdir(parents=True, exist_ok=True)
                shutil.copy(path, Path(artifact_dir) / path.name)
            def pixel(x,y):
                offset = (y*width+x)*3
                return tuple(pixels[offset:offset+3])
            return width, height, pixel

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
        if payload != 'text':
            log = (root / 'serial.log').read_text()
            assert '[GUI] framebuffer ready' in log, log
            assert '[GUI] PS/2 mouse ready' in log, log
            width, height, pixel = screenshot('desktop')
            assert (width, height) == ((800, 600) if payload == '800x600x32' else (1024, 768))
            assert pixel(8,120) == (0x62,0xdf,0xb6), 'Terminal selection is not rendered'
            type_keys(['f2'])
            wait_for(lambda: '[GUI] view=1' in (root / 'serial.log').read_text())
            time.sleep(0.15)
            _, _, pixel = screenshot('system')
            assert pixel(8,172) == (0x62,0xdf,0xb6), 'System selection is not rendered'
            type_keys(['f3'])
            wait_for(lambda: '[GUI] view=2' in (root / 'serial.log').read_text())
            time.sleep(0.15)
            screenshot('help')
            # Actual PS/2 relative motion and button packets select Terminal.
            monitor(f'mouse_move {40-(width-60)} {130-(height-70)}')
            time.sleep(0.2)
            monitor('mouse_button 1')
            time.sleep(0.1)
            monitor('mouse_button 0')
            wait_for(lambda: '[GUI] view=0' in (root / 'serial.log').read_text())
            time.sleep(0.15)
            _, _, pixel = screenshot('mouse-terminal')
            assert pixel(8,120) == (0x62,0xdf,0xb6), 'Mouse click did not select Terminal'
        else:
            assert '[GUI] text recovery mode' in (root / 'serial.log').read_text()
            width, height, _ = screenshot('text-recovery')
            assert width <= 720 and height <= 480, 'Expected VGA text output'
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
        if os.environ.get('SYSTEM_TEST'):
            for command, expected in [('ls', 'README.TXT'), ('cat readme.txt', 'Read through VFS'),
                                      ('run readme.txt', 'Process exit: -3'), ('run hello.elf', 'Process exit: 0'), ('usertest', 'usertest: PASS'),
                                      ('smptest', 'smptest: PASS')]:
                type_keys([('spc' if c==' ' else 'dot' if c=='.' else c) for c in command] + ['ret'])
                wait_for(lambda: expected in screen(), timeout=20)
            assert '[USER] isolated fault vector=' in (root / 'serial.log').read_text()
            if os.environ.get('EXPECT_CPUS'):
                log = (root / 'serial.log').read_text()
                assert f"[SMP] online=0x{int(os.environ['EXPECT_CPUS']):016X}" in log, log
                jobs = re.findall(r'CPU ([1-7]) completed jobs: (\d+)', screen())
                assert sum(int(count) for _, count in jobs)>0, 'No work executed on APs'
            # Test new file view, window move, minimize, and keyboard restore.
            if payload != 'text':
                type_keys(['f4'])
                wait_for(lambda: '[GUI] view=3' in (root / 'serial.log').read_text())
                time.sleep(0.15)
                screenshot('files')
                type_keys(['f1'])
                x = (168 if width>=960 else 88)+24
                monitor(f'mouse_move {x+100-40} {225-130}')
                time.sleep(0.15)
                monitor('mouse_button 1')
                time.sleep(0.15)
                monitor('mouse_move -16 -60')
                time.sleep(0.15)
                monitor('mouse_button 0')
                time.sleep(0.15)
                _, _, pixel = screenshot('dragged')
                assert pixel(x-15,151)==(0x17,0x24,0x37), 'Title bar did not move'
                monitor('mouse_button 1')
                time.sleep(0.15)
                monitor('mouse_move 16 60')
                time.sleep(0.15)
                monitor('mouse_button 0')
                time.sleep(0.15)
                monitor(f'mouse_move {width-56-(x+100)} 0')
                time.sleep(0.15)
                monitor('mouse_button 1')
                time.sleep(0.15)
                monitor('mouse_button 0')
                time.sleep(0.15)
                _, _, pixel = screenshot('minimized')
                assert pixel(x+1,211)==(0x0b,0x11,0x1b), 'Terminal did not minimize'
                type_keys(['f1'])
                time.sleep(0.15)
                _, _, pixel = screenshot('restored')
                assert pixel(x+1,211)==(0x17,0x24,0x37), 'Terminal did not restore'
        if os.environ.get('NETWORK_TEST'):
            for command, expected in [('ping 10.0.2.2', 'ping: reply'),
                                      ('udp 10.0.2.2 19000 hello', 'udp: reply hello'),
                                      ('tcp 10.0.2.2 19001 hello', 'tcp: reply hello')]:
                type_keys([('spc' if c==' ' else 'dot' if c=='.' else c) for c in command] + ['ret'])
                wait_for(lambda: expected in screen(), timeout=20)
        screenshot('commands')
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
        label = payload or 'GUI 1024x768'
        print(f'PASS: system={bool(os.environ.get("SYSTEM_TEST"))}, network={bool(os.environ.get("NETWORK_TEST"))}; {label}; Shell help/echo/edit/mem/tasks/uptime/memtest/shutdown; {counts[-1]} switches; {duration}s stability')
    finally:
        artifact_dir = os.environ.get('SMOKE_ARTIFACTS')
        if artifact_dir and (root / 'serial.log').exists():
            Path(artifact_dir).mkdir(parents=True, exist_ok=True)
            shutil.copy(root / 'serial.log', Path(artifact_dir) / 'serial.log')
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
