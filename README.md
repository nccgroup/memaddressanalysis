Mem Address Analyzer
======================
Research project related to memory address space analysis

Released as open source by NCC Group Plc - http://www.nccgroup.com/

Developed for Linux by Aidan Marlin, aidan dot marlin at nccgroup dot com

Developed for Windows by Nikos Laleas, nikos dor laleas at nccgroup dot com

Mentor Ollie Whitehouse, ollie dot whitehouse at nccgroup dot com

https://github.com/nccgroup/memaddressanalysis

Released under AGPL see LICENSE for more information

Purpose
-------------
Purpose of this research project was to produce tooling that was capable of:

* repeatedly running a program of choice (x) times where x is configurable
* then for each run
** wait (n) seconds - where n is configurable (to allow for program start-up, library loading etc.)
** record the addresses and sizes of the mapped memory pages noting which are writeable and writeable/executable
* at the conclusion of (x) executions scan through the results and see if any addresses are consistently writeable or writeable/executable at the same address (even if the sizes differ)

With the goal dynamically discovering any ASLR bypasses for the target program.

Caveats
-------------
The reason executable only pages were not included is due to how ASLR works on Microsoft Windows with regards to DLLs only being rebased once per reboot. This could be worked around as a logical extension.