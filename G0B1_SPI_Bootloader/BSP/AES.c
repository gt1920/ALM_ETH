
/*
  AES ����  ����
  ���ݿ�  �̶�Ϊ16�ֽ�
  ��Կ��   128bit��16�ֽڣ�    192bit��24�ֽڣ�    256bit��32�ֽڣ�
  */

#include"string.h"
//#include "stdafx.h"
#include "aes.h"

//����16����25694758ikmgtrds
unsigned char IV[16]={  // "g7k2m9pR4xW1cQ8v"
0x67,0x37,0x6b,0x32,0x6d,0x39,0x70,0x52,
0x34,0x78,0x57,0x31,0x63,0x51,0x38,0x76
};

// Key 32bytes: "Ht5bN8wE3jL6yA0fKr7dP2sV9mX4qU1z"
unsigned char Key[32]={
0x48,0x74,0x35,0x62,0x4e,0x38,0x77,0x45,
0x33,0x6a,0x4c,0x36,0x79,0x41,0x30,0x66,
0x4b,0x72,0x37,0x64,0x50,0x32,0x73,0x56,
0x39,0x6d,0x58,0x34,0x71,0x55,0x31,0x7a
};


//����Կ����   ԭʼ��Կ + �������Կ
static unsigned char Round_Key[256];

/*
* S-box transformation table    S������  ���ֽڴ�������Ҫʹ��
*/
const unsigned char aes_s_box[16][16] = {
	// 0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, // 0
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, // 1
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, // 2
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, // 3
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, // 4
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf, // 5
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, // 6
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, // 7
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, // 8
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, // 9
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, // a
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08, // b
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, // c
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, // d
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf, // e
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };// f


/*
* Inverse S-box transformation table   S�е���    ���ֽڴ�������Ҫʹ��
*/
const unsigned char  aes_inv_s_box[16][16] = {
	// 0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb, // 0
	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb, // 1
	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e, // 2
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25, // 3
	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92, // 4
	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84, // 5
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06, // 6
	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b, // 7
	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73, // 8
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e, // 9
	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, // a
	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4, // b
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f, // c
	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef, // d
	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61, // e
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d };// f



//�л��� �������  �л������������ ��������    ���л���������г������ľ������
const unsigned char aes_MixColumns[4][4] = {
	// 0     1     2      3      
	0x02, 0x03, 0x01, 0x01,  //0
	0x01, 0x02, 0x03, 0x01,  //1
	0x01, 0x01, 0x02, 0x03,  //2
	0x03, 0x01, 0x01, 0x02,  //3
};

//�л��� ����������
const unsigned char aes_invMixColumns[4][4] = {
	// 0     1     2      3      
	0x0E, 0x0B, 0x0D, 0x09,  //0
	0x09, 0x0E, 0x0B, 0x0D,  //1
	0x0D, 0x09, 0x0E, 0x0B,  //2
	0x0B, 0x0D, 0x09, 0x0E,  //3
};

//�ֳ���  ����ԿG�����õ�  ��һ������Ϊ ǰһ��������GF(28)���ϳ�2
const unsigned char aes_Rcon[14] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d };


//AES�ֽ��������
void Aes_SubBytes(unsigned char *Byte_IN_OUT, unsigned int Len)
{
	unsigned char S_X, S_Y;
	unsigned int Count = 0;
	for (Count = 0; Count < Len; Count++)
	{
		S_X = Byte_IN_OUT[Count] >> 4;   //�õ���4λ��ֵ
		S_Y = Byte_IN_OUT[Count] & 0x0f;
		Byte_IN_OUT[Count] = aes_s_box[S_X][S_Y];  //�õ���Ӧ��ֵ
	}
}
//AES �����ֽ��������
void Aes_invSubBytes(unsigned char *Byte_IN_OUT, unsigned int Len)
{
	unsigned char S_X, S_Y;
	unsigned int Count = 0;
	for (Count = 0; Count < Len; Count++)
	{
		S_X = Byte_IN_OUT[Count] >> 4;   //�õ���4λ��ֵ
		S_Y = Byte_IN_OUT[Count] & 0x0f;
		Byte_IN_OUT[Count] = aes_inv_s_box[S_X][S_Y];  //�õ���Ӧ��ֵ
	}
}

