import ctypes
import socket
import struct

PATH_LENGTH = 64


def ctypes_repr(self):
    return self.__class__.__name__ + '(' + ', '.join('%s=%s' % (f[0], getattr(self, f[0])) for f in self._fields_) + ')'


class sockaddr_in(ctypes.Structure):
    __repr__ = ctypes_repr
    _fields_ = (
        ('sin_family', ctypes.c_ushort),
        ('sin_port', ctypes.c_uint16),
        ('sin_addr', ctypes.c_uint8 * 4),
        ('sin_zero', ctypes.c_uint8 * (16 - 4 - 2- 2)), # padding
    )
    def __str__(self):
        return '%s:%d' % (socket.inet_ntop(self.sin_family, ''.join(map(chr, self.sin_addr))), socket.ntohs(self.sin_port))


class sockaddr_in6(ctypes.Structure):
    __repr__ = ctypes_repr
    _fields_ = (
        ('sin6_family', ctypes.c_ushort),
        ('sin6_port', ctypes.c_uint16),
        ('sin6_flowinfo', ctypes.c_uint32),
        ('sin6_addr', ctypes.c_uint8 * 16),
        ('sin6_scope_id', ctypes.c_uint32)
    )
    def __str__(self):
        return '[%s]:%d' % (socket.inet_ntop(self.sin6_family, ''.join(map(chr, self.sin6_addr))), socket.ntohs(self.sin6_port))


class StructReader:
    def __init__(self, buffer, byte_order):
        self.b_o = byte_order
        self.b = buffer
        self.i = 0

    def read(self, format_char, peek=False):
        format_length = struct.calcsize(self.b_o + format_char)
        val = struct.unpack(self.b_o + format_char, self.b[self.i:self.i+format_length])[0]
        if not peek:
            self.i += format_length
        return val

    def next(self, amount):
        val = self.b[self.i:self.i+amount]
        self.i += amount
        return val

    def __len__(self):
        return len(self.b[self.i:])


if __name__ == "__main__":
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 55555))

    while True:
        buf = StructReader(s.recv(4096), '<')

        while buf:
            path = {'time_elapsed': buf.read('Q')}

            path['icid'] = buf.next(buf.read('B')).hex()
            path['dcid'] = buf.next(buf.read('B')).hex()
            path['scid'] = buf.next(buf.read('B')).hex()

            local_addr_len = buf.read('I')
            if local_addr_len:
                family = buf.read('H', peek=True)
                path['local_addr'] = sockaddr_in() if family == 2 else sockaddr_in6()
                ctypes.memmove(ctypes.byref(path['local_addr']), buf.next(local_addr_len), local_addr_len)

            peer_addr_len = buf.read('I')
            if peer_addr_len:
                family = buf.read('H', peek=True)
                path['peer_addr'] = sockaddr_in() if family == 2 else sockaddr_in6()
                ctypes.memmove(ctypes.byref(path['peer_addr']), buf.next(peer_addr_len), peer_addr_len)

            path['data_sent'] = buf.read('Q')
            path['data_recv'] = buf.read('Q')
            path['data_lost'] = buf.read('Q')
            path['data_ooo'] = buf.read('Q')
            path['data_dupl'] = buf.read('Q')
            path['pkt_sent'] = buf.read('Q')
            path['pkt_recv'] = buf.read('Q')
            path['pkt_lost'] = buf.read('Q')
            path['pkt_ooo'] = buf.read('Q')
            path['pkt_dupl'] = buf.read('Q')
            path['frt_fired'] = buf.read('Q')
            path['ert_fired'] = buf.read('Q')
            path['rto_fired'] = buf.read('Q')
            path['tlp_fired'] = buf.read('Q')
            path['mean_rtt'] = buf.read('Q')
            path['rtt_variance'] = buf.read('Q')

            print(path)
