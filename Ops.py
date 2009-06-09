import struct

from glulx import pack, hexs, signed_size, unsigned_size

# paramaters:
#   l  indicates a loaded argument
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
    (  0x48, 'aload',           'lls'),
    (  0x49, 'aloads',          'lls'),
    (  0x4A, 'aloadb',          'lls'),
    (  0x4B, 'aloadbit',        'lls'),
    (  0x4C, 'astore',          'lll'),
    (  0x4D, 'astores',         'lll'),
    (  0x4E, 'astoreb',         'lll'),
    (  0x4F, 'astorebit',       'lll'),
    (  0x50, 'stkcount',        's'),
    (  0x51, 'stkpeek',         'ls'),
    (  0x52, 'stkswap',         ''),
    (  0x53, 'stkroll',         'll'),
    (  0x54, 'stkcopy',         'l'),
    (  0x70, 'streamchar',      'l'),
    (  0x71, 'streamnum',       'l'),
    (  0x72, 'streamstr',       'l'),
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
    ( 0x141, 'setstringtbl',    'l'),
    ( 0x148, 'getiosys',        'ss'),
    ( 0x149, 'setiosys',        'll'),
    ( 0x150, 'linearsearch',    'llllllls'),
    ( 0x151, 'binarysearch',    'llllllls'),
    ( 0x152, 'linkedsearch',    'lllllls'),
    ( 0x160, 'callf',           'fs'),
    ( 0x161, 'callfi',          'fls'),
    ( 0x162, 'callfii',         'flls'),
    ( 0x163, 'callfiii',        'fllls'),
    ( 0x170, 'mzero',           'll'),
    ( 0x171, 'mcopy',           'lll'),
    ( 0x178, 'malloc',          'ls'),
    ( 0x179, 'mfree',           'l'),
    ( 0x180, 'accelfunc',       'll'),
    ( 0x181, 'accelparam',      'll') ]

opcodemap = dict([ (num, (mnem, args)) for (num, mnem, args) in opcodelist ])

class Op:
    def __init__(self):
        self.data   = ''
        self.offset = 0

    def __len__(self):
        return len(self.data)

class Label(Op):
    def __init__(self, name):
        Op.__init__(self)
        self.name = name

class Db(Op):
    def __init__(self, value):
        Op.__init__(self)
        self.value = value
        self.data  = struct.pack('!B', value&0xff)

class Dw(Op):
    def __init__(self, value):
        Op.__init__(self)
        self.value = value
        self.data  = struct.pack('!H', value&0xffff)

class Dd(Op):
    def __init__(self, value):
        Op.__init__(self)
        self.value = value
        self.data  = struct.pack('!I', value&0xffffffff)

class Instr(Op):
    def __init__(self, opcode, operands):
        Op.__init__(self)

        self.mnemonic, self.parameters = opcodemap[opcode]

        self.opcode   = opcode
        self.operands = operands

        assert opcode >= 0 and opcode <= 0xfffffff

        # construct encoded instruction:

        # first the opcode
        if self.opcode <= 0x7f:
            self.data += pack(self.opcode, 1)
        elif self.opcode <= 0x3fff:
            self.data += pack(self.opcode + 0x8000, 2)
        else:
            self.data += pack(self.opcode + 0xc00000, 4)

        # then the operand modes
        modes = [ oper.mode for oper in operands ]
        while len(modes)%2: modes.append(0)
        for i in range(0, len(modes), 2):
            self.data += pack(modes[i] + (modes[i + 1] << 4), 1)

        # the the operands themselves
        for oper in operands:
            self.data += oper.data

class Operand(Op):
    def __init__(self, mode, value):
        Op.__init__(self)
        self.mode  = mode
        self.value = value

        if self.mode in (0x1, 0x5, 0x9, 0xd): self.data = pack(value, 1)
        if self.mode in (0x2, 0x6, 0xa, 0xe): self.data = pack(value, 2)
        if self.mode in (0x3, 0x7, 0xb, 0xf): self.data = pack(value, 4)

    def is_immediate(self):
        return self.mode in (0x0, 0x1, 0x2, 0x3)

    def is_mem_ref(self):
        return self.mode in (0x5, 0x6, 0x7)

    def is_stack_ref(self):
        return self.mode == 0x8

    def is_local_ref(self):
        return self.mode in (0x9, 0xa, 0xb)

    def is_ram_ref(self):
        return self.mode in (0xd, 0xe, 0xf)

    def is_canonical(self):
        if self.mode in (0x0, 0x8):
            return True
        if self.mode in (0x1, 0x2, 0x3):
            return len(self) == signed_size(self.value)
        if self.mode in (0x5, 0x6, 0x7, 0x9, 0xa, 0xb, 0xd, 0xe, 0xf):
            return len(self) == unsigned_size(self.value)
        assert 0

class Data(Op):
    def __init__(self, value):
        Op.__init__(self)
        self.data = value

class Func(Op):
    def __init__(self, type, locals):
        self.type   = type
        self.locals = locals
        self.data   = pack(self.type, 1)
        for (size,count) in locals:
            self.data += pack(size, 1)
            self.data += pack(count, 1)
        self.data += pack(0, 2)

    def stack_args(self):
        return self.type == 0xc0

    def local_args(self):
        return self.type == 0xc1
