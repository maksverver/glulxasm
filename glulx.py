import struct

MAGIC = 0x476C756C

class Header:

    def __init__(self):
        self.magic          = MAGIC
        self.version        = 0x00030101  # 3.1.1
        self.ramstart       = 0
        self.extstart       = 0
        self.endmem         = 0
        self.stack_size     = 0
        self.start_func     = 0
        self.decoding_tbl   = 0
        self.checksum       = 0

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

    def size(self):
        return 36

def pack(val, size):
    if size == 1: return struct.pack('!B', val&0xff)
    if size == 2: return struct.pack('!H', val&0xffff)
    if size == 4: return struct.pack('!I', val&0xffffffff)
    assert 0

def unpack(data, offset, size):
    if size == 1: return struct.unpack_from('!B', data, offset)[0]
    if size == 2: return struct.unpack_from('!H', data, offset)[0]
    if size == 4: return struct.unpack_from('!I', data, offset)[0]

def unpacks(data, offset, size):
    if size == 1: return struct.unpack_from('!b', data, offset)[0]
    if size == 2: return struct.unpack_from('!h', data, offset)[0]
    if size == 4: return struct.unpack_from('!i', data, offset)[0]

def hexs(data):
    res = ''
    for c in data: res += '%02x' % (ord(c)&0xff)
    return res

def signed_size(i):
    if i >= -0x80 and i <= 0x7f: return 1
    if i >= -0x8000 and i <= 0x7fff: return 2
    if i >= -0x80000000 and i <= 0x7fffffff: return 4
    assert 0

def unsigned_size(i):
    i &= 0xffffffff
    if i <= 0xff: return 1
    if i <= 0xffff: return 2
    if i <= 0xffffffff: return 4
    assert 0
