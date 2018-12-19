using System;
using System.IO;

namespace OoT_ROM_Tools
{
    public static class Decompress
    {
        public static void DecompressOoT(string inputPath, string outputPath)
        {
            var inputRom = File.ReadAllBytes(inputPath);

            // Size check.
            if (inputRom.Length != 0x2000000)
                throw new Exception("ROM is not the correct size for a compressed OoT ROM.");

            // Swap bytes if needed.
            if (inputRom[0] == 0x37)
                Util.ByteSwap(inputRom, 2);

            var outputRom = new byte[0x4000000];
            inputRom.CopyTo(outputRom, 0);

            // Find table offsets.
            var tableStart = TableEntry.FindTable(inputRom);
            var table = TableEntry.GetTableEntry(inputRom.Slice(tableStart), 2);
            var tableCount = table.Size / 16;
            var inputTable = inputRom.Slice(tableStart, table.EndVirtual - tableStart);
            var outputTable = outputRom.Slice(tableStart, table.EndVirtual - tableStart);

            // Set everything part the table in outputRom to 0.
            Array.Clear(outputRom, table.EndVirtual, outputRom.Length - table.EndVirtual);

            for (var i = 3; i < tableCount; i++)
            {
                table = TableEntry.GetTableEntry(inputTable, i);

                // Copy if decoded, decode if encoded.
                if (table.EndPhysical == 0)
                    inputRom.Slice(table.StartPhysical, table.Size)
                        .CopyTo(outputRom.Slice(table.StartVirtual));
                else
                    Decode(inputRom.Slice(table.StartPhysical),
                        outputRom.Slice(table.StartVirtual), table.Size);

                // Clean up output's table.
                table.StartPhysical = table.StartVirtual;
                table.EndPhysical = 0;
                table.SetTableEntry(outputTable, i);
            }

            N64CRC.FixCRC(outputRom);

            // Write output file.
            File.WriteAllBytes(outputPath, outputRom);
        }

        private static void Decode(Span<byte> source, Span<byte> destination, int size)
        {
            int sourcePlace = 0x10, destinationPlace = 0, bitCount = 0;
            byte codeByte = 0;

            while (destinationPlace < size)
            {
                // If there are no more bits to test, get a new byte.
                if (bitCount == 0)
                {
                    codeByte = source[sourcePlace++];
                    bitCount = 8;
                }

                // If bit 7 is a 1, just copy 1 byte from source to destination.
                // Else do some decoding.
                if ((codeByte & 0x80) != 0)
                {
                    destination[destinationPlace++] = source[sourcePlace++];
                }
                else
                {
                    // Get 2 bytes from source.
                    var bytes = source.Slice(sourcePlace, 2);
                    sourcePlace += 2;

                    // Calculate distance to move in destination and number of bytes to copy.
                    var distance = ((bytes[0] & 0xF) << 8) | bytes[1];
                    var copyPlace = destinationPlace - (distance + 1);
                    var numBytes = bytes[0] >> 4;

                    // Do more calculations on the number of bytes to copy.
                    if (numBytes == 0)
                        numBytes = source[sourcePlace++] + 0x12;
                    else
                        numBytes += 2;

                    // Copy data from a previous point in destination to current point in destination.
                    for (var i = 0; i < numBytes; i++)
                        destination[destinationPlace++] = destination[copyPlace++];
                }

                // Set up for the next read cycle.
                codeByte = (byte)(codeByte << 1);
                bitCount--;
            }
        }
    }
}
