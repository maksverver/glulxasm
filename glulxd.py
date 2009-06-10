#!/usr/bin/env python

from glulx import *
from Ops import *
import sys, struct

if len(sys.argv) != 2:
    print 'Usage: glulxda <file.ulx>'
    sys.exit(0)

def is_ascii(v):
    return v >= 32 and v <= 126

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
            return Instr(opcode, operands, offset)
    except KeyError:
        return None

def decode_instructions(data, start, instrs):

    # Decode instructions
    todo     = [ start ]
    seen     = { start: None }
    while todo:
        offset   = todo.pop()
        instr    = decode_instruction(data, offset)
        branches = []

        if instr is None:
            print >>sys.stderr, 'Warning: failed to decode instruction ' + \
                                'at offset 0x%08x!' % (offset)
            continue

        instrs[offset] = instr

        target = instr.branch_target()
        if target is not None:
            if target >= len(data):
                print >>sys.stderr, 'Warning: invalid absolute ' + \
                    'branch target %d at offset %d!' % (target, offset)
            else:
                branches.append(target)

        # Recognize instruction which end a sequence of instructions:
        # NOTE: simplified this to 'ret' since all functions seem to end with
        #  this' even though more branching instructions exist. Old list was:
        # ( 'tailcall', 'ret', 'throw', 'jump', 'absjump', 'quit', 'restart' )
        if instr.mnemonic != 'ret':
            # Queue next instruction
            branches.append(offset + len(instr))

        for target in branches:
            if target not in seen and instrs[target] is None:
                seen[target] = True
                todo.append(target)

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


def get_bindata(data, offset, ops, boundaries, labels, max_len = 16):
    "Get a chunk of at most max_len bytes, not crossing any section boundaries"
    end = offset + 1
    while ( end not in boundaries and end not in labels and
            ops[end] is None and end - offset < max_len ):
        end += 1
    return end


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

# Scan for label positions and labels
can_label = set()
has_label = set()
offset = header.size()
while offset < header.extstart:
    can_label.add(offset)
    op = ops[offset]
    if op is None:
        offset += 1
    else:
        target = op.target()
        if target is not None:
            has_label.add(target)
        if hasattr(op, 'operands') and hasattr(op, 'parameters'):
            for (t,o) in zip(op.parameters, op.operands):
                if o.is_mem_ref():
                    has_label.add(o.value)
                if o.is_ram_ref():
                    has_label.add(header.ramstart + o.value)
                if o.is_immediate() and t == 'm':
                    has_label.add(o.value)
        offset += len(op)

# Find valid label positions and names:
labels = {}
for a in sorted(has_label.intersection(can_label)):
    labels[a] = 'l%d' % (len(labels) + 1)
del can_label
del has_label

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

    if offset in labels:
        print 'label("%s")  # %08x' % (labels[offset], offset)

    op = ops[offset]
    if op is None:
        # Print a chunk of at most 16 bytes, not crossing any section boundaries
        end = get_bindata(data, offset, ops, boundaries, labels)
        values = [ unpack(data, i, 1) for i in range(offset, end) ]
        descr = ''
        for v in values:
            if is_ascii(v): descr += chr(v)
            else:           descr += '.'
        print '\tdb(%s)  # %s  %08x' % ( ','.join(['%3d'%v for v in values]),
                                         descr, offset)
        offset = end
    else:
        # Print decoded operations:
        if isinstance(op, Instr):
            args = []
            for i in range(len(op.operands)):
                o = op.operands[i]
                t = op.parameters[i]
                v = o.value

                if o.is_immediate():

                    # HACK: convert relative branch target to absolute address
                    w = v + op.offset + len(op) - 2

                    if t == 'b' and w in labels:
                        if o.is_canonical:
                            args.append('lb("%s")'%labels[w])
                        else:
                            args.append('lb("%s", %d)'%(labels[w], len(o)))
                    elif t in ('a', 'f') and v in labels:
                        if o.is_canonical:
                            args.append('la("%s")'%labels[v])
                        else:
                            args.append('la("%s", %d)'%(labels[v], len(o)))
                    elif t == 'm' and v in labels:
                        if o.is_canonical:
                            args.append('limm("%s")'%labels[v])
                        else:
                            args.append('limm("%s", %d)'%(labels[v], len(o)))
                    elif o.is_canonical():
                        args.append('%d'%v)
                    else:
                        args.append('imm(%d,%d)'%(v, len(o)))

                elif o.is_mem_ref():

                    if v in labels:
                        if o.is_canonical:
                            args.append('lmem("%s")'%labels[v])
                        else:
                            args.append('lmem("%s", %d)'%(labels[v], len(o)))
                    elif o.is_canonical():
                        args.append('mem(%d)'%o.value)
                    else:
                        args.append('mem(%d,%d)'%(o.value, len(o)))

                elif o.is_ram_ref():

                    # convert ram-relative address to absolute address
                    w = v + header.ramstart

                    if w in labels:
                        if o.is_canonical:
                            args.append('lram("%s")'%labels[w])
                        else:
                            args.append('lram("%s", %d)'%(labels[w], len(o)))
                    elif o.is_canonical():
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
