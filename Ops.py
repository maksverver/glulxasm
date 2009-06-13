import struct

# TODO: refactor classes here so their members are accessed via getters/setters
#       only (attribute _foo, getter foo(), setter set_foo(x)) and adjust
#       code in glulxd and glulxa accordingly.

from glulx import *

# paramaters:
#   l  indicates a loaded argument
#   m  indicates a loaded argument that is interpreted as a memory address
#   s  indicates a stored argument
#   b  indicates a branch target (target is IP - 2 + offset), or return 0/1
#   a  indicates an absolute branch target (target is offset)
#   f  indicates a function offset

opcodelist = [
    #  num    mnemonic          parameters
    (  0x00, 'nop',             ''),
    (  0x10, 'add',             'lls'),
    (  0x11, 'sub',             'lls'),
    (  0x12, 'mul',             'lls'),
    (  0x13, 'div',             'lls'),
    (  0x14, 'mod',             'lls'),
    (  0x15, 'neg',             'ls'),
    (  0x18, 'bitand',          'lls'),
    (  0x19, 'bitor',           'lls'),
    (  0x1A, 'bitxor',          'lls'),
    (  0x1B, 'bitnot',          'ls'),
    (  0x1C, 'shiftl',          'lls'),
    (  0x1D, 'sshiftr',         'lls'),
    (  0x1E, 'ushiftr',         'lls'),
    (  0x20, 'jump',            'b'),
    (  0x22, 'jz',              'lb'),
    (  0x23, 'jnz',             'lb'),
    (  0x24, 'jeq',             'llb'),
    (  0x25, 'jne',             'llb'),
    (  0x26, 'jlt',             'llb'),
    (  0x27, 'jge',             'llb'),
    (  0x28, 'jgt',             'llb'),
    (  0x29, 'jle',             'llb'),
    (  0x2A, 'jltu',            'llb'),
    (  0x2B, 'jgeu',            'llb'),
    (  0x2C, 'jgtu',            'llb'),
    (  0x2D, 'jleu',            'llb'),
    (  0x30, 'call',            'fls'),
    (  0x31, 'ret',             'l'),       # was 'return'
    (  0x32, 'catch',           'sb'),
    (  0x33, 'throw',           'll'),
    (  0x34, 'tailcall',        'fl'),
    (  0x40, 'copy',            'ls'),
    (  0x41, 'copys',           'ls'),
    (  0x42, 'copyb',           'ls'),
    (  0x44, 'sexs',            'ls'),
    (  0x45, 'sexb',            'ls'),
    (  0x48, 'aload',           'mls'),
    (  0x49, 'aloads',          'mls'),
    (  0x4A, 'aloadb',          'mls'),
    (  0x4B, 'aloadbit',        'mls'),
    (  0x4C, 'astore',          'mll'),
    (  0x4D, 'astores',         'mll'),
    (  0x4E, 'astoreb',         'mll'),
    (  0x4F, 'astorebit',       'mll'),
    (  0x50, 'stkcount',        's'),
    (  0x51, 'stkpeek',         'ls'),
    (  0x52, 'stkswap',         ''),
    (  0x53, 'stkroll',         'll'),
    (  0x54, 'stkcopy',         'l'),
    (  0x70, 'streamchar',      'l'),
    (  0x71, 'streamnum',       'l'),
    (  0x72, 'streamstr',       'm'),
    (  0x73, 'streamunichar',   'l'),
    ( 0x100, 'gestalt',         'lls'),
    ( 0x101, 'debugtrap',       'l'),
    ( 0x102, 'getmemsize',      's'),
    ( 0x103, 'setmemsize',      'ls'),
    ( 0x104, 'jumpabs',         'a'),
    ( 0x110, 'random',          'ls'),
    ( 0x111, 'setrandom',       'l'),
    ( 0x120, 'quit',            ''),
    ( 0x121, 'verify',          's'),
    ( 0x122, 'restart',         ''),
    ( 0x123, 'save',            'ls'),
    ( 0x124, 'restore',         'ls'),
    ( 0x125, 'saveundo',        's'),
    ( 0x126, 'restoreundo',     's'),
    ( 0x127, 'protect',         'll'),
    ( 0x130, 'glk',             'lls'),
    ( 0x140, 'getstringtbl',    's'),
    ( 0x141, 'setstringtbl',    'm'),
    ( 0x148, 'getiosys',        'ss'),
    ( 0x149, 'setiosys',        'll'),
    ( 0x150, 'linearsearch',    'mlmlllls'),
    ( 0x151, 'binarysearch',    'mlmlllls'),
    ( 0x152, 'linkedsearch',    'mlmllls'),
    ( 0x160, 'callf',           'fs'),
    ( 0x161, 'callfi',          'fls'),
    ( 0x162, 'callfii',         'flls'),
    ( 0x163, 'callfiii',        'fllls'),
    ( 0x170, 'mzero',           'lm'),
    ( 0x171, 'mcopy',           'lmm'),
    ( 0x178, 'malloc',          'ls'),
    ( 0x179, 'mfree',           'l'),
    ( 0x180, 'accelfunc',       'lf'),
    ( 0x181, 'accelparam',      'll') ]

