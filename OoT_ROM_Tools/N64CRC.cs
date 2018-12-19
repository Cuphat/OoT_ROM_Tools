using System;

namespace OoT_ROM_Tools
{
    public class N64CRC
    {
        private const byte N64HeaderSize = 0x40;
        private const short N64BCSize = 0x1000 - N64HeaderSize;

        private const byte N64CRC1 = 0x10;
        private const byte N64CRC2 = 0x14;

        private const int ChecksumStart = 0x00001000;
        private const int ChecksumLength = 0x00100000;
        private const uint ChecksumCIC6102 = 0xF8CA4DDC;
        private const uint ChecksumCIC6103 = 0xA3886759;
        private const uint ChecksumCIC6105 = 0xDF26F436;
        private const uint ChecksumCIC6106 = 0x1FEA617A;

        public byte[] Bytes { get; set; }

        public N64CRC() { }
        public N64CRC(byte[] bytes) => Bytes = bytes;

        public static void FixCRC(byte[] bytes)
        {
            var crcObj = new N64CRC(bytes);
            crcObj.FixCRC();
        }

        public void FixCRC()
        {
            var bytesSpan = Bytes.Slice(0, ChecksumStart + ChecksumLength);
            var crc = N64CalcCRC(bytesSpan);
            Util.UIntToBytes(crc[0]).CopyTo(bytesSpan.Slice(N64CRC1, 4));
            Util.UIntToBytes(crc[1]).CopyTo(bytesSpan.Slice(N64CRC2, 4));
        }

        private static uint ROL(uint i, int b) => (i << b) | (i >> (32 - b));

        private static uint[] _crcTable;
        private static uint[] CRCTable
        {
            get
            {
                if (_crcTable != null)
                    return _crcTable;

                _crcTable = new uint[256];
                const uint poly = 0xEDB88320;
                for (var i = 0; i < 256; i++)
                {
                    var crc = (uint)i;
                    for (var j = 8; j > 0; j--)
                    {
                        if ((crc & 1) != 0)
                            crc = (crc >> 1) ^ poly;
                        else crc >>= 1;
                    }
                    _crcTable[i] = crc;
                }

                return _crcTable;
            }
        }

        private static uint CRC32(Span<byte> data)
        {
            var crc = ~0u;
            foreach (var b in data)
            {
                crc = (crc >> 8) ^ CRCTable[(crc ^ b) & 0xFF];
            }

            return ~crc;
        }

        private static int N64GetCIC(Span<byte> bytes)
        {
            switch (CRC32(bytes.Slice(N64HeaderSize, N64BCSize)))
            {
                case 0x6170A4A1: return 6101;
                case 0x90BB6CB5: return 6102;
                case 0x0B050EE0: return 6103;
                case 0x98BC2C86: return 6105;
                case 0xACC8580A: return 6106;
                default: return 0;
            }
        }

        private static uint[] N64CalcCRC(Span<byte> bytes)
        {
            var crc = new uint[2];
            uint seed;
            var bootCode = N64GetCIC(bytes);

            switch (bootCode)
            {
                case 6101:
                case 6102:
                    seed = ChecksumCIC6102;
                    break;
                case 6103:
                    seed = ChecksumCIC6103;
                    break;
                case 6105:
                    seed = ChecksumCIC6105;
                    break;
                case 6106:
                    seed = ChecksumCIC6106;
                    break;
                default:
                    throw new Exception("BootCode not supported!");
            }

            uint t1, t2, t3, t4, t5, t6;
            t1 = t2 = t3 = t4 = t5 = t6 = seed;
            var i = ChecksumStart;
            while (i < ChecksumStart + ChecksumLength)
            {
                var d = Util.BytesToUInt(bytes.Slice(i, 4));
                if (t6 + d < t6)
                    t4++;
                t6 += d;
                t3 ^= d;
                var r = ROL(d, (int) (d & 0x1F));
                t5 += r;

                if (t2 > d)
                    t2 ^= r;
                else
                    t2 ^= t6 ^ d;

                if (bootCode == 6105)
                    t1 += Util.BytesToUInt(bytes.Slice(N64HeaderSize + 0x0710 + (i & 0xFF), 4)) ^ d;
                else
                    t1 += t5 ^ d;

                i += 4;
            }

            switch (bootCode)
            {
                case 6103:
                    crc[0] = (t6 ^ t4) + t3;
                    crc[1] = (t5 ^ t2) + t1;
                    break;
                case 6106:
                    crc[0] = (t6 * t4) + t3;
                    crc[1] = (t5 * t2) + t1;
                    break;
                default:
                    crc[0] = t6 ^ t4 ^ t3;
                    crc[1] = t5 ^ t2 ^ t1;
                    break;
            }

            return crc;
        }

    }
}
