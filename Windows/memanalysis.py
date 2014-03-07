#!/usr/bin/env python2
# Released as open source by NCC Group Plc - http://www.nccgroup.com/
# Developed by Nikos Laleas, nikos dot laleas at nccgroup dot com
# https://github.com/nccgroup/xcavator
# Released under AGPL. See LICENSE for more information

import subprocess
import platform
import argparse
import os
import datetime
import time
import textwrap
import sys
from ctypes import *
from ctypes.wintypes import *

list = []

MEMORY_STATES = {0x1000: "MEM_COMMIT", 0x10000: "MEM_FREE", 0x2000: "MEM_RESERVE"}
MEMORY_PROTECTIONS = {0x10: "EXECUTE", 0x20: "EXECUTE_READ", 0x40: "EXECUTE_READWRITE",
                     0x80: "EXECUTE_WRITECOPY", 0x01: "NOACCESS", 0x04: "READWRITE", 0x08: "WRITECOPY", 0x02: "READONLY"}
MEMORY_TYPES = {0x1000000: "MEM_IMAGE", 0x40000: "MEM_MAPPED", 0x20000: "MEM_PRIVATE"}


class MEMORY_BASIC_INFORMATION32 (Structure):
    _fields_ = [
        ("BaseAddress", c_void_p),
        ("AllocationBase", c_void_p),
        ("AllocationProtect", DWORD),
        ("RegionSize", c_size_t),
        ("State", DWORD),
        ("Protect", DWORD),
        ("Type", DWORD)
        ]


class MEMORY_BASIC_INFORMATION64 (Structure):
    _fields_ = [
        ("BaseAddress", c_ulonglong),
        ("AllocationBase", c_ulonglong),
        ("AllocationProtect", DWORD),
        ("__alignment1", DWORD),
        ("RegionSize", c_ulonglong),
        ("State", DWORD),
        ("Protect", DWORD),
        ("Type", DWORD),
        ("__alignment2", DWORD)
        ]

class SYSTEM_INFO(Structure):

    _fields_ = [("wProcessorArchitecture", WORD),
                ("wReserved", WORD),
                ("dwPageSize", DWORD),
                ("lpMinimumApplicationAddress", LPVOID),
                ("lpMaximumApplicationAddress", LPVOID),
                ("dwActiveProcessorMask", DWORD),
                ("dwNumberOfProcessors", DWORD),
                ("dwProcessorType", DWORD),
                ("dwAllocationGranularity", DWORD),
                ("wProcessorLevel", WORD),
                ("wProcessorRevision", WORD)]


class MEMORY_BASIC_INFORMATION:

    def __init__ (self, MBI):
        self.MBI = MBI
        self.set_attributes()

    def set_attributes(self):
        self.BaseAddress = self.MBI.BaseAddress
        self.AllocationBase = self.MBI.AllocationBase
        self.AllocationProtect = MEMORY_PROTECTIONS.get(self.MBI.AllocationProtect, self.MBI.AllocationProtect)
        self.RegionSize = self.MBI.RegionSize
        self.State = MEMORY_STATES.get(self.MBI.State, self.MBI.State)
        self.Protect = MEMORY_PROTECTIONS.get(self.MBI.Protect, self.MBI.Protect)
        self.Type = MEMORY_TYPES.get(self.MBI.Type, self.MBI.Type)
        self.ProtectBits = self.MBI.Protect


