#!/usr/bin/env python3
"""Configurable voltage limits and CV/CC against a PTY; never physical hardware."""
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


def exchange(master, request, reply):
    expected = frame(bytes.fromhex(request))
    received = b''
    deadline = time.monotonic() + 2
    while len(received) < len(expected) and time.monotonic() < deadline:
        if select.select([master], [], [], 0.02)[0]:
            received += os.read(master, 100)
    assert received == expected, (received.hex(), expected.hex())
    if reply is not None:
        response = frame(reply)
        for pos in range(0, len(response), 2):
            os.write(master, response[pos:pos+2])
            time.sleep(0.003)


for case in ['mode-cv', 'mode-cc', 'mode-invalid', 'mode-timeout',
             'write-24', 'write-unlimited', 'guard-24', 'guard-device',
             'output-24', 'output-24-high', 'output-unlimited', 'output-device-high']:
    master, slave = pty.openpty()
    process = subprocess.Popen([sys.argv[1], os.ttyname(slave), case])
    if case.startswith('mode-'):
        value = {'mode-cv': 0, 'mode-cc': 1, 'mode-invalid': 2, 'mode-timeout': 0}[case]
        exchange(master, '010300110001', None if case == 'mode-timeout' else bytes([1, 3, 2, 0, value]))
    elif case.startswith('write-'):
        request = '010600000960' if case == 'write-24' else '010600000e10'
        exchange(master, request, bytes.fromhex(request))
    elif case.startswith('output-'):
        voltage = {'output-24': 2400, 'output-24-high': 2401,
                   'output-unlimited': 3600, 'output-device-high': 3601}[case]
        exchange(master, '010300000006', bytes([1, 3, 12]) + struct.pack('>6H', voltage, 7100, voltage, 1000, 1000, 2400))
        if case in ('output-24', 'output-unlimited'):
            exchange(master, '010600120001', bytes.fromhex('010600120001'))
    # Guard cases must exit without a write; failure cases must never retry.
    assert process.wait(timeout=2) == 0, case
    assert not select.select([master], [], [], 0.02)[0], case
    os.close(master)
    os.close(slave)
    print(f'{case}: OK')