//����ָ�������� ָ���ֽ�
//SrcInOut��  �������뼰������ ����Ϊ16�ֽ�  ���ڲ��ֽ��4X4����
//Rows��      ����ڼ���  0~3
//Number:     �����ֽ���  1~3
void ShiftByteNumRows(unsigned char *SrcInOut, unsigned char Rows, unsigned char Number)
{
	unsigned char Data[10];  //������λ���ݱ���
	unsigned char Count;
	if ((Number > 10) || (Number == 0))
	{
		return;
	}
	for (Count = 0; Count < Number; Count++) //�Ȱ�Ҫ������������ݱ���  
	{
		Data[Count] = SrcInOut[Rows + 4 * Count];
	}
	for (Count = 0; Count < (4 - Number); Count++)  //�Ѻ������������
	{
		SrcInOut[Rows + 4 * Count] = SrcInOut[Rows + 4 * (Number + Count)];
	}
	for (Count = 0; Count < Number; Count++)  //�Ѻ������������
	{
		SrcInOut[Rows + 4 * ((4 - Number) + Count)] = Data[Count];
	}
}
//���ƶ�
void Aes_ShiftRows(unsigned char *SrcInOut)
{
	//const unsigned char Shitftable[4] = { 0, 1, 2, 3 };
	//ShiftByteNumRows(SrcInOut, 4, 0);  //��һ�в���λ
	ShiftByteNumRows(SrcInOut, 1, 1);    //����1���ֽ�
	ShiftByteNumRows(SrcInOut, 2, 2);    //����2���ֽ�
	ShiftByteNumRows(SrcInOut, 3, 3);    //����3���ֽ�
}
//�������ƶ�
void Aes_invShiftRows(unsigned char *SrcInOut)
{
	//const unsigned char Shitftable[4] = { 0, 1, 2, 3 };
	//ShiftByteNumRows(SrcInOut, 4, 0);  //��һ�в���λ
	ShiftByteNumRows(SrcInOut, 1, 3);    //֮ǰ����1���ֽ� ������Ϊ����3���ֽ�
	ShiftByteNumRows(SrcInOut, 2, 2);    //����2���ֽ�     ������Ϊ����2���ֽ�
	ShiftByteNumRows(SrcInOut, 3, 1);    //����3���ֽ�     ������Ϊ����1���ֽ�
}

//������������GF��28�����ϵĳ˷�
unsigned char  Get_Calculate_GF28(unsigned char data0, unsigned char data1)
{
	unsigned char Val = 0;
	unsigned char Count = 0;
	//�ѱ������ֳɵ���bitλ��ɵĽ��  ����  0x03*0x14 =0x03*00010100 =0x03*0x04 ^ 0x03*0x10  =((0x03*2)*2) ^ .... =  0x0c ^.... 
	for (Count = 0; Count < 8; Count++)
	{
		if (data1 & 0x01)  //�жϵ�ǰ
		{
			Val ^= data0;
		}
		data0 = (data0 << 1) ^ ((data0 & 0x80) ? 0x1b : 0); //ÿѭ��һ�γ˷���ֵ�������
		data1 >>= 1; //ɨ����һλ
	}
	return Val;
}
//�л���
void Aes_MixColumns(unsigned char *SrcInOut)
{
	unsigned char Data_buff[4];  //һ�����ݱ���
	unsigned char Calculate_val[4];   //������ֵ
	unsigned char Count; //�м���
	for (Count = 0; Count < 4; Count++)  //4 ��
	{
		memcpy(Data_buff, &SrcInOut[Count * 4], 4); //����һ�е�����
		//ִ�� GF��28�����ϵĳ˷�
		Calculate_val[0] = Get_Calculate_GF28(Data_buff[0], aes_MixColumns[0][0]) ^ Get_Calculate_GF28(Data_buff[1], aes_MixColumns[0][1]) ^ \
			Get_Calculate_GF28(Data_buff[2], aes_MixColumns[0][2]) ^ Get_Calculate_GF28(Data_buff[3], aes_MixColumns[0][3]);

		Calculate_val[1] = Get_Calculate_GF28(Data_buff[0], aes_MixColumns[1][0]) ^ Get_Calculate_GF28(Data_buff[1], aes_MixColumns[1][1]) ^ \
			Get_Calculate_GF28(Data_buff[2], aes_MixColumns[1][2]) ^ Get_Calculate_GF28(Data_buff[3], aes_MixColumns[1][3]);

		Calculate_val[2] = Get_Calculate_GF28(Data_buff[0], aes_MixColumns[2][0]) ^ Get_Calculate_GF28(Data_buff[1], aes_MixColumns[2][1]) ^ \
			Get_Calculate_GF28(Data_buff[2], aes_MixColumns[2][2]) ^ Get_Calculate_GF28(Data_buff[3], aes_MixColumns[2][3]);

		Calculate_val[3] = Get_Calculate_GF28(Data_buff[0], aes_MixColumns[3][0]) ^ Get_Calculate_GF28(Data_buff[1], aes_MixColumns[3][1]) ^ \
			Get_Calculate_GF28(Data_buff[2], aes_MixColumns[3][2]) ^ Get_Calculate_GF28(Data_buff[3], aes_MixColumns[3][3]);

		memcpy(&SrcInOut[Count * 4], Calculate_val, 4); //�������
	}
}

