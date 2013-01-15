#!/usr/bin/env python

from glulx import *
from Ops import *
import sys, struct

def is_ascii(v):
    return v >= 32 and v <= 126

def decode_function_header(data, offset):
    '''Tries to decode a function header in data starting from offset. Returns
       a tuple of function type and a list of localtype/localcount values
       or None.'''
    start = offset
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
                    return Func(type, locals, start)
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
                print >>sys.stderr, 'Warning: invalid branch target %d ' + \
                    'at offset %d!' % (target, offset)
            else:
                branches.append(target)

        # Recognize instruction which end a sequence of instructions:
        if instr.mnemonic not in ('tailcall', 'ret', 'throw', 'jump',
                                  'absjump', 'quit', 'restart'):
            # Queue next instruction:
            branches.append(offset + len(instr))
        else:
            # Some smarter test to see if we should queue the next instruction
            # even though it seems this branch has ended:
            new_offset = offset + len(instr)
            next_instr = decode_instruction(data, new_offset)
            next_func  = decode_function_header(data, new_offset)
            if next_func is None and next_instr is not None \
                and next_instr.mnemonic != 'nop':
                # Assume next is a valid instruction too:
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
    if instr is None: return None

    # Reject functions that start with nop to reduce false positives
    if instr.mnemonic == 'nop': return None

    # All seems OK, start decoding
    ops[start] = func
    decode_instructions(data, offset, ops)
    while ops[offset] is not None:
        offset += len(ops[offset])
    return offset - start

def disassemble(data):
    assert len(data) >= 256
    assert len(data)%256 == 0

    ops = [None]*len(data)

    # Parse header
    header = Header()
    header.unpack(data)
    ops[0] = header

    # Verify checksum
    assert header.magic == MAGIC
    assert header.verify_checksum(data)

    # Try to decode data (in ROM only!)
    offset = len(header)
    skipped = []
    while offset < header.ramstart:

        if offset == header.decoding_tbl:
            # Skip string decoding table entirely:
            offset += unpack(data, offset, 4)
            continue

        if ops[offset] is None:
            decode_function(data, offset, ops)

        if ops[offset] is None:
            skipped.append(unpack(data, offset, 1))
            offset += 1
        else:
            if skipped:
                # Warn if we may have skipped important data:
                if skipped.count(0) < len(skipped) and \
                   offset - len(skipped) != len(header):
                    descr = ' '.join([ '%02x'%i for i in skipped[:10]])
                    if len(skipped) > 10: descr += '..'
                    print >>sys.stderr, ('Warning: skipped %d bytes (%s) at ' + 
                        'offset 0x%08x') % (len(skipped), descr, offset - len(skipped))
                skipped = []
            offset += len(ops[offset])

    return ops


def get_bindata(data, offset, ops, labels, max_len = 16):
    "Get a chunk of at most max_len bytes, not crossing any section boundaries"
    end = offset + 1
    while (end not in labels and ops[end] is None and end < offset + max_len):
        end += 1
    return end

if __name__ == '__main__':
    # Read data
    if len(sys.argv) > 1:
        data = file(sys.argv[1], 'rb').read()
    else:
        data = sys.stdin.read()
    data = unwrap(data)

    ops = disassemble(data)
    header = ops[0]
    assert header is not None

    # Scan for label positions and labels
    can_label = set()
    has_label = set()
    offset = len(header)
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
                        has_label.add(o.value())
                    if o.is_ram_ref():
                        has_label.add(header.ramstart + o.value())
                    if o.is_immediate() and t == 'm':
                        has_label.add(o.value())
            offset += len(op)

    # Find valid label positions and names:
    labels = {}
    for a in sorted(has_label.intersection(can_label)):
        labels[a] = [ 'l%d' % (len(labels) + 1) ]
    del can_label
    del has_label

    # Add fixed section markers:
    sections = [
        (len(header),           'romstart'),
        (header.start_func,     'start_func'),
        (header.decoding_tbl,   'decoding_tbl'),
        (header.ramstart,       'ramstart'),
        (header.extstart,       'extstart') ]

    for offset, label in sections:
        if offset not in labels:
            labels[offset] = []
        labels[offset].append(label)

    # Print header parameters
    print 'version(%d,%d,%d)' % (
        header.version >> 16, (header.version>>8)&0xff, header.version&0xff )
    print 'stack_size(0x%08x)' % header.stack_size

    # Print memory (ROM/RAM) contents (not including the header)
    offset = len(header)
    while True:

        if offset in labels:
            for label in labels[offset]:
                print 'label("%s")  # %08x' % (label, offset)

        if offset == header.extstart:
            break

        op = ops[offset]
        if op is None:
            # Print a chunk of at most 16 bytes, not crossing any section boundaries
            end = get_bindata(data, offset, ops, labels)
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
                    v = o.value()

                    if o.is_immediate():

                        # HACK: convert relative branch target to absolute address
                        w = v + op.offset() + len(op) - 2

                        if t == 'b' and w in labels:
                            if o.is_canonical and False: # TEMP
                                args.append('lb("%s")'%labels[w][0])
                            else:
                                args.append('lb("%s", %d)'%(labels[w][0], len(o)))
                        elif t in ('a', 'f') and v in labels:
                            if o.is_canonical and False: # TEMP
                                args.append('la("%s")'%labels[v][0])
                            else:
                                args.append('la("%s", %d)'%(labels[v][0], len(o)))
                        elif t == 'm' and v in labels:
                            if o.is_canonical and False: # Temp
                                args.append('limm("%s")'%labels[v][0])
                            else:
                                args.append('limm("%s", %d)'%(labels[v][0], len(o)))
                        elif o.is_canonical():
                            args.append('%d'%v)
                        else:
                            args.append('imm(%d,%d)'%(v, len(o)))

                    elif o.is_mem_ref():

                        if v in labels:
                            if o.is_canonical and False: # TEMP
                                args.append('lmem("%s")'%labels[v][0])
                            else:
                                args.append('lmem("%s", %d)'%(labels[v][0], len(o)))
                        elif o.is_canonical():
                            args.append('mem(%d)'%v)
                        else:
                            args.append('mem(%d,%d)'%(v, len(o)))

                    elif o.is_ram_ref():

                        # convert ram-relative address to absolute address
                        w = v + header.ramstart

                        if w in labels:
                            if o.is_canonical and False: # TEMP
                                args.append('lram("%s")'%labels[w][0])
                            else:
                                args.append('lram("%s", %d)'%(labels[w][0], len(o)))
                        elif o.is_canonical():
                            args.append('ram(%d)'%v)
                        else:
                            args.append('ram(%d,%d)'%(v, len(o)))

                    elif o.is_local_ref():
                        if o.is_canonical():
                            args.append('loc(%d)'%v)
                        else:
                            args.append('loc(%d,%d)'%(v, len(o)))
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

        if offset == header.ramstart or offset == header.extstart:
            print 'pad(256)'

    print 'fill(%d)' % (header.endmem - header.extstart)
    print 'pad(256)'
    print 'label("endmem")'
    print 'eof()'
