#!/usr/bin/env python3
"""Pseudo-terminal integration tests. Does not open any physical serial port."""
import os
import pty
import select
import struct
import subprocess
import sys
import time


def crc(b):
    value = 0xFFFF
    for byte in b:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xA001 if value & 1 else 0)
    return struct.pack('<H', value)


def frame(b):
    return b + crc(b)


for case in ['read', 'write', 'guard', 'crc', 'slave', 'function',
             'exception', 'count', 'timeout', 'truncated', 'write-error']:
    master, slave = pty.openpty()
    mode = case if case in ('read', 'write', 'guard', 'write-error') else 'error'
    process = subprocess.Popen([sys.argv[1], os.ttyname(slave), mode])
    request = b''
    deadline = time.monotonic() + 2
    while len(request) < 8 and process.poll() is None and time.monotonic() < deadline:
        if select.select([master], [], [], 0.02)[0]:
            request += os.read(master, 100)
    if case == 'guard':
        assert not request, request
    else:
        expected = frame(bytes.fromhex('0106000004b0' if mode.startswith('write') else '010300000006'))
        assert request == expected, (case, request.hex())
        payload = bytes([1, 3, 12]) + struct.pack('>6H', 1000, 7100, 999, 1234, 1234, 2400)
        response = frame(payload)
        if case == 'write':
            response = request
        elif case == 'write-error':
            response = frame(bytes.fromhex('0106000003e8'))
        elif case == 'crc':
            response = response[:-1] + bytes([response[-1] ^ 1])
        elif case == 'slave':
            response = frame(bytes([2]) + payload[1:])
        elif case == 'function':
            response = frame(bytes([1, 4]) + payload[2:])
        elif case == 'exception':
            response = frame(bytes([1, 0x83, 2]))
        elif case == 'count':
            response = frame(bytes([1, 3, 10]) + payload[3:-2])
        elif case == 'timeout':
            response = b''
        elif case == 'truncated':
            response = response[:7]
        # Fragmentation exercises partial reads.
        for pos in range(0, len(response), 2):
            os.write(master, response[pos:pos+2])
            time.sleep(0.003)
    assert process.wait(timeout=2) == 0, case
    # No retry or extra write should occur on failures.
    assert not select.select([master], [], [], 0.01)[0], case
    os.close(master)
    os.close(slave)
    print(f'{case}: OK')
