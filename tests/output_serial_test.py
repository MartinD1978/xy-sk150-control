#!/usr/bin/env python3
"""Output register tests using PTYs only. No physical serial devices."""
import os
import pty
import select
import struct
import subprocess
import sys
import time


def frame(b):
    value = 0xffff
    for byte in b:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xa001 if value & 1 else 0)
    return b + struct.pack('<H', value)


def exchange(master, expected, reply):
    received = b''
    deadline = time.monotonic() + 2
    while len(received) < 8 and time.monotonic() < deadline:
        if select.select([master], [], [], 0.02)[0]:
            received += os.read(master, 100)
    assert received == frame(bytes.fromhex(expected)), received.hex()
    if reply:
        data = frame(reply)
        for pos in range(0, len(data), 2):
            os.write(master, data[pos:pos+2])
            time.sleep(0.003)


for case in ['output-read', 'output-read-error', 'output-on', 'output-off',
             'output-high', 'output-high-measured', 'output-timeout',
             'output-guard', 'output-error']:
    master, slave = pty.openpty()
    process = subprocess.Popen([sys.argv[1], os.ttyname(slave), case])
    if case.startswith('output-read'):
        exchange(master, '010300120001', bytes([1, 3, 2, 0, 2 if case.endswith('error') else 1]))
    elif case in ('output-on', 'output-high', 'output-high-measured', 'output-timeout'):
        voltage = 1201 if case == 'output-high' else 1200
        measured = 1201 if case == 'output-high-measured' else 1000
        reply = bytes([1, 3, 12]) + struct.pack('>6H', voltage, 7100, measured, 1000, 1000, 2400)
        exchange(master, '010300000006', None if case == 'output-timeout' else reply)
        if case == 'output-on':
            exchange(master, '010600120001', bytes.fromhex('010600120001'))
    elif case in ('output-off', 'output-error'):
        exchange(master, '010600120000', bytes.fromhex('010600120001' if case == 'output-error' else '010600120000'))
    assert process.wait(timeout=2) == 0, case
    # High voltage / invalid values / failed preflight must send no output write.
    assert not select.select([master], [], [], 0.02)[0], case
    os.close(master)
    os.close(slave)
    print(f'{case}: OK')
