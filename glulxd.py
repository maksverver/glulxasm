#!/usr/bin/env python

from glulx import *
from Ops import *
import sys, struct

if len(sys.argv) != 2:
    print 'Usage: glulxda <file.ulx>'
    sys.exit(0)

def decode_function_header(data, offset):
    '''Tries to decode a function header in data starting from offset. Returns
       a tuple of function type and a list of localtype/localcount values
       or None.'''
    if offset >= 0 and offset < len(data):
        type = ord(data[offset])
        if type == 0xc0 or type == 0xc1:
            offset += 1
            locals = []
            while offset + 2 <= len(data):
                localtype  = unpack(data, offset, 1)
                localcount = unpack(data, offset + 1, 1)
                offset += 2
                if localtype == 0 and localcount == 0:  # end of header
                    return Func(type, locals)
                if localtype not in (1,2,4) or localcount == 0:  # invalid
                    return None
                locals.append((localtype, localcount))
    return None

def function_start(data, offset):
    '''If data contains a valid function header at the given offset, returns
       the start offset of its instructions.'''
    func = decode_function_header(data, offset)
    if func == None: return None
    return offset + len(func)

def decode_operands(data, offset, cnt):
    res = []
    start = offset
    offset += (cnt + 1)/2
    for i in range(cnt):
        mode = (unpack(data, start + i/2, 1) >> 4*(i%2))&0x0f

        if mode == 0x0 or mode == 0x8:
            res.append(Operand(mode, 0))
        elif mode == 0x1:
            res.append(Operand(mode, unpacks(data, offset, 1)))
            offset += 1
        elif mode == 0x2:
            res.append(Operand(mode, unpacks(data, offset, 2)))
            offset += 2
        elif mode == 0x3:
            res.append(Operand(mode, unpacks(data, offset, 4)))
            offset += 4
        elif mode == 0x5 or mode == 0x9 or mode == 0xd:
            res.append(Operand(mode, unpack(data, offset, 1)))
            offset += 1
        elif mode == 0x6 or mode == 0xa or mode == 0xe:
            res.append(Operand(mode, unpack(data, offset, 2)))
            offset += 2
        elif mode == 0x7 or mode == 0xb or mode == 0xf:
            res.append(Operand(mode, unpack(data, offset, 4)))
            offset += 4
        else:
            return None  # invalid mode (0x4 or 0xC)

    return res

def decode_instruction(data, offset):
    'Try to decode bytes in data starting from offset. Returns an Instr or None'
    opcode = unpack(data, offset, 1)

    if opcode&0x80 == 0:
        opcode_len = 1
    elif opcode&0xc0 == 0x80:
        opcode = unpack(data, offset, 2) - 0x8000
        opcode_len = 2
    elif opcode&0xc0 == 0xc0:
        opcode = unpack(data, offset, 4) - 0xc00000
        opcode_len = 4
    else:
        return None

    try:
        mnem, param = opcodemap[opcode]
        operands    = decode_operands(data, offset + opcode_len, len(param))
        if operands is not None:
            return Instr(opcode, operands)
    except KeyError:
        return None

def decode_instructions(data, start, instrs):
    # Decode instructions
    todo     = [ start ]
    seen     = { start: None }
    outcalls = set()
    while todo:
        offset   = todo.pop()
        instr    = decode_instruction(data, offset)
        branches = []

        if instr is None:
            print >>sys.stderr, 'Warning: failed to decode instruction ' + \
                                'at offset 0x%08x!' % (offset)
            continue

        instrs[offset] = instr

        # Recognize (relative) branch targets
        i = instr.parameters.find('b')
        if i >= 0:
            oper = instr.operands[i]
            if oper.is_immediate() and oper.value not in (0, 1):
                target = (offset + len(instr) + oper.value - 2)&0xffffffff
                branches.append(target)

        # Recognize (absolute) branch targets
        i = instr.parameters.find('a')
        if i >= 0:
            oper = instr.operands[i]
            if oper.is_immediate():
                target = oper.value
                branches.append(target)

        # Recognize call targets
        i = instr.parameters.find('f')
        if i >= 0:
            oper = instr.operands[i]
            if oper.is_immediate():
                target = function_start(data, oper.value)
                if target is None:
                    print >>sys.stderr, 'Warning: invalid call target ' + \
                        '%d at offset %d!' % (target, offset)
                else:
                    outcalls.add(target)

        # Recognize instruction which end a sequence of instructions:
        if instr.mnemonic not in ( 'tailcall', 'ret', 'throw', 'jump',
                                    'absjump', 'quit', 'restart'):
            # Queue next instruction
            offset += len(instr)
            branches.append(offset)

        # Add branches to new locations to the todo list
        for target in branches:
            if target >= len(data):
                print >>sys.stderr, 'Warning: invalid absolute ' + \
                    'branch target %d at offset %d!' % (target, offset)
            elif target not in seen and instrs[target] is None:
                seen[target] = True
                todo.append(target)

    return outcalls

