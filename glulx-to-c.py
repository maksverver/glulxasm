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
        if code  == '-': code  = ''
        assert mnem not in opcode_map
        opcode_map[mnem] = (param, sizes, code)

def func_name(f):
    return 'func%08x' % f.offset()

def int_type(size):
    if size == 'B': return 'uint8_t'
    if size == 'S': return 'uint16_t'
    if size == 'L': return 'uint32_t'
    if size == 'b': return 'int8_t'
    if size == 's': return 'int16_t'
    if size == 'l': return 'int32_t'
    if size == 'f': return 'uint32_t'
    assert 0

def getter(size):
    if size == 'B': return 'get_byte'
    if size == 'S': return 'get_shrt'
    if size == 'L': return 'get_long'
    if size == 'b': return '(int8_t)get_byte'
    if size == 's': return '(int16_t)get_shrt'
    if size == 'l': return '(int32_t)get_long'
    if size == 'f': return 'get_long'
    assert 0

def setter(size):
    if size in ('B', 'b'): return 'set_byte'
    if size in ('S', 's'): return 'set_shrt'
    if size in ('L', 'l'): return 'set_long'
    if size == 'f':        return 'set_long'
    assert 0

def main(path = None):
    read_opcode_map()

    if path is not None:
        data = file(path, 'rb').read()
    else:
        data = sys.stdin.read()
    data = glulxd.unwrap(data)

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

    print '#include "storycode.h"'
    print ''
    print '#define RAMSTART     ((uint32_t)%du)' % header.ramstart
    print '#define EXTSTART     ((uint32_t)%du)' % header.extstart
    print '#define ENDMEM       ((uint32_t)%du)' % header.endmem
    print '#define STACK_SIZE   ((uint32_t)%du)' % header.stack_size
    print '#define START_FUNC   ((uint32_t)%du)' % header.start_func
    print '#define DECODING_TBL ((uint32_t)%du)' % header.decoding_tbl
    print '#define CHECKSUM     ((uint32_t)%du)' % header.checksum
    print ''
    print 'const uint32_t init_ramstart     = RAMSTART;'
    print 'const uint32_t init_extstart     = EXTSTART;'
    print 'const uint32_t init_endmem       = ENDMEM;'
    print 'const uint32_t init_stack_size   = STACK_SIZE;'
    print 'const uint32_t init_start_func   = START_FUNC;'
    print 'const uint32_t init_decoding_tbl = DECODING_TBL;'
    print 'const uint32_t init_checksum     = CHECKSUM;'
    print ''

    for f in functions:
        print 'uint32_t %s(uint32_t*);' % func_name(f)
    print ''

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
            print '\tuint32_t * const bp = sp - *sp;'
            for n in range(nlocal):
                print '\tuint32_t loc%d = 0;' % n
            print '\t++sp;'
        elif f.type == 0xc1:  # local args
            print '\tuint32_t * const bp = sp;'
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
                print 'a%08x: {' % instr.offset()
            else:
                print '\t{ /* %08x */' % instr.offset()

            ids = range(1, len(param) + 1)
            num_load = num_store = 0
            for n,o,p,s in zip(ids, instr.operands, param, sizes):

                if p == 'b':  # label target
                    assert s == 'x'
                    target = instr.branch_target()
                    if target is not None:
                        print '\t\t#define b1 goto a%08x' % (target)
                    else:
                        target = instr.return_value()
                        if target is not None:
                            print '\t\t#define b1 return %d' % (target)
                        else:
                            print '\t\t#define b1 native_invalidop(%d, "%s")'%\
                                (instr.offset(), 'indirect jump target')

                elif p == 'l':  # loaded argument
                    num_load += 1
                    t = int_type(s)

                    if o.is_immediate():
                        v = str(o.value())
                        if s in 'LSBf': v += 'u'
                    elif o.is_mem_ref():
                        v = '%s(%d)' % (getter(s), o.value()&0xffffffff)
                    elif o.is_ram_ref():
                        v = '%s(%d + RAMSTART);' % (getter(s), o.value())
                    elif o.is_local_ref():
                        assert o.value()%4 == 0
                        v = 'loc%d' % (o.value()/4)
                    elif o.is_stack_ref():
                        v = '(%s)*--sp;' % (t,)
                    else:
                        assert 0

                    if s == 'f':
                        t = 'float'
                        v = 'long_to_float(%s)' % v

                    print '\t\t%s l%d = %s;' % (t, num_load, v)

                elif p == 's':  # stored argument
                    num_store += 1
                    t = int_type(s)
                    if s == 'f':
                        t = 'float'
                    if code:
                        print '\t\t%s s%d;' % (t, num_store)
                    else:
                        # initialize to zero to suppress spurious warnings
                        print '\t\t%s s%d = 0;' % (t, num_store)

                else:
                    assert 0

            if code != '':
                print '\t\t%s /* %s */' % (code, instr.mnemonic)
            else:
                print '\t\tnative_invalidop(%d, "%s");'%\
                    (instr.offset(), instr.mnemonic)

            num_load = num_store = 0
            for n,o,p,s in zip(ids, instr.operands, param, sizes):
                if p == 'l':
                    pass
                elif p == 'b':  # branch argument
                    print '\t\t#undef b1'
                elif p == 's':  # stored argument
                    num_store += 1
                    v = 's%d'%num_store
                    if s == 'f':
                        v = 'float_to_long(%s)'%v
                    if o.is_immediate():
                        assert o.value() == 0
                    elif o.is_mem_ref():
                        print '\t\t%s(%d, %s);' % (setter(s), o.value(), v)
                    elif o.is_ram_ref():
                        print '\t\t%s(%d + RAMSTART, %s);' % \
                            (setter(s), o.value(), v)
                    elif o.is_local_ref():
                        print '\t\tloc%d = %s;' % (o.value()/4, v)
                    elif o.is_stack_ref():
                        print '\t\t*sp++ = %s;' % (v,)
                    else:
                        assert 0
                else:
                    assert 0

            print '\t}'

        print '\treturn 0;'
        print '}\n'

if __name__ == '__main__': main(*sys.argv[1:])
