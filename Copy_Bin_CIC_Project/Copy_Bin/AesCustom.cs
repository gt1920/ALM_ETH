using System;

namespace Copy_Bin
{
    public class AesCustom
    {
        private static byte[] roundKey = new byte[256];
        
        // S盒数据
        private static readonly byte[,] sBox = new byte[16,16] {
            {0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76},
            {0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0},
            {0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15},
            {0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75},
            {0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84},
            {0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf},
            {0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8},
            {0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2},
            {0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73},
            {0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb},
            {0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79},
            {0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08},
            {0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a},
            {0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e},
            {0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf},
            {0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16}
        };

        // 列混淆矩阵
        private static readonly byte[,] mixColumns = new byte[4,4] {
            {0x02, 0x03, 0x01, 0x01},
            {0x01, 0x02, 0x03, 0x01},
            {0x01, 0x01, 0x02, 0x03},
            {0x03, 0x01, 0x01, 0x02}
        };

        // 轮常数
        private static readonly byte[] rcon = new byte[] { 
            0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d 
        };

        private static void SubBytes(byte[] data, int len)
        {
            for (int i = 0; i < len; i++)
            {
                byte x = (byte)(data[i] >> 4);
                byte y = (byte)(data[i] & 0x0f);
                data[i] = sBox[x,y];
            }
        }

        private static void ShiftRows(byte[] data)
        {
            ShiftByteNumRows(data, 1, 1);
            ShiftByteNumRows(data, 2, 2);
            ShiftByteNumRows(data, 3, 3);
        }

        private static void ShiftByteNumRows(byte[] data, int row, int shift)
        {
            byte[] temp = new byte[4];
            for (int i = 0; i < 4; i++)
            {
                temp[i] = data[row + i * 4];
            }
            for (int i = 0; i < 4; i++)
            {
                data[row + i * 4] = temp[(i + shift) % 4];
            }
        }

        private static byte GaloisMultiply(byte a, byte b)
        {
            byte result = 0;
            for (int i = 0; i < 8; i++)
            {
                if ((b & 1) != 0)
                {
                    result ^= a;
                }
                bool highBitSet = (a & 0x80) != 0;
                a <<= 1;
                if (highBitSet)
                {
                    a ^= 0x1b;
                }
                b >>= 1;
            }
            return result;
        }

        private static void MixColumns(byte[] data)
        {
            for (int col = 0; col < 4; col++)
            {
                byte[] temp = new byte[4];
                byte[] result = new byte[4];

                Buffer.BlockCopy(data, col * 4, temp, 0, 4);

                for (int row = 0; row < 4; row++)
                {
                    result[row] = (byte)(
                        GaloisMultiply(temp[0], mixColumns[row,0]) ^
                        GaloisMultiply(temp[1], mixColumns[row,1]) ^
                        GaloisMultiply(temp[2], mixColumns[row,2]) ^
                        GaloisMultiply(temp[3], mixColumns[row,3])
                    );
                }

                Buffer.BlockCopy(result, 0, data, col * 4, 4);
            }
        }

        public static void Encrypt256(byte[] iv, byte[] data, byte[] key)
        {
            if (key == null) return;

            // IV异或
            if (iv != null)
            {
                for (int i = 0; i < 16; i++)
                {
                    data[i] ^= iv[i];
                }
            }

            // 密钥调度
            CreateKeySchedule(key, 32);

            // 初始轮密钥异或
            for (int i = 0; i < 16; i++)
            {
                data[i] ^= roundKey[i];
            }

            // 13轮加密
            for (int round = 1; round < 14; round++)
            {
                SubBytes(data, 16);
                ShiftRows(data);
                MixColumns(data);
                for (int i = 0; i < 16; i++)
                {
                    data[i] ^= roundKey[round * 16 + i];
                }
            }

            // 最后一轮
            SubBytes(data, 16);
            ShiftRows(data);
            for (int i = 0; i < 16; i++)
            {
                data[i] ^= roundKey[14 * 16 + i];
            }

            // 更新IV
            if (iv != null)
            {
                Buffer.BlockCopy(data, 0, iv, 0, 16);
            }
        }

        private static void CreateKeySchedule(byte[] keyData, int keySize)
        {
            // 复制原始密钥
            Buffer.BlockCopy(keyData, 0, roundKey, 0, keySize);
            
            // 密钥扩展
            byte[] temp = new byte[4];
            byte[] temp0 = new byte[4];
            int roundCount;

            // 确定轮数
            if (keySize == 16)
                roundCount = 10;
            else if (keySize == 24)
                roundCount = 12;
            else if (keySize == 32)
                roundCount = 14;
            else
                return;

            // 复制最后一列数据
            Buffer.BlockCopy(keyData, keySize - 4, temp0, 0, 4);

            // 生成扩展密钥
            for (int i = 0; i < (roundCount * 4 - ((keySize - 16) / 8) * 2); i++)
            {
                if ((i % (keySize / 4)) == 0)
                {
                    // G函数
                    // 1. 循环左移一个字节
                    byte t = temp0[0];
                    temp0[0] = temp0[1];
                    temp0[1] = temp0[2];
                    temp0[2] = temp0[3];
                    temp0[3] = t;

                    // 2. S盒替换
                    SubBytes(temp0, 4);

                    // 3. 与轮常数异或
                    temp0[0] ^= rcon[i / (keySize / 4)];
                    temp0[1] ^= 0x00;
                    temp0[2] ^= 0x00;
                    temp0[3] ^= 0x00;
                }
                else if (((i % (keySize / 4)) == 4) && (keySize == 32))
                {
                    // 32字节密钥的特殊处理
                    SubBytes(temp0, 4);
                }

                // 获取前一轮对应列的数据
                Buffer.BlockCopy(roundKey, i * 4, temp, 0, 4);

                // 与计算结果异或
                for (int j = 0; j < 4; j++)
                {
                    temp0[j] ^= temp[j];
                }

                // 保存结果
                Buffer.BlockCopy(temp0, 0, roundKey, keySize + i * 4, 4);
            }
        }
    }
} 