//�����л���
void Aes_invMixColumns(unsigned char *SrcInOut)
{
	unsigned char Data_buff[4];  //һ�����ݱ���
	unsigned char Calculate_val[4];   //������ֵ
	unsigned char Count; //�м���
	for (Count = 0; Count < 4; Count++)  //4 ��
	{
		memcpy(Data_buff, &SrcInOut[Count * 4], 4); //����һ�е�����
		//ִ�� GF��28�����ϵĳ˷�
		Calculate_val[0] = Get_Calculate_GF28(Data_buff[0], aes_invMixColumns[0][0]) ^ Get_Calculate_GF28(Data_buff[1], aes_invMixColumns[0][1]) ^ \
			Get_Calculate_GF28(Data_buff[2], aes_invMixColumns[0][2]) ^ Get_Calculate_GF28(Data_buff[3], aes_invMixColumns[0][3]);

		Calculate_val[1] = Get_Calculate_GF28(Data_buff[0], aes_invMixColumns[1][0]) ^ Get_Calculate_GF28(Data_buff[1], aes_invMixColumns[1][1]) ^ \
			Get_Calculate_GF28(Data_buff[2], aes_invMixColumns[1][2]) ^ Get_Calculate_GF28(Data_buff[3], aes_invMixColumns[1][3]);

		Calculate_val[2] = Get_Calculate_GF28(Data_buff[0], aes_invMixColumns[2][0]) ^ Get_Calculate_GF28(Data_buff[1], aes_invMixColumns[2][1]) ^ \
			Get_Calculate_GF28(Data_buff[2], aes_invMixColumns[2][2]) ^ Get_Calculate_GF28(Data_buff[3], aes_invMixColumns[2][3]);

		Calculate_val[3] = Get_Calculate_GF28(Data_buff[0], aes_invMixColumns[3][0]) ^ Get_Calculate_GF28(Data_buff[1], aes_invMixColumns[3][1]) ^ \
			Get_Calculate_GF28(Data_buff[2], aes_invMixColumns[3][2]) ^ Get_Calculate_GF28(Data_buff[3], aes_invMixColumns[3][3]);

		memcpy(&SrcInOut[Count * 4], Calculate_val, 4); //�������
	}
}

//AES ��Կ��չ�� ��G����
//����λ  ��S�����  �����ֳ��� ���
void Aes_G_function(unsigned char *Wjbuff_IN_OUT, unsigned char Count_j)
{
	unsigned char temp;
	//����1���ֽ�
	temp = Wjbuff_IN_OUT[0];
	memcpy(Wjbuff_IN_OUT, &Wjbuff_IN_OUT[1], 3);
	Wjbuff_IN_OUT[3] = temp;
	Aes_SubBytes(Wjbuff_IN_OUT, 4);       //S���滻

	Wjbuff_IN_OUT[0] ^= aes_Rcon[Count_j];  //�볣���� ���
	Wjbuff_IN_OUT[1] ^= 0x00;
	Wjbuff_IN_OUT[2] ^= 0x00;
	Wjbuff_IN_OUT[3] ^= 0x00;
}

void Aes_Bytes_Xor(unsigned char *Byte_IO, unsigned char *Byte_I, unsigned int Len)
{
	while (Len--)
	{
		*Byte_IO ^= *Byte_I;
		Byte_IO++;
		Byte_I++;
	}
}

