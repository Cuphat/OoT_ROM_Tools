using System;
using System.Linq;

namespace OoT_ROM_Tools
{
    internal class Program
    {
        private enum Mode
        {
            Decompress = 1,
            Compress
        }

        private static void Main(string[] args)
        {
            if (!args.Any())
            {
                Console.WriteLine("Usage: OoT_ROM_Tools <mode> <input> <output>");
                Console.WriteLine("Modes supported: decompress");
                return;
            }

            Mode mode;
            Span<string> inputArgs;
            switch (args[0])
            {
                case "decompress":
                    inputArgs = args.Slice(1);
                    mode = Mode.Decompress;
                    break;
                case "compress":
                    inputArgs = args.Slice(1);
                    mode = Mode.Compress;
                    break;
                default:
                    inputArgs = args.AsSpan();
                    // TODO: Make mode optional and guess based on input arguments.
                    throw new Exception("Invalid mode specified.");
            }

            if (inputArgs.IsEmpty)
                throw new Exception("No input argument specified!");

            switch (mode)
            {
                case Mode.Decompress:
                    var inputPath = inputArgs[0];
                    var outputPath = inputArgs.Length > 1
                        ? inputArgs[1]
                        : inputArgs[0].Substring(0, inputArgs[0].LastIndexOf('.')) + "-decomp.z64";

                    Decompress.DecompressOoT(inputPath, outputPath);
                    return;
                default:
                    throw new Exception("Mode currently not supported.");
            }
        }
    }
}
