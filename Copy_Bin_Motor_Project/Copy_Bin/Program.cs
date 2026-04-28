using System;
using System.IO;
using System.Text;
using Copy_Bin;

namespace Copy_Bin
{
    /*
     * Motor pipeline Copy_Bin — encrypts a Motor APP .bin into .mot.
     *
     * Invocation (from ALM_Motor_App/MDK-ARM/After_Build.bat):
     *     Copy_Bin.exe ALM_Motor_App
     *   reads ..\Output\ALM_Motor_App.bin
     *   writes ..\HEX\ALM_Motor_App_<snTag>_yyMMdd_HHmm.mot
     *
     * The KEY+IV here MUST stay byte-identical to:
     *   ALM_Motor_Bootloader/BSP/aes.c
     *   ALM_Motor_App/BSP/aes.c
     * Drift breaks the .mot upgrade pipeline.
     *
     * No Bootloader+App merge step (Motor's bootloader is flashed separately
     * via SWD; OTA only carries the encrypted App).
     */
    class Program
    {
        // AES-256 key (32B) — randomly generated for the Motor pipeline
        static readonly byte[] AES_KEY = new byte[]
        {
            0xD1, 0x16, 0x0B, 0xF7, 0xB6, 0xBA, 0xA6, 0x3D,
            0x8B, 0x96, 0x5F, 0x45, 0x62, 0x24, 0xEC, 0xBA,
            0x1C, 0x80, 0x82, 0x00, 0x94, 0xA9, 0x66, 0x49,
            0x07, 0x71, 0x52, 0xBF, 0x33, 0xDC, 0xE2, 0x2E
        };

        // CBC IV (16B) — randomly generated for the Motor pipeline
        static readonly byte[] AES_IV = new byte[]
        {
            0xCD, 0x11, 0xC5, 0xE4, 0xF8, 0xFB, 0xA2, 0xDC,
            0x39, 0x83, 0xD8, 0x4C, 0x0F, 0x78, 0xAC, 0xE4
        };

        const uint FW_ID_MAGIC       = 0x47544657u;   // "GTFW"
        const uint FW_SN_WILDCARD    = 0xA5C3F09Eu;
        const int  FW_ID_HEADER_SIZE = 16;            // magic(4)+board(4)+sn(4)+crc32(4)
        const string USERSTR         = "LEDFW012";    // 8B fixed metadata tag

        static void Main(string[] args)
        {
            if (args.Length == 0)
            {
                Console.WriteLine("Usage: Copy_Bin.exe <axf_basename> (e.g. ALM_Motor_App)");
                return;
            }

            string sourceDir = @"..\Output";
            string targetDir = @"..\HEX";

            string sourceFile = Path.Combine(sourceDir, args[0] + ".bin");
            if (!File.Exists(sourceFile))
            {
                Console.WriteLine($"[Copy_Bin] ERROR: {sourceFile} not found");
                return;
            }
            Directory.CreateDirectory(targetDir);

            string snTag  = ExtractFwSnTag(sourceFile);
            string stamp  = DateTime.Now.ToString("yyMMdd_HHmm");
            string motOut = Path.Combine(targetDir, $"{args[0]}_{snTag}_{stamp}.mot");

            EncryptFile(sourceFile, motOut);
            Console.WriteLine($"[Copy_Bin] {sourceFile} encrypted to {motOut}");
        }

        // Scan .bin for FW_ID magic; return tag for filename:
        //   wildcard SN -> "Unlock"; bound -> "XXXXXXXX"; no FW_ID block -> "NoFwID"
        static string ExtractFwSnTag(string binPath)
        {
            byte[] bytes = File.ReadAllBytes(binPath);
            int lastMatch = -1;
            for (int i = 0; i <= bytes.Length - 12; i += 4)
            {
                if (BitConverter.ToUInt32(bytes, i) == FW_ID_MAGIC) lastMatch = i;
            }
            if (lastMatch < 0) return "NoFwID";

            uint sn = BitConverter.ToUInt32(bytes, lastMatch + 8);
            return sn == FW_SN_WILDCARD ? "Unlock" : sn.ToString("X8");
        }

        // CRC32 / ISO-HDLC (poly 0xEDB88320, init 0xFFFFFFFF, final ^0xFFFFFFFF)
        static readonly uint[] Crc32Table = BuildCrc32Table();
        static uint[] BuildCrc32Table()
        {
            uint[] t = new uint[256];
            for (uint i = 0; i < 256; i++)
            {
                uint c = i;
                for (int j = 0; j < 8; j++)
                    c = ((c & 1) != 0) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                t[i] = c;
            }
            return t;
        }
        static uint Crc32(byte[] buf, int len)
        {
            uint crc = 0xFFFFFFFFu;
            for (int i = 0; i < len; i++) crc = Crc32Table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
            return crc ^ 0xFFFFFFFFu;
        }