//��Կ��չ
//KeySize: ��Կ��С ��λ���ֽ�  ֻ��Ϊ16  24  32 
void Key_Schedule(unsigned char *KeyData_In, unsigned char KeySize)
{
	unsigned char Datatemp[4];
	unsigned char Datatemp0[4]; //����Ľ��
	unsigned char Count;
	unsigned char Round_Count;  //�ּ���  ���Ϊ14*4 = 56
	if (KeySize == 16)   //�����ԿΪ16�ֽ�
	{
		Round_Count = 10;  //�������
	}
	else if (KeySize == 24)
	{
		Round_Count = 12;  //�������
	}
	else if (KeySize == 32)
	{
		Round_Count = 14;  //�������
	}
	else //�������
	{
		return;
	}
	//����ԭʼ��Կ ���һ������
	memcpy(Datatemp0, &KeyData_In[KeySize - 4], 4);
	//ÿ����Ҫһ������Կ   ÿ������Կ16�ֽ�  ÿ��ѭ������4���ֽ�   ��ѭ��4�βŲ���һ���ֽ���Կ  
	//��ԭʼ��Կ����16�ֽ�ʱ  ����16�ֽڵ�����Ϊ����Կ����   ����24�ֽ���Կ  ��ԭʼ��Կ�еĺ�8���ֽڼ�Ϊ��һ������Կ��ǰ8���ֽ�����    32�ֽ���Կ�ĺ�16�ֽ����� Ϊ��һ������Կ����
	//���� * 4 ��Ϊԭʼ��Կ������ֽ����   ���ǵ�ԭʼ����16�ֽ�ʱ  ��Ҫ����ԭʼ��Կ�����������Կ���
	for (Count = 0; Count < (Round_Count * 4 - ((KeySize - 16) / 8) * 2)/*��ȥԭʼ��Կ�������Կ���ݳ���*/; Count++)
	{
		//�������Ϊ4����6����8����������ȡ������Կ��С��  ��Ҫִ��G����   ������ֻ��Ҫ���Ӧ����� ��ԭ��Կ������ ����������������
		if ((Count % (KeySize / 4)) == 0) //����Ϊ4�ı���  ����ֵ Wi = W(i-4) ^ G( W(i-1) )
		{
			Aes_G_function(Datatemp0, Count / (KeySize / 4));     //ִ��G����  ִ�еĽ��������ԭ������
		}
		else
		{
			//���Է��� �ϴμ���Ľ����Ϊ��Ҫ��������  ��û�����¶�ȡ����
			if (((Count % (KeySize / 4)) == 4) && (KeySize == 32))  //��32�ֽ���Կ��  �����ǰ����Ϊ4�ı���  ��Ҫ����һ��S�����
			{
				Aes_SubBytes(Datatemp0, 4);       //S���滻
			}
		}
		memcpy(Datatemp, &KeyData_In[Count * 4], 4); //�õ�ָ��һ�е����� 
		Aes_Bytes_Xor(Datatemp0, Datatemp, 4);  //�������������
		memcpy(&KeyData_In[KeySize + Count * 4], Datatemp0, 4);  //�������
	}
}

//��������Կ 128bit ��Կ Ϊ10������Կ   192bit��ԿΪ   12������Կ   256bitΪ14������Կ    ÿ������Կ�̶�Ϊ16    
//KeyByteSize: ��Կ�ֽ���    ֻ��Ϊ 16  24  32 
void Aes_Key_Schedule_Create(unsigned char *KeyData, unsigned char KeyByteSize)
{
	memcpy(Round_Key, KeyData, KeyByteSize); //��ԭʼ��Կ����    ������ȫ�ֱ���
	//��ʼ����ԭʼ��Կ��չ ����Կ
	Key_Schedule(Round_Key, KeyByteSize);  //��������Կ
}



//AES����  16�ֽ�һ�����ݿ�
//IV_IN_OUT:        ��������  �������
//State_IN_OUT��    ��������  �������
//key128bit:        ��Կ  128bit  16�ֽ�
void Aes_IV_key128bit_Encrypt(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key128bit)
{
	unsigned char Count;
	if (key128bit == NULL)
	{
		return;
	}
	if (IV_IN_OUT != NULL)  //�������������  �����������������
	{
		Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);
	}
	Aes_Key_Schedule_Create(key128bit, 16); //��������Կ   ��11��  ����Ϊ 16�ֽ�
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[0], 16); //�ѵ�0����Կ���������
	for (Count = 1; Count < 10; Count++)  //����9�� ��10û���л���  �̵�������
	{
		Aes_SubBytes(State_IN_OUT, 16);  //�ֽ����
		Aes_ShiftRows(State_IN_OUT); //����λ
		Aes_MixColumns(State_IN_OUT);//�л���
		Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);     //������Կ���
	}
	//��10��----------------------------
	Aes_SubBytes(State_IN_OUT, 16);  //�ֽ����
	Aes_ShiftRows(State_IN_OUT); //����λ
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[16 * Count], 16);     //�������Կ
	if (IV_IN_OUT != NULL)  //�����Ŀ���������
	{
		memcpy(IV_IN_OUT, State_IN_OUT,16);
	}
}

