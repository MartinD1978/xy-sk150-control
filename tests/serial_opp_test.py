#!/usr/bin/env python3
"""Serial configuration, non-default slave and OPP tests on PTYs only."""
import os
import pty
import select
import struct
import subprocess
import sys
import termios
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


def opp(value):
    return bytes([1, 3, 2]) + struct.pack('>H', value)


cases = ['serial-read', 'serial-write', 'serial-wrong-slave', 'opp-read', 'opp-write',
         'opp-invalid', 'opp-guard', 'opp-mismatch', 'opp-timeout', 'opp-preflight']
for case in cases:
    master, slave = pty.openpty()
    process = subprocess.Popen([sys.argv[1], os.ttyname(slave), case])
    if case.startswith('serial-'):
        if case == 'serial-write':
            exchange(master, '1106000004b0', bytes.fromhex('1106000004b0'))
        else:
            address = 1 if case == 'serial-wrong-slave' else 17
            exchange(master, '110300000006', bytes([address, 3, 12]) + struct.pack('>6H', 1000, 7100, 999, 1234, 1234, 2400))
        attributes = termios.tcgetattr(slave)
        assert attributes[4] == termios.B9600 and attributes[5] == termios.B9600
        assert attributes[2] & termios.CSTOPB and attributes[2] & termios.CSIZE == termios.CS8
        assert not attributes[2] & termios.PARENB
    elif case == 'opp-read':
        exchange(master, '010300550001', opp(1250))
    elif case == 'opp-invalid':
        exchange(master, '010300550001', opp(1501))
    elif case != 'opp-guard':
        exchange(master, '010300550001', None if case == 'opp-preflight' else opp(1250))
        if case != 'opp-preflight':
            exchange(master, '0106005503e8', None if case == 'opp-timeout' else bytes.fromhex('0106005503e8'))
            if case != 'opp-timeout':
                exchange(master, '010300550001', opp(900 if case == 'opp-mismatch' else 1000))
    assert process.wait(timeout=2) == 0, case
    assert not select.select([master], [], [], 0.02)[0], case
    os.close(master)
    os.close(slave)
    print(f'{case}: OK')
