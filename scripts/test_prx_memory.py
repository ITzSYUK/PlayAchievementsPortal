import struct
import unittest
from check_prx_memory import check_memory


def fixture(*sizes):
    data = bytearray(52 + 32*len(sizes))
    data[:6] = b'\x7fELF\x01\x01'
    struct.pack_into('<I', data, 28, 52)
    struct.pack_into('<HH', data, 42, 32, len(sizes))
    for i, size in enumerate(sizes):
        struct.pack_into('<8I', data, 52+32*i, 1, len(data), i*0x100000, 0, 0, size, 5, 16)
    return data


class MemoryTest(unittest.TestCase):
    def test_reported_psp_failure(self):
        data = fixture(344332)
        with self.assertRaises(AssertionError):
            check_memory(data, 344064)
        self.assertEqual(check_memory(data, 348160), 344332)

    def test_total_and_boundary(self):
        self.assertEqual(check_memory(fixture(200000, 148160), 348160), 348160)
        with self.assertRaises(AssertionError):
            check_memory(fixture(200000, 148161), 348160)

    def test_malformed(self):
        for data in (fixture(), fixture(0), fixture(100)[:-1]):
            with self.assertRaises(AssertionError):
                check_memory(data, 348160)
        data = fixture(100)
        struct.pack_into('<I', data, 52+8, 0xfffffffe)
        with self.assertRaises(AssertionError):
            check_memory(data, 348160)


if __name__ == '__main__':
    unittest.main()
