#!/usr/bin/env python3
"""Run GUI tests against an isolated Modbus emulator, never physical devices.
Usage: DISPLAY=:97 python3 tests/gui_serial_test.py ./gui-test /path/to/screenshots
"""
import os
import pty
import select
import struct
import subprocess
import sys
import tempfile
import time
import termios
from pathlib import Path


def frame(data):
    crc = 0xffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xa001 if crc & 1 else 0)
    return data + struct.pack('<H', crc)


shots = Path(sys.argv[2]).resolve()
shots.mkdir(parents=True, exist_ok=True)
master, slave = pty.openpty()
with tempfile.TemporaryDirectory(prefix='sk150-gui-') as directory:
    config = Path(directory) / 'settings.conf'
    process = subprocess.Popen([sys.argv[1], os.ttyname(slave), str(config), str(shots)])
    buffer = b''
    reads = 0
    writes = []
    registers = {0: 1200, 1: 500, 2: 1084, 3: 500, 4: 542, 5: 2453,
                 0x10: 0, 0x11: 1, 0x12: 1, 0x53: 1250, 0x54: 750, 0x55: 1250}
    deadline = time.monotonic() + 25
    try:
        while process.poll() is None and time.monotonic() < deadline:
            for request in shots.glob('*.request'):
                coordinates = request.read_text().split()
                if len(coordinates) != 4:
                    continue
                x, y, width, height = map(int, coordinates)
                filename = str(request)[:-len('.request')]
                subprocess.run(['import', '-display', os.environ['DISPLAY'], '-window', 'root',
                                '-crop', f'{width}x{height}+{x}+{y}', filename], check=True, timeout=3)
                request.unlink()
            if not select.select([master], [], [], 0.02)[0]:
                continue
            buffer += os.read(master, 256)
            while len(buffer) >= 8:
                request, buffer = buffer[:8], buffer[8:]
                assert request == frame(request[:6]), 'Request CRC invalid'
                if request[1] == 6:
                    address, value = struct.unpack('>HH', request[2:6])
                    writes.append((address, value))
                    expected = [(1, 0), (0, 900), (1, 600), (0, 1050), (0x12, 0), (0x12, 1)]
                    assert writes == expected[:len(writes)], writes
                    registers[address] = value
                    os.write(master, request)
                    continue
                assert request[0:2] == bytes([17, 3]), request.hex()
                address, count = struct.unpack('>HH', request[2:6])
                values = [registers[address + offset] for offset in range(count)]
                os.write(master, frame(bytes([17, 3, count * 2]) + struct.pack('>' + 'H' * count, *values)))
                reads += 1
        assert process.wait(timeout=2) == 0, 'GUI test failed'
        assert reads >= 6 and writes == [(1, 0), (0, 900), (1, 600), (0, 1050), (0x12, 0), (0x12, 1)], (reads, writes)
        attributes = termios.tcgetattr(slave)
        assert attributes[4] == termios.B19200 and attributes[5] == termios.B19200
        assert attributes[2] & termios.CSTOPB
        assert os.ttyname(slave) in config.read_text(), 'Selected port not saved'
        print(f'GUI emulator: {reads} reads, {len(writes)} intentional writes; isolated port preferences: OK')
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        os.close(master)
        os.close(slave)
