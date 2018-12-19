using System;

namespace OoT_ROM_Tools
{
    public struct TableEntry
    {
        public int StartVirtual;
        public int EndVirtual;
        public int StartPhysical;
        public int EndPhysical;

        public int Size => EndVirtual - StartVirtual;

        public static int FindTable(Span<byte> rom)
        {
            for (var i = 0; i + 16 < rom.Length; i += 16)
            {
                // This marks the beginning of the file table.
                if (Util.BytesToInt(rom.Slice(i, 4)) != 0x7A656C64)
                    continue;
                if (Util.BytesToInt(rom.Slice(i + 4, 4)) != 0x61407372)
                    continue;
                if ((Util.BytesToInt(rom.Slice(i + 8, 4)) & 0xFF000000) != 0x64000000)
                    continue;

                // Find the first entry in file table.
                i += 16;
                int intVal;
                do
                {
                    i += 16;
                    intVal = Util.BytesToInt(rom.Slice(i, 4));
                }
                while (intVal != 0x00001060);

                return i - 16;
            }
            throw new Exception("Table was not found!");
        }

        public static TableEntry GetTableEntry(Span<byte> bytes, int i)
        {
            i = i * 16;
            return new TableEntry
            {
                StartVirtual = Util.BytesToInt(bytes.Slice(i, 4)),
                EndVirtual = Util.BytesToInt(bytes.Slice(i + 4, 4)),
                StartPhysical = Util.BytesToInt(bytes.Slice(i + 8, 4)),
                EndPhysical = Util.BytesToInt(bytes.Slice(i + 12, 4))
            };
        }

        public void SetTableEntry(Span<byte> bytes, int i)
        {
            i = i * 16;
            var intBytes = new byte[16];
            Util.IntToBytes(StartVirtual).CopyTo(intBytes.Slice(0, 4));
            Util.IntToBytes(EndVirtual).CopyTo(intBytes.Slice(4, 4));
            Util.IntToBytes(StartPhysical).CopyTo(intBytes.Slice(8, 4));
            Util.IntToBytes(EndPhysical).CopyTo(intBytes.Slice(12, 4));
            intBytes.AsSpan().CopyTo(bytes.Slice(i));
        }
    }
}
