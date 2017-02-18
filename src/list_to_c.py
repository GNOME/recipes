#!/usr/bin/env python3

import os
import sys

ifile = sys.argv[1]
ofile = sys.argv[2]
prefix = sys.argv[3]

i = open(ifile, 'r', encoding='utf-8')
o = open(ofile, 'w', encoding='utf-8')

out_templ = '\tN_("{prefix}{line}"),\n'
values = {'prefix': prefix}
for line in i.readlines():
    values['line'] = line.strip()
    o.write(out_templ.format(**values))
