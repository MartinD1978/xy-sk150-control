#!/usr/bin/env python3
"""Setpoint/preset guards and non-atomic pair failures on isolated PTYs only."""
import os
import pty
import select
import struct
import subprocess
import sys
import time


def frame(data):
    crc = 0xffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xa001 if crc & 1 else 0)
    return data + struct.pack('<H', crc)


cases = {
    'current': [(1, 600)], 'voltage': [(0, 1100)],
    'pair': [(0, 1100), (1, 600)],
    'lower-current': [(1, 600), (0, 1100)],
    'ovp': [], 'ocp': [], 'limit': [], 'invalid-current': [],
    'output-guard': [], 'protection-timeout': [], 'protection-crc': [],
    'changed-guard': [(0, 1100)], 'first-echo': [(0, 1100)],
    'readback-mismatch': [(0, 1100)], 'second-timeout': [(0, 1100), (1, 600)],
}
for case, expected in cases.items():
    master, slave = pty.openpty()
    registers = {0: 1000, 1: 700 if case == 'lower-current' else 500,
                 2: 1000, 3: 500, 4: 500, 5: 2400, 0x53: 1300, 0x54: 800}
    if case == 'output-guard':
        registers[1] = 800
    process = subprocess.Popen([sys.argv[1], os.ttyname(slave), 'targets-' + case])
    writes = []
    buffer = b''
    deadline = time.monotonic() + 5
    while process.poll() is None and time.monotonic() < deadline:
        if not select.select([master], [], [], 0.02)[0]:
            continue
        buffer += os.read(master, 256)
        while len(buffer) >= 8:
            request, buffer = buffer[:8], buffer[8:]
            assert request == frame(request[:6])
            address, value = struct.unpack('>HH', request[2:6])
            if request[1] == 6:
                writes.append((address, value))
                assert writes == expected[:len(writes)], (case, writes)
                if case == 'second-timeout' and len(writes) == 2:
                    continue
                if case != 'readback-mismatch':
                    registers[address] = value
                if case == 'first-echo':
                    os.write(master, frame(request[:4] + struct.pack('>H', value + 1)))
                else:
                    os.write(master, request)
                if case == 'changed-guard':
                    registers[0x54] = 600
            else:
                assert request[1] == 3
                if address == 0x53 and case == 'protection-timeout':
                    continue
                values = [registers[address + offset] for offset in range(value)]
                response = frame(bytes([1, 3, value * 2]) + struct.pack('>' + 'H' * value, *values))
                if address == 0x53 and case == 'protection-crc':
                    response = response[:-1] + bytes([response[-1] ^ 1])
                os.write(master, response)
    assert process.wait(timeout=2) == 0, case
    assert writes == expected, (case, writes, expected)
    assert not select.select([master], [], [], 0.02)[0], case
    os.close(master)
    os.close(slave)
    print('targets-' + case + ': OK')
