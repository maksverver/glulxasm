#!/usr/bin/env python

import glulxd
import sys
from Ops import *

# Maps mnemonics to 3-tuple of parameters, sizes and code.
opcode_map = {}

def read_opcode_map():
    global opcode_map

    for line in file('opcode-map.txt'):
        line = line.strip()
        if not line or line.startswith('#'): continue
        mnem, param, sizes, code = line.split(None,3)
        if param == '-': param = ''
        if sizes == '-': sizes = ''
        assert mnem not in opcode_map
        opcode_map[mnem] = (param, sizes, code)

def func_name(f):
    return 'func%08x' % f.offset()

def uint_type(size):
    if int(size) == 1: return 'uint8_t'
    if int(size) == 2: return 'uint16_t'
    if int(size) == 4: return 'uint32_t'
    assert 0

def hton_type(size):
    if int(size) == 1: return ''
    if int(size) == 2: return 'htons'
    if int(size) == 4: return 'htonl'
    assert 0

def ntoh_type(size):
    if int(size) == 1: return ''
    if int(size) == 2: return 'ntohs'
    if int(size) == 4: return 'ntohl'
    assert 0

def main(path = None):
    read_opcode_map()

    if path is not None:
        data = file(path, 'rb').read()
    else:
        data = sys.stdin.read()

    ops = glulxd.disassemble(data)
    header = ops[0]
    functions    = []
    instructions = []
    for o in ops:
        if isinstance(o, Func):
            functions.append(o)
            instructions.append([])
        if isinstance(o, Instr):
            instructions[-1].append(o)

    func_map = [ None ] * (header.ramstart/4)
    for f in functions:
        assert func_map[f.offset()/4] is None
        func_map[f.offset()/4] = f

    print '#include "storyfile.h"'
    print ''
    print '#define RAMSTART     ((uint32_t)%d)' % header.ramstart
    print '#define EXTSTART     ((uint32_t)%d)' % header.extstart
    print '#define ENDMEM       ((uint32_t)%d)' % header.endmem
    print '#define STACK_SIZE   ((uint32_t)%d)' % header.stack_size
    print '#define START_FUNC   ((uint32_t)%d)' % header.start_func
    print '#define DECODING_TBL ((uint32_t)%d)' % header.decoding_tbl
    print '#define CHECKSUM     ((uint32_t)%d)' % header.checksum
    print ''
    print 'const uint32_t init_ramstart     = RAMSTART;'
    print 'const uint32_t init_extstart     = EXTSTART;'
    print 'const uint32_t init_endmem       = ENDMEM;'
    print 'const uint32_t init_stack_size   = RAMSTART;'
    print 'const uint32_t init_start_func   = START_FUNC;'
    print 'const uint32_t init_decoding_tbl = DECODING_TBL;'
    print 'const uint32_t init_checksum     = CHECKSUM;'
    print ''

    for f in functions:
        print 'uint32_t %s(uint32_t*);' % func_name(f)
    print ''

    print 'uint8_t mem[ENDMEM];'
    print 'uint32_t stack[STACK_SIZE/sizeof(uint32_t)];'
    print 'uint32_t (* const func_map[RAMSTART/4 + 1])(uint32_t*) = {'
    line = '\t'
    for i in range(0, header.ramstart/4):
        if func_map[i] is None: line += '0, '
        else:                   line += '&' + func_name(func_map[i]) + ', '
        if len(line) > 60:
            print line
            line = '\t'
    print line + '0 };\n'
    print '#define func(addr) func_map[addr/4]\n'
    del line

    for (f, instrs) in zip(functions, instructions):
        print 'uint32_t %s(uint32_t *sp)' % func_name(f)
        print '{'

        nlocal = sum([ count for size,count in f.locals ])
        if f.type == 0xc0:  # stack args
            for n in range(nlocal):
                print '\tuint32_t loc%d = 0;' % n
            print '\t++sp;'
        elif f.type == 0xc1:  # local args
            if nlocal > 0:
                print '\tuint32_t narg = *sp;'
                for n in range(nlocal):
                    print '\tuint32_t loc%d = (narg > %d) ? *--sp : 0;' % (n, n)
        else:
            assert 0

        branch_targets = set([i.branch_target() for i in instrs])
        branch_targets.remove(None)

        for instr in instrs:
            (param, sizes, code) = opcode_map[instr.mnemonic]
            assert len(param) == len(sizes) == len(instr.operands)
            if instr.offset() in branch_targets:
                print 'l%08x: {' % instr.offset()
            else:
                print '\t{ /* %08x */' % instr.offset()

            ids = range(1, len(param) + 1)
            for n,o,p,s in zip(ids, instr.operands, param, sizes):

                if p == 'b':  # label target
                    assert s == '-'
                    print '\t\t#define a%d l%08x' % (n, instr.branch_target())

                elif p == 'l':  # loaded argument
                    t = uint_type(s)
                    htonx = hton_type(s)

                    if o.is_immediate():
                        print '\t\t%s a%d = %d;' % (t, n, o.value())
                    elif o.is_mem_ref():
                        print '\t\t%s a%d = %s(*(%s*)&mem[%d]);' % \
                            (t, n, ntoh_type(s), t, o.value())
                    elif o.is_ram_ref():
                        print '\t\t%s a%d = %s(*(%s*)&mem[%d + RAMSTART]);' % \
                            (t, n, ntoh_type(s), t, o.value())
                    elif o.is_local_ref():
                        assert o.value()%4 == 0
                        print '\t\t%s a%d = local%d' % \
                            (t, n, t, o.value()/4)
                    elif o.is_stack_ref():
                        print '\t\t%s a%d = (%s)*--sp' % (t, n, t)
                    else:
                        assert 0

                elif p == 's':  # stored argument
                    print '\t\t%s a%d;' % (uint_type(s), n)

                else:
                    assert 0

            print '\t\t'+code

            for n,o,p,s in zip(ids, instr.operands, param, sizes):
                if p == 'l':
                    pass
                elif p == 'b':  # branch argument
                    print '\t\t#undef a%d'%(n,)
                elif p == 's':  # stored argument
                    if o.is_immediate():
                        assert o.value() == 0
                    elif o.is_mem_ref():
                        print '\t\t*(%s*)&mem[%d] = %s(a%d);' % \
                            (t, o.value(), hton_type(s), n)
                    elif o.is_ram_ref():
                        print '\t\t*(%s*)&mem[%d + RAMSTART] = %s(a%d);' % \
                            (t, o.value(), hton_type(s), n)
                    elif o.is_local_ref():
                        print '\t\tlocal%d = a%d;' % (o.value()/4, n)
                    elif o.is_stack_ref():
                        print '\t\t*sp++ = a%d;' % (n,)
                    else:
                        assert 0
                else:
                    assert 0

            print '\t}'

        print '\treturn 0;'
        print '}\n'

if __name__ == '__main__': main(*sys.argv[1:])
