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

header = Header()

labels = { }
ops = [ header ]

def ops_len():
    return ops[-1].offset() + len(ops[-1])

def add_op(op):
    op.set_offset(ops_len())
    ops.append(op)

def version(a,b,c):
    header.version = ((a&0xffff)<<16)|((b&0xff)<<8)|(c&0xff)

def stack_size(size):
    header.stack_size = size

def decoding_tbl(offset):
    header.decoding_tbl = offset

def absolute_labels():
    ls = {}
    for l, i in labels.iteritems():
        if i == len(ops):
            ls[l] = ops_len()
        else:
            ls[l] = ops[i].offset()
    return ls

def eof():
    # Resolve all references to labels:
    changed = 1
    while changed:
        print >>sys.stderr, "resolving label references..."
        changed = 0
        offset  = 0
        ls = absolute_labels()
        for op in ops:
            old_len = len(op)
            op.resolve_references(ls)
            new_len = len(op)
            op.set_offset(offset)
            changed += old_len != new_len
            offset += new_len
        print >>sys.stderr, changed, "operations changed length."

    # Compute output (cut off at extstart)
    sio = StringIO()
    for op in ops: sio.write(op.data())
    data = sio.getvalue()[:header.extstart]
    del sio

    # Fix header checksum:
    header.update_checksum(data)
    data = header.data() + data[len(header):]

    # Write out
    sys.stdout.write(data)
    sys.exit(0)

def pad(boundary):
    return Padding(boundary)

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
    assert isinstance(v, OperandBase)
    return v

def instr(opcode, *operands):
    return Instr(opcode, [ conv_imm(v) for v in operands ])

def label(id):
    labels[id] = len(ops)

def db(*xs):
    return ByteData(xs, 1)

def dw(*xs):
    return ByteData(xs, 2)

def dd(*xs):
    return ByteData(xs, 4)

def dc(s):
    return ByteData(map(ord, s + '\0'), 1)

def fill(n):
    return ByteData([0]*n, 1)

def func_stack(*args):
    return Func(0xc0, args)

def func_local(*args):
    return Func(0xc1, args)

def lb(l, n=-1):
    if n == -1: n = 4
    return ImmediateOperand(l, n, True, True)

def la(l, n=-1):
    if n == -1: n = 4
    return ImmediateOperand(l, n)

def limm(l, n=-1):
    if n == -1: n = 4
    return ImmediateOperand(l, n)

def lmem(l, n=-1):
    if n == -1: n = 4
    return MemRefOperand(l, n)

def lram(l, n=-1):
    if n == -1: n = 4
    return RamRefOperand(l, n)

while True:
    line = raw_input().strip()
    if line == '' or line[0] == '#': continue
    obj = eval(line)
    if obj is not None:
        if not isinstance(obj, list):
            obj = [obj]
        for o in obj:
            assert isinstance(o, Op)
            add_op(o)
