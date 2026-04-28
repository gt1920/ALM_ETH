using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using Copy_Bin;

namespace Copy_Bin
{
    class Program
    {
        static void Main(string[] args)
        {
            // 检查输入参数
            if (args.Length == 0)
            {
                Console.WriteLine("请输入要复制的文件名，例如：Copy_Bin.exe B85");
                return;
            }

            string sourceFileName = args[0] + ".bin";
            string targetFileName = args[0] + "_" + DateTime.Now.ToString("yyMMdd") + "_" + DateTime.Now.ToString("HHmm") + ".bin";

            string sourceDir = @"..\Output";
            string targetDir = @"..\HEX";

            // 构造源文件和目标文件的完整路径
            string sourceFile = Path.Combine(sourceDir, sourceFileName);
            string targetFile = Path.Combine(targetDir, targetFileName);

            // 检查源文件是否存在
            if (!File.Exists(sourceFile))
            {
                Console.WriteLine($"在 {sourceDir} 目录中找不到 {sourceFileName} 文件。");
                return;
            }

            // 复制源文件到目标文件
            File.Copy(sourceFile, targetFile, true);
            Console.WriteLine($"{sourceFileName} {targetDir}\\{targetFileName} was creat!");

            // 在 HEX\app与Bootloader合并0x3800_115200 目录中查是否存在 app.bin 文件，如果存在就删除它
            string appDir = Path.Combine(targetDir, "app");
            string appFile = Path.Combine(appDir, "app.bin");            
            if (File.Exists(appFile))
            {
                File.Delete(appFile);
            }

            string HEXFile = Path.Combine(appDir, "Full_Package.hex");
            if (File.Exists(HEXFile))
            {
                File.Delete(HEXFile);
            }

            // 将目标文件再复制一份到 HEX\app与Bootloader合并0x3800_115200 目录中，并且重命名为 app.bin
            File.Copy(targetFile, appFile, true);
            //Console.WriteLine($"{targetFileName} 文件已经成功复制到 {appDir} 目录，并且重命名为 app.bin。");

            // 将当前的工作目录设置为 HEX\app与Bootloader合并0x3800_115200 目录
            Directory.SetCurrentDirectory(appDir);

            //Console.WriteLine("Current working directory: " + appDir);
            // 执行 run.bat 文
            //string appBatchFile = Path.Combine(appDir, "Run.bat");
            string appBatchFile = "Run.bat";

            //Console.WriteLine("appBatchFile: " + appBatchFile);

            //Process.Start(new ProcessStartInfo(appBatchFile));

            ProcessStartInfo startInfo = new ProcessStartInfo(appBatchFile)
            {
                //WorkingDirectory = appDir,
                WindowStyle = ProcessWindowStyle.Normal,
                CreateNoWindow = false
            };

            Process process = new Process();
            process.StartInfo = startInfo;
            process.Start();

            // 等待 run.bat 执行完毕
            process.WaitForExit();

            // 将当前的工作目录恢复到原来的目录
            Directory.SetCurrentDirectory(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location));

            // 检查 app 目录下是否有 Full_Package.hex 文件，如果有就将其复制到 HEX 目录下
            string fullPackageFile = Path.Combine(appDir, "Full_Package.hex");

            if (File.Exists(fullPackageFile))
            {
                string targetHexFile = Path.Combine(targetDir, args[0] + "_" + DateTime.Now.ToString("yyMMdd") + "_" + DateTime.Now.ToString("HHmm") + ".hex");
                File.Copy(fullPackageFile, targetHexFile, true);
                Console.WriteLine($"{targetDir}\\{Path.GetFileName(targetHexFile)} was creat!");               
                File.Delete(fullPackageFile);            
            }
            else
            {
                Console.WriteLine($"在 {appDir} 目录中找不到 Full_Package.hex 文件。");
            }

            // 加密 .bin 文件为 .cic 格式。文件名里嵌入 fw_sn 方便区分绑定目标
            string snTag = ExtractFwSnTag(targetFile);
            string gtFile = Path.Combine(targetDir, args[0] + "_" + snTag + "_" + DateTime.Now.ToString("yyMMdd") + "_" + DateTime.Now.ToString("HHmm") + ".cic");
            EncryptFile(targetFile, gtFile);
            Console.WriteLine($"{targetFile} 已加密为 {gtFile}。");

            // 删除复制过来的 .bin 文件
            if (File.Exists(targetFile))
            {
                File.Delete(targetFile);
                Console.WriteLine($"{targetFile} 已被删除。");
            }

        }

        // 扫描 bin 取 fw_sn, 返回用于拼接文件名的 SN 标签:
        //   通配 -> "Unlock"；绑定 -> 8位大写HEX (如 "0630003E")；未找到 -> "NoFwID"
        static string ExtractFwSnTag(string binPath)
        {
            const uint FW_ID_MAGIC    = 0x47544657;
            const uint FW_SN_WILDCARD = 0xA5C3F09E;

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

        // CRC32 (多项式 0xEDB88320, 初始值 0xFFFFFFFF, 结果异或 0xFFFFFFFF)
        // 与 iap.c CRC32TableCreate 完全一致
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
            uint ret = 0xFFFFFFFFu;
            for (int i = 0; i < len; i++)
                ret = Crc32Table[(ret ^ buf[i]) & 0xFF] ^ (ret >> 8);
            return ret ^ 0xFFFFFFFFu;
        }