def decode_function(data, start, ops):
    "Try to decode a function at `start' and return the number of bytes decoded."

    # See if we have a valid function header
    func = decode_function_header(data, start)
    if func is None: return None
    offset = start + len(func)

    # See if we can decode the first instruction
    instr = decode_instruction(data, offset)
    if not instr: return None

    # Reject functions that start with nop to reduce false positives
    if instr.mnemonic == 'nop': return None

    # All seems OK, start decoding
    ops[start] = func
    decode_instructions(data, offset, ops)
    while ops[offset] is not None:
        offset += len(ops[offset])
    return offset - start

# Read data
data = file(sys.argv[1], 'rb').read()
assert len(data) >= 256
assert len(data)%256 == 0

# Parse header
header = Header()
header.unpack(data)

# Verify checksum
assert header.magic == MAGIC
assert header.verify_checksum(data)

# Set header parameters
print 'version(%d,%d,%d)' % (header.version >> 16, (header.version>>8)&0xff, header.version&0xff)
print 'stack_size(0x%08x)' % header.stack_size
print 'decoding_tbl(0x%08x)' % header.decoding_tbl

# Try to decode data
ops = [None]*len(data)
offset = header.size()
skipped = []
while offset < header.extstart:
    decoded = None

    if unpack(data, offset, 1) == 0:
        if skipped:
            skipped.append(0)
        offset += 1
        continue

    if decoded is None:
        decoded = decode_function(data, offset, ops)
        if decoded is not None:
            #print >>sys.stderr, 'Function of size %d at offset 0x%x' % (decoded, offset)
            pass

    if decoded is None:
        if not skipped:
            skip_offset = offset
        skipped.append(unpack(data, offset, 1))
        offset += 1
        continue

    if skipped:
        descr = ' '.join([ '%02x'%i for i in skipped[:10]])
        if len(skipped) > 10: descr += '..'
        print >>sys.stderr, 'Warning: skipped %d bytes (%s) at offset 0x%x' % \
            (len(skipped), descr, skip_offset)
        skipped = []

    offset += decoded


# Print memory (ROM/RAM) contents (not including the header)
print 'label("romstart")'
offset = header.size()
boundaries = (header.extstart, header.ramstart, header.start_func)
while offset < header.extstart:
    if offset == header.ramstart:
        print 'pad(256)'
        print ''
        print 'label("ramstart")'

    if offset == header.start_func:
        print ''
        print 'label("start_func")'

    op = ops[offset]
    if op is None:
        # Print a chunk of at most 16 bytes, not crossing any section boundaries
        n = 1
        while ( offset + n not in boundaries and
                n < 16 and ops[offset + n] is None ):
            n += 1
        values = [ unpack(data, i, 1) for i in range(offset, offset + n) ]
        print '\tdb(%s)  # %08x' % (','.join(['%3d'%v for v in values]), offset)
        offset += n
    else:
        # Print decoded operations:
        if isinstance(op, Instr):
            args = []
            for o in op.operands:
                if o.is_immediate():
                    if o.is_canonical():
                        args.append('%d'%o.value)
                    else:
                        args.append('imm(%d,%d)'%(o.value, len(o)))
                elif o.is_mem_ref():
                    if o.is_canonical():
                        args.append('mem(%d)'%o.value)
                    else:
                        args.append('mem(%d,%d)'%(o.value, len(o)))
                elif o.is_ram_ref():
                    if o.is_canonical():
                        args.append('ram(%d)'%o.value)
                    else:
                        args.append('ram(%d,%d)'%(o.value, len(o)))
                elif o.is_local_ref():
                    if o.is_canonical():
                        args.append('loc(%d)'%o.value)
                    else:
                        args.append('loc(%d,%d)'%(o.value, len(o)))
                elif o.is_stack_ref():
                    args.append('stk()')
                else:
                    print o.mode, o.value
                    assert 0
            print '\t%s(%s)  # %08x' % (op.mnemonic, ', '.join(args), offset)

        elif isinstance(op, Func):
            print ''
            print '# Function at offset %08x (code starts at %08x)' % \
                  (offset, offset + len(op))
            # TODO: print memory map of locals here?
            if op.stack_args():     f = 'func_stack'
            elif op.local_args():   f = 'func_local'
            else:                   assert 0
            pieces = map(lambda x: '(%d,%d)' % x, op.locals)
            print '\t%s(%s)'%(f,', '.join(pieces))

        offset += len(op)


print 'pad(256)'
print ''
print 'label("extstart")'
print 'fill(%d)' % (header.endmem - header.extstart)
print 'pad(256)'
print ''
print 'eof()'