opcodemap = dict([ (num, (mnem, args)) for (num, mnem, args) in opcodelist ])

class Op:
    def __init__(self, data = '', offset = 0):
        self._data   = data
        self._offset = offset

    def __len__(self):
        return len(self._data)

    def set_offset(self, offset):
        self._offset = offset

    def data(self):
        return self._data

    def offset(self):
        return self._offset

    def target(self):
        return None

    def update(self):
        pass

    def resolve_references(self, ops):
        pass


class Header(Op):

    def __init__(self):
        Op.__init__(self)
        self.magic          = MAGIC
        self.version        = 0x00030101  # 3.1.1
        self.ramstart       = 0
        self.extstart       = 0
        self.endmem         = 0
        self.stack_size     = 0
        self.start_func     = 0
        self.decoding_tbl   = 0
        self.checksum       = 0
        self.update()

    def header_checksum(self):
        return (self.magic + self.version + self.ramstart + self.extstart +
                self.endmem + self.stack_size + self.start_func +
                self.decoding_tbl)&0xffffffff

    def calculate_checksum(self, data):
        checksum = self.header_checksum()
        for i in xrange(36, self.extstart, 4):
            checksum = (checksum + struct.unpack_from('!I', data, i)[0])&0xffffffff
        return checksum

    def update_checksum(self, data):
        self.checksum = self.calculate_checksum(data)
        self.update()

    def verify_checksum(self, data):
        return self.checksum == self.calculate_checksum(data)

    def pack(self):
        return struct.pack('!IIIIIIIII',
            self.magic, self.version, self.ramstart, self.extstart, self.endmem,
            self.stack_size, self.start_func, self.decoding_tbl, self.checksum)

    def unpack(self, data):
        (self.magic, self.version, self.ramstart, self.extstart, self.endmem,
         self.stack_size, self.start_func, self.decoding_tbl, self.checksum) = \
         struct.unpack_from('!IIIIIIIII', data, 0)

    def update(self):
        self._data = self.pack()

    def resolve_references(self, labels):
        self.ramstart     = labels['ramstart']
        self.extstart     = labels['extstart']
        self.endmem       = labels['endmem']
        self.start_func   = labels['start_func']
        try:
            self.decoding_tbl = labels['decoding_tbl']
        except KeyError:
            self.decoding_tbl = 0

class Label(Op):
    def __init__(self, name):
        Op.__init__(self)
        self.name = name

class ByteData(Op):
    def __init__(self, values, size):
        data = ''
        for v in values: data += pack(v, size)
        Op.__init__(self, data)

class Instr(Op):
    def __init__(self, opcode, operands, offset = 0):
        assert opcode >= 0 and opcode <= 0xfffffff
        Op.__init__(self, '', offset)
        self.mnemonic, self.parameters = opcodemap[opcode]
        self.opcode   = opcode
        self.operands = operands
        self.update()

    def is_branch(self):
        return self.parameters.find('b') >= 0 or self.parameters.find('a') >= 0

    def is_call(self):
        return self.parameters.find('f') >= 0

    def return_value(self):
        # NOTE: currently only works for branches
        i = self.parameters.find('b')
        if i >= 0:
            oper = self.operands[i]
            if oper.is_immediate() and oper.value() in (0, 1):
                return oper.value()

    def branch_target(self):

        # Recognize (relative) branch targets
        i = self.parameters.find('b')
        if i >= 0:
            oper = self.operands[i]
            if oper.is_immediate() and oper.value() not in (0, 1):
                return (self.offset() + len(self) + oper.value() - 2)&0xffffffff

        # Recognize (absolute) branch targets
        i = self.parameters.find('a')
        if i >= 0:
            oper = self.operands[i]
            if oper.is_immediate():
                target = oper.value()
                return target

    def call_target(self):

        # Recognize call targets
        i = self.parameters.find('f')
        if i >= 0:
            oper = self.operands[i]
            if oper.is_immediate():
                return oper.value()

    def target(self):
        return self.branch_target() or self.call_target()

    def update(self):
        # construct encoded instruction:
        self._data = ''

        # first the opcode
        if self.opcode <= 0x7f:
            self._data += pack(self.opcode, 1)
        elif self.opcode <= 0x3fff:
            self._data += pack(self.opcode + 0x8000, 2)
        else:
            self._data += pack(self.opcode + 0xc00000, 4)

        # then the operand modes
        modes = [ oper.mode() for oper in self.operands ]
        while len(modes)%2: modes.append(0)
        for i in range(0, len(modes), 2):
            self._data += pack(modes[i] + (modes[i + 1] << 4), 1)

        # then the operands themselves
        for oper in self.operands:
            self._data += oper.data()

    def resolve_references(self, labels):
        for oper in self.operands:
            oper.set_instr(self)
            oper.resolve_references(labels)
        self.update()

