import struct

MAGIC = 0x476C756C

def pack(val, size):
    if size == 1: return struct.pack('!B', val&0xff)
    if size == 2: return struct.pack('!H', val&0xffff)
    if size == 4: return struct.pack('!I', val&0xffffffff)
    assert 0

packs = pack

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
