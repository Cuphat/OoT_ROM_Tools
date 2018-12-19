using System;
using System.Linq;

namespace OoT_ROM_Tools
{
    internal class Program
    {
        private static void Main(string[] args)
        {
            if (!args.Any())
                throw new Exception("Missing input argument.");

            var inputPath = args[0];
            var outputPath = args.Length > 1 ? args[1] : args[0].Substring(0, args[0].LastIndexOf('.')) + "-decomp.z64";

            Decompress.DecompressOoT(inputPath, outputPath);
        }
    }
}