def main():
    args = parse_args()
    print("+"+58*"-"+"+")
    print("memaddressanalysis")
    print("Author: Nikos Laleas")
    print("NCC Group")
    print("+"+58*"-"+"+\n\n")
    executable = args.e
    runs = args.r
    sleep = args.t
    min_address = int(args.min, 16)
    max_address = int(args.max, 16)
    filename = args.o
    permissions = args.p
    appearances = args.a
    procargs = args.args
    x = 0
    while x < runs:
        e = [executable] + procargs
        p = subprocess.Popen(e)
        hProc = int(p._handle)
        arch, process_is32 = ValidateProcess(hProc)
        if x is 0:
            printt('Starting memory analysis...\n')
            printt('Current Process is %s.\n' % arch)
            if process_is32:
                printt('Target Process is 32bit.\n')
            else:
                printt('Target Process is 64bit.\n')

        if arch == "32bit" and not process_is32:
            printt('Error: Can\'t dump a 64-bit process from a 32-bit one.\n')
            printt('Install python 64-bit.\n')
            p.kill()
            exit(1)

        printt("Run[%d]... " % (x+1))
        time.sleep(sleep)
        sys.stdout.write("Walking memory...\t")
        sys.stdout.write('{:>12}  {:>7}'.format("Remaining: %s" % (datetime.timedelta(seconds=(runs-x-1)*sleep)),
                                                "[%d%%]\n" % ((x+1)*100/runs)))
        si = SYSTEM_INFO()
        psi = byref(si)
        windll.kernel32.GetSystemInfo(psi)
        base_address = si.lpMinimumApplicationAddress
        if max_address is 0:
            if process_is32 and si.lpMaximumApplicationAddress > 0xFFFFFFFF:
                max_address = 0xFFFEFFFF
            else:
                max_address = si.lpMaximumApplicationAddress
        page_address = base_address
        while page_address < max_address:
            next_page = scan_page(hProc, page_address, process_is32, min_address, permissions)
            page_address = next_page
        p.kill()
        x += 1

    printt("Printing findings...\n\n")
    header = 60*"-"+"\n"
    header += '{:>14}  {:>8}  {:<19}  {:>10}'.format("Address", "Appear.", "Permissions", "Size")
    header += "\n"+60*"-"
    f = None
    if filename:
        try:
            f = open(filename, 'w')
            f.write('Memory analysis of: %s\n\n' % executable)
            f.write(header+'\n')
        except IOError:
            printt('Error: Can\'t write to file. Will just print the results.\n')
    print header

    for i in sorted(list, key=lambda l:l[1], reverse=True):
        if i[1] > appearances - 1:
            result = '{:>14}  {:>8}  {:<19}  {:>10}'.format("0x"+i[0], i[1], i[2], i[3])
            if f:
                f.write(result+'\n')
            print result
    if f:
        f.close()


def ValidateProcess(hProc):
    arch, type = platform.architecture()
    ProcIs32 = c_bool()
    success = windll.kernel32.IsWow64Process(hProc, byref(ProcIs32))
    if not success:
        printt('Failed to check whether target process is under WoW64.\nQuitting...\n')
        exit(1)
    return arch, ProcIs32


def scan_page(process_handle, page_address, ProcIs32, min, permissions):
    info = VirtualQueryEx(process_handle, page_address, ProcIs32)
    base_address = info.BaseAddress
    region_size = info.RegionSize
    next_region = base_address + region_size
    if info.Protect != 0 and page_address >= min:
        modifier = None
        protect = info.Protect
        if info.ProtectBits & 0x100:
            modifier = "GUARD"
            perm = info.ProtectBits ^ 0x100
            protect = MEMORY_PROTECTIONS.get(perm, perm)
        elif info.ProtectBits & 0x200:
            modifier = "NOCACHE"
            perm = info.ProtectBits ^ 0x200
            protect = MEMORY_PROTECTIONS.get(perm, perm)
        elif info.ProtectBits & 0x400:
            modifier = "WRITECOMBINE"
            perm = info.ProtectBits ^ 0x400
            protect = MEMORY_PROTECTIONS.get(perm, perm)

        if permissions is None or protect in permissions:
            add_to_array(format(page_address, 'x'), protect, convert_bytes(region_size), modifier)
    return next_region


def add_to_array(address, permission, size, modifier):
    if modifier:
        permission = "%s/%s" % (permission, modifier)
    for i, x in enumerate(list):
        if address in x:
            if list[i][2] == permission:
                list[i][1] += 1
                return
    l = [[address, 1, permission, size]]
    list.extend(l)


