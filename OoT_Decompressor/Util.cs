using System;
using System.Collections.Generic;

namespace OoT_Decompressor
{
    public static class Util
    {
        public static Span<T> Slice<T>(this T[] array, int start)
            => array.AsSpan().Slice(start);

        public static Span<T> Slice<T>(this T[] array, int start, int length)
            => array.AsSpan().Slice(start, length);

        public static ushort ByteSwap(ushort input)
        {
            var b0 = (ushort)((ushort)(input & 0x00ff) << 8);
            var b1 = (ushort)((ushort)(input & 0xff00) >> 8);

            var result = (ushort)(b0 | b1);
            return result;
        }

        public static uint ByteSwap(uint input)
        {
            var b0 = (input & 0x000000ff) << 24;
            var b1 = (input & 0x0000ff00) << 8;
            var b2 = (input & 0x00ff0000) >> 8;
            var b3 = (input & 0xff000000) >> 24;

            var result = b0 | b1 | b2 | b3;
            return result;
        }

        public static Span<byte> ByteSwap(Span<byte> bytes, int bytesToSwap, bool alwaysSwap = true)
        {
            if (bytes.Length % bytesToSwap != 0)
                throw new ArgumentException($"Bytes span must be divisible by {bytesToSwap}!");

            for (var i = 0; i < bytes.Length; i += bytesToSwap)
            {
                var span = bytes.Slice(i, bytesToSwap);
                if (alwaysSwap || BitConverter.IsLittleEndian)
                    span.Reverse();
            }

            // This method modified the span directly, but this makes chaining calls easier.
            return bytes;
        }

        public static int BytesToInt(Span<byte> bytes)
            => BitConverter.ToInt32(ByteSwap(bytes.ToArray(), 4, false));

        public static uint BytesToUInt(Span<byte> bytes)
            => BitConverter.ToUInt32(ByteSwap(bytes.ToArray(), 4, false));

        public static Span<byte> IntToBytes(int intVal)
            => ByteSwap(BitConverter.GetBytes(intVal), 4, false);

        public static Span<byte> UIntToBytes(uint intVal)
            => ByteSwap(BitConverter.GetBytes(intVal), 4, false);
    }
}
