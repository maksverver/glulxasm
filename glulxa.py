#!/usr/bin/env python

from cStringIO import StringIO
import glulx
import sys
import struct
from Ops import *

def make_mnemonic_func(opcode, nparam):
    if   nparam == 0:  return lambda: instr(opcode)
    elif nparam == 1:  return lambda a: instr(opcode, a)
    elif nparam == 2:  return lambda a,b: instr(opcode, a,b)
    elif nparam == 3:  return lambda a,b,c: instr(opcode, a,b,c)
    elif nparam == 4:  return lambda a,b,c,d: instr(opcode, a,b,c,d)
    elif nparam == 5:  return lambda a,b,c,d,e: instr(opcode, a,b,c,d,e)
    elif nparam == 6:  return lambda a,b,c,d,e,f: instr(opcode, a,b,c,d,e,f)
    elif nparam == 7:  return lambda a,b,c,d,e,f,g: instr(opcode, a,b,c,d,e,f,g)
    elif nparam == 8:  return lambda a,b,c,d,e,f,g,h: instr(opcode, a,b,c,d,e,f,g,h)
    assert 0

for opcode, mnemomic, parameters in opcodelist:
    assert getattr(__import__(__name__), mnemomic, None) is None
    f = make_mnemonic_func(opcode, len(parameters))
    setattr(__import__(__name__), mnemomic, f)

header = glulx.Header()

labels = { }

def version(a,b,c):
    header.version = ((a&0xffff)<<16)|((b&0xff)<<8)|(c&0xff)

def stack_size(size):
    header.stack_size = size

def decoding_tbl(offset):
    header.decoding_tbl = offset

def eof():
    header.ramstart     = labels['ramstart']
    header.extstart     = labels['extstart']
    header.endmem       = output_len
    header.start_func   = labels['start_func']

    # Chop off everything after extstart
    data = output.getvalue()[:header.extstart]

    # Fix header checksum:
    header.update_checksum(data)
    data = header.pack() + data[header.size():]

    # Write out
    sys.stdout.write(data)
    sys.exit(0)

def pad(boundary):
    # HACK! need to do real padding later.
    return fill(((boundary - output_len)%boundary)%boundary)

def op(mode, value):
    return Operand(mode, value)

def imm(v, n = -1):
    if n == -1:
        if v == 0: n = 0
        else:      n = signed_size(v)
    if n == 0: return Operand(0x0, v)
    if n == 1: return Operand(0x1, v)
    if n == 2: return Operand(0x2, v)
    if n == 4: return Operand(0x3, v)
    assert 0

def mem(v, n = -1):
    if n == -1: n = unsigned_size(v)
    if n == 1: return Operand(0x5, v&0xffffffff)
    if n == 2: return Operand(0x6, v&0xffffffff)
    if n == 4: return Operand(0x7, v&0xffffffff)
    assert 0

def ram(v, n = -1):
    if n == -1: n = unsigned_size(v)
    if n == 1: return Operand(0xd, v&0xffffffff)
    if n == 2: return Operand(0xe, v&0xffffffff)
    if n == 4: return Operand(0xf, v&0xffffffff)
    assert 0

def loc(v, n = -1):
    if n == -1: n = unsigned_size(v)
    if n == 1: return Operand(0x9, v&0xffffffff)
    if n == 2: return Operand(0xa, v&0xffffffff)
    if n == 4: return Operand(0xb, v&0xffffffff)
    assert 0

def stk():
    return Operand(8, 0)

def conv_imm(v):
    if isinstance(v, int): return imm(v)
    assert isinstance(v, Operand)
    return v

def instr(opcode, *operands):
    return Instr(opcode, [ conv_imm(v) for v in operands ])

def label(id):
    labels[id] = output_len

def db(*xs):
    return [ Db(x) for x in xs ]

def dw(x):
    return [ Dw(x) for x in xs ]

def dd(x):
    return [ Dd(x) for x in xs ]

def dc(s):
    return Data(s + '\0')

def fill(n):
    return Data('\0'*n)

def func_stack(*args):
    return Func(0xc0, args)

def func_local(*args):
    return Func(0xc1, args)

output     = StringIO()
output.write(header.pack())
output_len = header.size()
while True:
    line = raw_input().strip()
    if line == '' or line[0] == '#': continue
    obj = eval(line)
    if obj is not None:
        if not isinstance(obj, list):
            obj = [obj]
        for o in obj:
            output.write(o.data)
            output_len += len(o.data)