def VirtualQueryEx (hProcess, lpAddress, ProcIs32):
    if ProcIs32:
        lpBuffer = MEMORY_BASIC_INFORMATION32()
    else:
        lpBuffer = MEMORY_BASIC_INFORMATION64()

    success = windll.kernel32.VirtualQueryEx(hProcess, LPVOID(lpAddress), byref(lpBuffer), sizeof(lpBuffer))
    assert success,  "VirtualQueryEx Failed.\n%s" % (WinError(GetLastError())[1])
    return MEMORY_BASIC_INFORMATION(lpBuffer)


def parse_args():
    permissions = textwrap.dedent('''\
                    Available Permissions:  EXECUTE, EXECUTE_READ, EXECUTE_READWRITE,
                                            EXECUTE_WRITECOPY, NOACCESS, READWRITE,
                                            WRITECOPY, READONLY''')
    parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
                                     description=textwrap.dedent('''\
                                     Examples:  memanalysis.py -e "WINWORD.exe" -args "/a" -r 300 -t 6
                                                memanalysis.py -e "WINWORD.exe" -r 300 -p EXECUTE_READWRITE EXECUTE_READ -o log.txt
                                                memanalysis.py -e "WINWORD.exe" -r 300 -min 0x71e0000 -max 0x7fefcd81000 -p READWRITE
                                                '''),
                                     epilog=permissions)
    parser.add_argument('-e', metavar='EXECUTABLE', required=True, type=str, help='The executable to test')
    parser.add_argument('-args', metavar='ARGS', nargs='+', default=[], help='Executable\'s arguments')
    parser.add_argument('-r', metavar='RUNS', type=int, default=2, help='Number of runs (default: 2)')
    parser.add_argument('-t', metavar='SECONDS', type=int, default=5, help='The time in seconds to wait before walking memory (default: 5)')
    parser.add_argument('-o', metavar='FILENAME', type=str, help='Save the results to a file.')
    parser.add_argument('-min', type=str, metavar='ADDRESS', default="0x0", help='Print only the addresses that are higher than the specified one. (e.g. 0x000A0000)')
    parser.add_argument('-max', type=str, metavar='ADDRESS', default="0x0", help='Scan up to a max address. (e.g. 0x000B0000)')
    parser.add_argument('-p', metavar='PERMS', nargs='+', help='Print only specific permissions.')
    parser.add_argument('-a', metavar='APPEARANCES', type=int, default=2, help='Prints only the results that appear more that X times. (default: 2)')
    args = parser.parse_args()
    if not os.path.isfile(args.e):
        printt('Error: Executable not found.\n')
        exit(1)
    if args.min is not "0x0":
        try:
            int(args.min, 0)
        except ValueError:
            printt('Error: Invalid minimum address. Format must be: 0xDEADBEEF\n')
            exit(1)

    if args.max is not "0x0":
        try:
            int(args.max, 0)
        except ValueError:
            printt('Error: Invalid maximum address. Format must be: 0xDEADBEEF\n')
            exit(1)
        if args.min > args.max:
            tmp = args.min
            args.min = args.max
            args.max = tmp

    if args.p is not None:
        for x in args.p:
            if x not in MEMORY_PROTECTIONS.values():
                printt('Error: Invalid permission: %s\n\n' % x)
                print(permissions)
                exit(1)
    return args


def printt(string):
    ts = datetime.datetime.fromtimestamp(time.time()).strftime('%H:%M:%S')
    sys.stdout.write("[*][%s] %s" % (ts, string))


def convert_bytes(bytes):
    bytes = float(bytes)
    if bytes >= 1099511627776:
        terabytes = bytes / 1099511627776
        size = '%.2fTB' % terabytes
    elif bytes >= 1073741824:
        gigabytes = bytes / 1073741824
        size = '%.2fGB' % gigabytes
    elif bytes >= 1048576:
        megabytes = bytes / 1048576
        size = '%.2fMB' % megabytes
    elif bytes >= 1024:
        kilobytes = bytes / 1024
        size = '%.2fKB' % kilobytes
    else:
        size = '%.2fb' % bytes
    return size


if __name__ == '__main__':
    main()