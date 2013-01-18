#!/usr/bin/env python

import glulxd
import sys
from Ops import *
from analyze import optimize

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

def sp_name(i):
    return ('sp_%d'%i).replace('-', 'n')

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

    func_map = [ None ] * (header.ramstart//4)
    for f in functions:
        assert func_map[f.offset()//4] is None
        func_map[f.offset()//4] = f

    for (f, instrs) in zip(functions, instructions):
        f.needs_sp = True
        # Stack optimization: (determines where stack loads/stores occur,
        # so they can be replaced with local variable references)
        f.stack_refs = optimize(instrs)

    # Try to remove stack pointer argument from functions that don't need it.
    # These are leaf functions that (after stack optimization) don't change the
    # stack, and don't call any other functions that require a stack argument:
    changed = True
    while changed:
        changed = False
        for (f, instrs) in zip(functions, instructions):
            if f.needs_sp and f.local_args() and f.stack_refs is not None:
                for instr in instrs:
                    if instr.is_call():
                        target = instr.call_target()
                        if target is None or func_map[target//4].needs_sp:
                            break
                    else:
                        (_, _, code) = opcode_map[instr.mnemonic]
                        if 'sp' in code: # FIXME: should match whole words only!
                            break
                else:
                    f.needs_sp = False
                    changed = True

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
    print 'void *init_start_thunk(void *ctx_out)'
    print '{'
    print '    void *res;'
    print '    struct Context ctx;'
    print '    *(struct Context **)ctx_out = &ctx;'
    print '    res = context_save(&ctx);'
    print '    if (res == NULL)'
    print '    {'
    print '        data_stack[0] = 0;'
    print '        func(init_start_func)(data_stack);'
    print '    }'
    print '    return res;'
    print '}'
    print ''

    for f in functions:
        print 'static uint32_t %s(uint32_t*);' % func_name(f)
        if f.type == 0xc1:  # local args
            print 'static uint32_t %s_args(%s);' % \
                    (func_name(f), ','.join( f.needs_sp*["uint32_t*"] +
                                             f.nlocal*['uint32_t'] ))
    print ''

    print 'uint32_t (* const func_map[RAMSTART/4 + 1])(uint32_t*) = {'
    line = '\t'
    for i in range(0, header.ramstart//4):
        if func_map[i] is None: line += '0, '
        else:                   line += '&' + func_name(func_map[i]) + ', '
        if len(line) > 60:
            print line
            line = '\t'
    print line + '0 };\n'
    print '#define func(addr) func_map[addr/4]\n'
    del line

    for (func, instrs) in zip(functions, instructions):

        print 'static uint32_t %s(uint32_t *sp)' % func_name(func)
        print '{'

        if func.type == 0xc0:  # stack args
            print '\tuint32_t * const bp = sp - *sp;'
            for n in range(func.nlocal):
                print '\tuint32_t loc%d = 0;' % n
            print '\t++sp;'
        elif func.type == 0xc1:  # local args
            print '\tuint32_t narg = *sp;'
            for n in range(func.nlocal):
                print '\tuint32_t loc%d = (narg > %d) ? *--sp : 0;' % (n, n)
            print '\treturn %s_args(%s);' % ( func_name(func),
                ', '.join( func.needs_sp*['sp'] +
                           ['loc%d'%n for n in range(func.nlocal)] ) )
            print '}'
            print 'static uint32_t %s_args(%s)' % ( func_name(func),
                ', '.join( func.needs_sp*['uint32_t *sp'] +
                           ['uint32_t loc%d'%n for n in range(func.nlocal)] ) )
            print '{'
            if func.needs_sp:
                print '\tuint32_t * const bp = sp;'
        else:
            assert 0

        if func.stack_refs:
            for i in func.stack_refs:
                if i < 0:
                    print '\tuint32_t %s = sp[%d];' % (sp_name(i), i)
                else:
                    print '\tuint32_t %s;' % sp_name(i)

        branch_targets = set([i.branch_target() for i in instrs])
        branch_targets.remove(None)

        for instr in instrs:
            (param, sizes, code) = opcode_map[instr.mnemonic]
            assert len(param) == len(sizes) == len(instr.operands)
            if instr.offset() in branch_targets:
                print 'a%08x: {' % instr.offset()
            else:
                print '\t{ /* %08x */' % instr.offset()

            if instr.mnemonic.startswith('callf') and \
                    instr.operands[0].is_immediate():

                # Shortcut call to known function:
                f = func_map[instr.operands[0].value()//4]
                if f.type == 0xc1:
                    args = [ 'l%d'%(n + 2) if 1 < n + 2 < len(param) else '0'
                                           for n in range(f.nlocal) ]
                    code = 's1 = %s_args(%s);' % \
                        (func_name(f), ', '.join(f.needs_sp*['sp'] + args))
                f = None  # I wish Python had lexical scoping

            if instr.mnemonic == 'call' or instr.mnemonic == 'tailcall':

                if func.stack_refs:

                    if instr.mnemonic == 'call':        res = 's1 ='
                    elif instr.mnemonic == 'tailcall':  res = 'return'
                    else:                               assert False

                    assert instr.operands[1].is_immediate()
                    n = instr.operands[1].value()
                    h = instr.sp
                    code = ''
                    for i in range(h - n, h):
                        code += 'sp[%d] = %s; ' % (i, sp_name(i))
                    code += 'sp[%d] = %d; %s func(l1)(sp + %d);' % (h,n,res,h)

                    # FIXME: should shortcut call to args() function if
                    #        operands[0].is_immediate too.


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
                        v = '%s(%d + RAMSTART)' % (getter(s), o.value())
                    elif o.is_local_ref():
                        assert o.value()%4 == 0
                        v = 'loc%d' % (o.value()//4)
                    elif o.is_stack_ref():
                        if not func.stack_refs:
                            v = '(%s)*--sp' % (t,)
                        else:
                            v = '(%s)%s' % (t, sp_name(o.value()))
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
                        print '\t\t(void)%s;'%v
                    elif o.is_mem_ref():
                        print '\t\t%s(%d, %s);' % (setter(s), o.value(), v)
                    elif o.is_ram_ref():
                        print '\t\t%s(%d + RAMSTART, %s);' % \
                            (setter(s), o.value(), v)
                    elif o.is_local_ref():
                        print '\t\tloc%d = %s;' % (o.value()//4, v)
                    elif o.is_stack_ref():
                        if not func.stack_refs:
                            print '\t\t*sp++ = %s;' % (v,)
                        else:
                            print '\t\t%s = %s;' % (sp_name(o.value()), v)
                    else:
                        assert 0
                else:
                    assert 0

            print '\t}'

        print '\treturn 0;'
        print '}\n'

if __name__ == '__main__': main(*sys.argv[1:])