        static void EncryptFile(string inputFile, string outputFile)
        {
            const string m_Userstr = "LEDFW012";      // 8 bytes
            // Key/IV must match ALM_CIC_Bootloader/BSP/aes.c
            const string m_IVStr = "u3F9hM2zE6vK1oQ5"; // 16 bytes
            const string m_KeyStr = "Lp7kZ4cN9bX2vQ6mT8yJ3sD5fW0rH1aE"; // 32 bytes
            const uint FW_ID_MAGIC = 0x47544657; // "GTFW"
            const int FW_ID_HEADER_SIZE = 16;    // 密文头: magic(4)+board(4 ASCII)+sn(4)+crc32(4)
            const uint FW_SN_WILDCARD = 0xA5C3F09E;

            byte[] fileContent = File.ReadAllBytes(inputFile);

            // 搜索 .bin 中的 FW_ID magic，取最后一个匹配（真正的 FW_HW_ID 在 .rodata 地址最高）
            // 抽出 12 字节字段头，拼 4 字节 CRC32(12B) 成 16 字节明文头
            byte[] plainHdr = new byte[FW_ID_HEADER_SIZE];
            bool fwIdFound = false;
            int lastMatchOffset = -1;
            for (int i = 0; i <= fileContent.Length - 12; i += 4)
            {
                uint val = BitConverter.ToUInt32(fileContent, i);
                if (val == FW_ID_MAGIC)
                {
                    lastMatchOffset = i;
                }
            }
            if (lastMatchOffset >= 0)
            {
                Buffer.BlockCopy(fileContent, lastMatchOffset, plainHdr, 0, 12);
                uint hdrCrc = Crc32(plainHdr, 12);
                BitConverter.GetBytes(hdrCrc).CopyTo(plainHdr, 12);
                fwIdFound = true;
                // New FW_HW_ID layout: magic(4) + board(4 ASCII LE) + sn(4)  → 12 B
                // Board is ASCII-packed little-endian; e.g. "ETH\0" = 0x00485445
                uint   board = BitConverter.ToUInt32(fileContent, lastMatchOffset + 4);
                uint   fwSn  = BitConverter.ToUInt32(fileContent, lastMatchOffset + 8);

                string boardAscii = "";
                bool   printable  = true;
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

                string snTag = (fwSn == FW_SN_WILDCARD)
                    ? "(Unlock - 通配，任何 MCU 可装)"
                    : $"(绑定 MCU SN)";
                Console.WriteLine($"FW_ID: magic=0x{FW_ID_MAGIC:X8}, board={boardStr}, sn=0x{fwSn:X8}, crc=0x{hdrCrc:X8} {snTag}");
            }
            if (!fwIdFound)
            {
                Console.WriteLine("警告: bin中未找到FW_ID magic(0x47544657)，.cic文件不含FW_ID头");
            }

            byte[] userStrLen = BitConverter.GetBytes(m_Userstr.Length);
            byte[] fileSize = BitConverter.GetBytes(fileContent.Length);

            int headerSize = 4 + m_Userstr.Length + 4;
            int contentSize = fileContent.Length;
            int paddingSize = (16 - (contentSize % 16)) % 16;
            if (paddingSize == 0) paddingSize = 16; // 如果是16的整数倍，增加一个16字节的块
            int totalSize = headerSize + contentSize + paddingSize;

            byte[] dataToEncrypt = new byte[totalSize];
            int offset = 0;

            Buffer.BlockCopy(userStrLen, 0, dataToEncrypt, offset, 4);
            offset += 4;
            Buffer.BlockCopy(Encoding.ASCII.GetBytes(m_Userstr), 0, dataToEncrypt, offset, m_Userstr.Length);
            offset += m_Userstr.Length;
            Buffer.BlockCopy(fileSize, 0, dataToEncrypt, offset, 4);
            offset += 4;

            Buffer.BlockCopy(fileContent, 0, dataToEncrypt, offset, contentSize);
            offset += contentSize;

            // 使用当前时间填充不足16字节的部分
            byte[] timeBytes = BitConverter.GetBytes(DateTime.Now.Ticks);
            for (int i = 0; i < paddingSize; i++)
            {
                dataToEncrypt[offset + i] = timeBytes[i % timeBytes.Length];
            }

            // 加密数据
            byte[] iv = Encoding.ASCII.GetBytes(m_IVStr);
            byte[] key = Encoding.ASCII.GetBytes(m_KeyStr);
            byte[] currentIv = new byte[16];
            Buffer.BlockCopy(iv, 0, currentIv, 0, 16);

            for (int i = 0; i < totalSize; i += 16)
            {
                byte[] block = new byte[16];
                Buffer.BlockCopy(dataToEncrypt, i, block, 0, 16);

                AesCustom.Encrypt256(currentIv, block, key);

                Buffer.BlockCopy(block, 0, dataToEncrypt, i, 16);
                Buffer.BlockCopy(block, 0, currentIv, 0, 16);
            }

            // 对 16B 明文头做独立 CBC：本地 IV 副本加密一块 → 16B 密文头
            // 不共享全局 IV，与 Bootloader 端对头的独立解密一一对应
            byte[] encHdr = new byte[FW_ID_HEADER_SIZE];
            if (fwIdFound)
            {
                Buffer.BlockCopy(plainHdr, 0, encHdr, 0, FW_ID_HEADER_SIZE);
                byte[] ivHdr = new byte[16];
                Buffer.BlockCopy(iv, 0, ivHdr, 0, 16);
                AesCustom.Encrypt256(ivHdr, encHdr, key);
            }

            // 输出: [16B 密文头 (AES-CBC, 独立 IV)] + [AES 加密的 Metadata+Payload (独立 IV 链)]
            using (var fs = new FileStream(outputFile, FileMode.Create))
            {
                if (fwIdFound)
                {
                    fs.Write(encHdr, 0, FW_ID_HEADER_SIZE);
                }
                fs.Write(dataToEncrypt, 0, totalSize);
            }
        }
    }
}