class OperandBase(Op):
    def __init__(self):
        self._instr = None

    def set_instr(self, instr):
        self._instr = instr

    def instr(self):
        return sel.f_instr

class Operand(OperandBase):
    def __init__(self, mode, value):
        OperandBase.__init__(self)
        self._mode  = mode
        self._value = value
        self.update()

    def mode(self):
        return self._mode

    def value(self):
        return self._value

    def is_immediate(self):
        return self._mode in (0x0, 0x1, 0x2, 0x3)

    def is_mem_ref(self):
        return self._mode in (0x5, 0x6, 0x7)

    def is_stack_ref(self):
        return self._mode == 0x8

    def is_local_ref(self):
        return self._mode in (0x9, 0xa, 0xb)

    def is_ram_ref(self):
        return self._mode in (0xd, 0xe, 0xf)

    def is_canonical(self):
        if self._mode in (0x0, 0x8):
            return True
        if self._mode in (0x1, 0x2, 0x3):
            return len(self) == signed_size(self._value)
        if self._mode in (0x5, 0x6, 0x7, 0x9, 0xa, 0xb, 0xd, 0xe, 0xf):
            return len(self) == unsigned_size(self._value)
        assert 0

    def update(self):
        if self._mode in (0x0, 0x8): self._data = ''
        elif self._mode in (0x1, 0x5, 0x9, 0xd): self._data = pack(self._value, 1)
        elif self._mode in (0x2, 0x6, 0xa, 0xe): self._data = pack(self._value, 2)
        elif self._mode in (0x3, 0x7, 0xb, 0xf): self._data = pack(self._value, 4)
        else: assert 0


class ImmediateOperand(OperandBase):
    'Behaves as an Operand but refers to a label instead of a fixed value'

    def __init__(self, label, size, signed = False, relative = False):
        OperandBase.__init__(self)
        self._label    = label
        self._size     = size
        self._signed   = signed
        self._relative = relative
        self._target   = 0
        self.update()

    def is_immediate(self): return True
    def is_mem_ref(self):   return False
    def is_stack_ref(self): return False
    def is_local_ref(self): return False
    def is_ram_ref(self):   return False

    def update(self):
        if self._signed:
            self._data = packs(self._target, self._size)
        else:
            self._data = pack(self._target, self._size)

    def mode(self):
        if self._size == 1: return 0x1
        if self._size == 2: return 0x2
        if self._size == 4: return 0x3
        assert 0

    def resolve_references(self, labels):
        target = labels[self._label]
        if self._relative:
            target = target - self._instr.offset() - len(self._instr) + 2
        self._target = target
        self.update()


class MemRefOperand(OperandBase):

    def __init__(self, label, size):
        OperandBase.__init__(self)
        self._label    = label
        self._size     = size
        self._target   = 0
        self.update()

    def is_immediate(self): return False
    def is_mem_ref(self):   return True
    def is_stack_ref(self): return False
    def is_local_ref(self): return False
    def is_ram_ref(self):   return False

    def update(self):
        self._data = pack(self._target, self._size)

    def mode(self):
        if self._size == 1: return 0x5
        if self._size == 2: return 0x6
        if self._size == 4: return 0x7
        assert 0

    def resolve_references(self, labels):
        self._target = labels[self._label]
        self.update()


class RamRefOperand(OperandBase):

    def __init__(self, label, size):
        OperandBase.__init__(self)
        self._label    = label
        self._size     = size
        self._target   = 0
        self.update()

    def is_immediate(self): return False
    def is_mem_ref(self):   return False
    def is_stack_ref(self): return False
    def is_local_ref(self): return False
    def is_ram_ref(self):   return True

    def update(self):
        self._data = pack(self._target, self._size)

    def mode(self):
        if self._size == 1: return 0xd
        if self._size == 2: return 0xe
        if self._size == 4: return 0xf
        assert 0

    def resolve_references(self, labels):
        self._target = labels[self._label] - labels['ramstart']
        self.update()

class Func(Op):
    def __init__(self, type, locals, offset = 0):
        self.type   = type
        self.locals = locals
        data = pack(self.type, 1)
        for (size,count) in locals:
            data += pack(size, 1)
            data += pack(count, 1)
        data += pack(0, 2)
        Op.__init__(self, data, offset)

    def stack_args(self):
        return self.type == 0xc0

    def local_args(self):
        return self.type == 0xc1

class Padding(Op):
    def __init__(self, boundary):
        Op.__init__(self)
        self._boundary = boundary
        self.update()

    def update(self):
        b = self._boundary
        self._data = '\0'*((b - self._offset%b)%b)

    def resolve_references(self, labels):
        self.update()