//AES����  16�ֽ�һ�����ݿ�
//IV_IN_OUT:        ��������  ԭ�������
//State_IN_OUT��    ��������  �������
//key128bit:        ��Կ  128bit  16�ֽ�
void Aes_IV_key128bit_Decode(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key128bit)
{
	unsigned char Count;
	unsigned char Temp[16];  //ԭ�������ݻ���
	if (key128bit == NULL)
	{
		return;
	}
	if (IV_IN_OUT!=NULL)
	{
		memcpy(Temp, State_IN_OUT, 16);  //����ԭʼ����
	}
	Aes_Key_Schedule_Create(key128bit, 16); //��������Կ   ��11��  ����Ϊ 16�ֽ�
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[10 * 16], 16); //�ѵ�10����Կ�����������
	Aes_invShiftRows(State_IN_OUT);       //��������λ
	Aes_invSubBytes(State_IN_OUT, 16);    //�����ֽ����
	for (Count = 9; Count > 0; Count--)  //����9�� ��10û���л���  �̵�������
	{
		Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);     //������Կ���
		Aes_invMixColumns(State_IN_OUT);    //�����л���
		Aes_invShiftRows(State_IN_OUT);     //��������λ
		Aes_invSubBytes(State_IN_OUT, 16);  //�����ֽ����	
	}
	//��10��----------------------------
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[16 * Count], 16);     //�������Կ  �˴����Ϊԭʼ��Կ
	if (IV_IN_OUT != NULL)
	{
		Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);  //���ܺ�Ľ�����������
		memcpy(IV_IN_OUT, Temp, 16);  //����ԭʼ���ĵ���������
	}
}




//AES����  16�ֽ�һ�����ݿ�
//IV_IN_OUT:        ��������  �������
//State_IN_OUT��    ��������  �������
//key192bit:        ��Կ  192bit  24�ֽ�
void Aes_IV_key192bit_Encrypt(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key192bit)
{
	unsigned char Count;
	if (key192bit == NULL)
	{
		return;
	}
	if (IV_IN_OUT != NULL)  //�������������  �����������������
	{
		Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);
	}
	Aes_Key_Schedule_Create(key192bit, 24); //��������Կ   ��11��  ����Ϊ 24�ֽ�
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[0], 16); //�ѵ�0����Կ���������
	for (Count = 1; Count < 12; Count++)  //����11�� ��12û���л���  �̵�������
	{
		Aes_SubBytes(State_IN_OUT, 16);  //�ֽ����
		Aes_ShiftRows(State_IN_OUT); //����λ
		Aes_MixColumns(State_IN_OUT);//�л���
		Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);     //������Կ���
	}
	//��10��----------------------------
	Aes_SubBytes(State_IN_OUT, 16);  //�ֽ����
	Aes_ShiftRows(State_IN_OUT); //����λ
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);     //�������Կ
	if (IV_IN_OUT != NULL)  //�����Ŀ���������
	{
		memcpy(IV_IN_OUT, State_IN_OUT, 16);
	}
}

//AES����  16�ֽ�һ�����ݿ�
//IV_IN_OUT:        ��������  ԭ�������
//State_IN_OUT��    ��������  �������
//key192bit:        ��Կ  192bit  24�ֽ�
void Aes_IV_key192bit_Decode(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key192bit)
{
	unsigned char Count;
	unsigned char Temp[16];  //ԭ�������ݻ���
	if (key192bit == NULL)
	{
		return;
	}
	if (IV_IN_OUT != NULL)
	{
		memcpy(Temp, State_IN_OUT, 16);  //����ԭʼ����
	}
	Aes_Key_Schedule_Create(key192bit, 24); //��������Կ   ��12��  ����Ϊ 24�ֽ�
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[12 * 16], 16); //�ѵ�12����Կ�����������
	Aes_invShiftRows(State_IN_OUT);       //��������λ
	Aes_invSubBytes(State_IN_OUT, 16);    //�����ֽ����
	for (Count = 11; Count > 0; Count--)  //����11�� ��12û���л���  �̵�������
	{
		Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);     //������Կ���
		Aes_invMixColumns(State_IN_OUT);    //�����л���
		Aes_invShiftRows(State_IN_OUT);     //��������λ
		Aes_invSubBytes(State_IN_OUT, 16);  //�����ֽ����	
	}
	//��12��----------------------------
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[16 * Count], 16);     //�������Կ  �˴����Ϊԭʼ��Կ
	if (IV_IN_OUT != NULL)
	{
		Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);  //���ܺ�Ľ�����������
		memcpy(IV_IN_OUT, Temp, 16);  //����ԭʼ���ĵ���������
	}
}


