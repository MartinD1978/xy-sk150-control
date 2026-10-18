#!/usr/bin/env python3
"""OVP/OCP register and readback tests. Only pseudo terminals are opened."""
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
        data = frame(reply)
        for pos in range(0, len(data), 2):
            os.write(master, data[pos:pos+2])
            time.sleep(0.003)


def values(ovp, ocp):
    return bytes([1, 3, 4]) + struct.pack('>HH', ovp, ocp)


cases = ['protection-read', 'protection-invalid', 'ovp-write', 'ocp-write',
         'ovp-mismatch', 'ovp-timeout', 'ovp-preflight', 'ovp-guard', 'ocp-guard']
cases += [f'trip-{code}' for code in range(12)] + ['trip-99', 'trip-65535', 'trip-timeout', 'trip-crc']
for case in cases:
    master, slave = pty.openpty()
    process = subprocess.Popen([sys.argv[1], os.ttyname(slave), case])
    if case.startswith('trip-'):
        if case == 'trip-timeout':
            exchange(master, '010300100001', None)
        elif case == 'trip-crc':
            # Read the request without replying, then send a damaged response.
            exchange(master, '010300100001', None)
            response = frame(bytes([1, 3, 2, 0, 1]))
            os.write(master, response[:-1] + bytes([response[-1] ^ 1]))
        else:
            exchange(master, '010300100001', bytes([1, 3, 2]) + struct.pack('>H', int(case[5:])))
    elif case == 'protection-read':
        exchange(master, '010300530002', values(3800, 7200))
    elif case == 'protection-invalid':
        exchange(master, '010300530002', values(3801, 7200))
    elif case not in ('ovp-guard', 'ocp-guard'):
        exchange(master, '010300530002', None if case == 'ovp-preflight' else values(3800, 7200))
        if case != 'ovp-preflight':
            request = '0106005402ee' if case == 'ocp-write' else '0106005304e2'
            exchange(master, request, None if case == 'ovp-timeout' else bytes.fromhex(request))
            if case != 'ovp-timeout':
                result = values(3800, 750) if case == 'ocp-write' else values(1250, 7200)
                if case == 'ovp-mismatch':
                    result = values(1300, 7200)
                exchange(master, '010300530002', result)
    assert process.wait(timeout=2) == 0, case
    # One write per explicit request; no retries, voltage or output writes.
    assert not select.select([master], [], [], 0.02)[0], case
    os.close(master)
    os.close(slave)
    print(f'{case}: OK')