        // .mot layout (decoded by ALM_Motor_Bootloader/BSP/iap.c):
        //   [ 0..15]  16B encrypted FW_ID header        — independent CBC, fresh IV copy
        //   [16..31]  16B encrypted metadata block-0    — chain CBC starts here, fresh IV copy
        //   [32..end] encrypted firmware payload        — chain continues
        static void EncryptFile(string inputFile, string outputFile)
        {
            byte[] fileContent = File.ReadAllBytes(inputFile);

            // ---- locate FW_HW_ID block in .bin (last magic match wins; rodata addr is highest) ----
            byte[] plainHdr = new byte[FW_ID_HEADER_SIZE];
            bool fwIdFound = false;
            int lastMatchOffset = -1;
            for (int i = 0; i <= fileContent.Length - 12; i += 4)
            {
                if (BitConverter.ToUInt32(fileContent, i) == FW_ID_MAGIC) lastMatchOffset = i;
            }
            if (lastMatchOffset >= 0)
            {
                Buffer.BlockCopy(fileContent, lastMatchOffset, plainHdr, 0, 12);
                uint hdrCrc = Crc32(plainHdr, 12);
                BitConverter.GetBytes(hdrCrc).CopyTo(plainHdr, 12);
                fwIdFound = true;

                uint board = BitConverter.ToUInt32(fileContent, lastMatchOffset + 4);
                uint fwSn  = BitConverter.ToUInt32(fileContent, lastMatchOffset + 8);
                string boardAscii = "";
                bool printable = true;
                for (int i = 0; i < 4; i++)
                {
                    byte ch = (byte)((board >> (i * 8)) & 0xFF);
                    if (ch == 0) break;
                    if (ch < 0x20 || ch > 0x7E) { printable = false; break; }
                    boardAscii += (char)ch;
                }
                string boardStr = (printable && boardAscii.Length > 0)
                    ? $"\"{boardAscii}\" (0x{board:X8})"
                    : $"0x{board:X8}";
                string snTag = (fwSn == FW_SN_WILDCARD) ? "(Unlock)" : "(bound)";
                Console.WriteLine($"FW_ID: magic=0x{FW_ID_MAGIC:X8}, board={boardStr}, sn=0x{fwSn:X8}, crc=0x{hdrCrc:X8} {snTag}");
            }
            else
            {
                Console.WriteLine("WARNING: FW_ID magic (0x47544657) not found in .bin — .mot will not contain a valid FW_ID header");
            }

            // ---- pack metadata + payload ----
            byte[] userStrLen = BitConverter.GetBytes(USERSTR.Length);
            byte[] fileSize   = BitConverter.GetBytes(fileContent.Length);

            int headerSize  = 4 + USERSTR.Length + 4;
            int contentSize = fileContent.Length;
            int paddingSize = (16 - (contentSize % 16)) % 16;
            if (paddingSize == 0) paddingSize = 16;     // always at least one padding block
            int totalSize   = headerSize + contentSize + paddingSize;

            byte[] dataToEncrypt = new byte[totalSize];
            int offset = 0;
            Buffer.BlockCopy(userStrLen, 0, dataToEncrypt, offset, 4); offset += 4;
            Buffer.BlockCopy(Encoding.ASCII.GetBytes(USERSTR), 0, dataToEncrypt, offset, USERSTR.Length); offset += USERSTR.Length;
            Buffer.BlockCopy(fileSize, 0, dataToEncrypt, offset, 4); offset += 4;
            Buffer.BlockCopy(fileContent, 0, dataToEncrypt, offset, contentSize); offset += contentSize;

            // padding bytes derived from time (cosmetic; bootloader truncates by fileSize anyway)
            byte[] timeBytes = BitConverter.GetBytes(DateTime.Now.Ticks);
            for (int i = 0; i < paddingSize; i++)
                dataToEncrypt[offset + i] = timeBytes[i % timeBytes.Length];

            // ---- chain-CBC encrypt metadata + payload (fresh IV copy, advances per block) ----
            byte[] payIv = new byte[16];
            Buffer.BlockCopy(AES_IV, 0, payIv, 0, 16);
            for (int i = 0; i < totalSize; i += 16)
            {
                byte[] block = new byte[16];
                Buffer.BlockCopy(dataToEncrypt, i, block, 0, 16);
                AesCustom.Encrypt256(payIv, block, AES_KEY);
                Buffer.BlockCopy(block, 0, dataToEncrypt, i, 16);
                Buffer.BlockCopy(block, 0, payIv, 0, 16);   // chain: next IV = this ciphertext
            }

            // ---- 16B encrypted FW_ID header (independent CBC: own IV copy) ----
            byte[] encHdr = new byte[FW_ID_HEADER_SIZE];
            if (fwIdFound)
            {
                Buffer.BlockCopy(plainHdr, 0, encHdr, 0, FW_ID_HEADER_SIZE);
                byte[] hdrIv = new byte[16];
                Buffer.BlockCopy(AES_IV, 0, hdrIv, 0, 16);
                AesCustom.Encrypt256(hdrIv, encHdr, AES_KEY);
            }

            // ---- write [encHdr(16) | encrypted payload(totalSize)] ----
            using (FileStream fs = new FileStream(outputFile, FileMode.Create, FileAccess.Write))
            {
                if (fwIdFound) fs.Write(encHdr, 0, FW_ID_HEADER_SIZE);
                fs.Write(dataToEncrypt, 0, totalSize);
            }
        }
    }
}