//AES����  16�ֽ�һ�����ݿ�
//IV_IN_OUT:        ��������  �������
//State_IN_OUT��    ��������  �������
//key256bit:        ��Կ  256bit  32�ֽ�
void Aes_IV_key256bit_Encrypt(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key256bit)
{
	unsigned char Count;
	if (key256bit == NULL)
	{
		return;
	}
	if (IV_IN_OUT != NULL)  //�������������  �����������������
	{
		Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);
	}
	Aes_Key_Schedule_Create(key256bit, 32); //��������Կ   ��14��  ����Ϊ 32�ֽ�
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[0], 16); //�ѵ�0����Կ���������
	for (Count = 1; Count < 14; Count++)  //����13�� ��14û���л���  �̵�������
	{
		Aes_SubBytes(State_IN_OUT, 16);  //�ֽ����
		Aes_ShiftRows(State_IN_OUT); //����λ
		Aes_MixColumns(State_IN_OUT);//�л���
		Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);     //������Կ���
	}
	//��14��----------------------------
	Aes_SubBytes(State_IN_OUT, 16);  //�ֽ����
	Aes_ShiftRows(State_IN_OUT); //����λ
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);     //�������Կ
	if (IV_IN_OUT != NULL)  //�����Ŀ���������
	{
		memcpy(IV_IN_OUT, State_IN_OUT, 16);
	}
}

//AES����  16�ֽ�һ�����ݿ�
//IV_IN_OUT:        ��������  ԭ�������
//State_IN_OUT��    ��������  �������
//key256bit:        ��Կ  256bit  32�ֽ�
void Aes_IV_key256bit_Decode(unsigned char *IV_IN_OUT, unsigned char *State_IN_OUT, unsigned char *key256bit)
{
	unsigned char Count;
	unsigned char Temp[16];  //ԭ�������ݻ���
	if (key256bit == NULL)
	{
		return;
	}
	if (IV_IN_OUT != NULL)
	{
		memcpy(Temp, State_IN_OUT, 16);  //����ԭʼ����
	}
	Aes_Key_Schedule_Create(key256bit, 32); //��������Կ   ��11��  ����Ϊ 32�ֽ�
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[14 * 16], 16); //�ѵ�14����Կ�����������
	Aes_invShiftRows(State_IN_OUT);       //��������λ
	Aes_invSubBytes(State_IN_OUT, 16);    //�����ֽ����
	for (Count = 13; Count > 0; Count--)  //����13�� ��14û���л���  �̵�������
	{
		Aes_Bytes_Xor(State_IN_OUT, &Round_Key[Count * 16], 16);     //������Կ���
		Aes_invMixColumns(State_IN_OUT);    //�����л���
		Aes_invShiftRows(State_IN_OUT);     //��������λ
		Aes_invSubBytes(State_IN_OUT, 16);  //�����ֽ����	
	}
	//��14��----------------------------
	Aes_Bytes_Xor(State_IN_OUT, &Round_Key[16 * Count], 16);     //�������Կ  �˴����Ϊԭʼ��Կ
	if (IV_IN_OUT != NULL)
	{
		Aes_Bytes_Xor(State_IN_OUT, IV_IN_OUT, 16);  //���ܺ�Ľ�����������
		memcpy(IV_IN_OUT, Temp, 16);  //����ԭʼ���ĵ���������
	}
}



//int main(void)
//{
//	unsigned char Temp[16]={0x2B,0XA3,0xDE,0xB4,0x2B,0x0E,0x8A,0xCB,0xD7,0x02,0x65,0x74,0x96,0x23,0xD8,0x25};  //ԭ�������ݻ���
//	unsigned char IV[16]={0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32};  
//	unsigned char Key[32]={0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32,0x31,0X32};  //ԭ�������ݻ���
//	Aes_IV_key256bit_Decode(IV,Temp,Key);
//	for(int i=0;i<16;i++)
//	{
//		printf("%02x ",Temp[i]);	
//	}
//	
// } 





