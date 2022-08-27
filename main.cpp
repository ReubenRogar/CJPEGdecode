#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include "RC4.h"
#define MAXSIZE 1048576
#define uint unsigned int
#define byte unsigned char
#define ulong unsigned long
#define ushort unsigned short
#define SBOXSIZE 256*8
char key[256];
struct JNode
{
    uint id;
    uint offset;
    uint lenth;
};//存储不同段的开始位置和长度



struct DHTstruct
{
    struct JNode *location;
    int num;
    ushort ****Huffman;
};

//功能函数
uint Big2Little16(byte data[]); //大端转小端16bit
uint bytelow4(byte b);
uint bytehigh4(byte b);
ushort** genHuffman(byte JbinDHT[],int *offset);
ushort hufDecode(int *k, ushort **code,byte bits[]);
int numDecode(int k, ushort code,byte bits[]);
void xorC(byte* bits, byte* Sboxbits,uint  Boffset, int j,uint codeLenth);


//过程函数
//Jpeg读取
uint JpegRead(byte Jbin[],char *filePath)
{

    FILE *fin = fopen(filePath, "rb");
    if(fin==NULL)
    {
        printf("FILE failde to open\n");
        exit(EXIT_FAILURE);
    }
    uint lenth = fread(Jbin, sizeof(byte), MAXSIZE, fin);//lenth 文件总字节数
    fclose(fin);
    return lenth;
}


void isJPEG(byte Jbin[],uint lenth) //判断头尾
{
    if(Big2Little16(Jbin) != 0xffd8 || Big2Little16(Jbin+lenth-2)!=0xffd9)
    {
        printf("SOT EOI can't found\n");
        exit(EXIT_FAILURE);
    }
    return;
}

void Seglocate(byte Jbin[],uint lenth,struct JNode *node)//定位区段
{
    uint flag = 0;
    for (int i = 0; i < lenth - 2; i++)
    {
        if(Big2Little16(Jbin+i)==node->id)
        {
            flag = 1;
            node->offset = i;
            node->lenth = Big2Little16(Jbin + i + 2);
            break;
        }
    }
    if(flag==0)//未找到区段
    {
        printf("JPEG segment can't locate\n");
        printf("Identifier:%x\n", node->id);
        exit(EXIT_FAILURE);
    }
    //printf("%x %x %d\n", node->id, node->offset, node->lenth);
}

int DHTmutiLocate(byte Jbin[],uint lenth,struct JNode DHTs[])
{
    int numDHT = 1;
    for (int i = DHTs[0].offset + DHTs[0].lenth + 2, j = 1; i < lenth - 2 && j < 4; i++)
    {
        if(Big2Little16(Jbin+i)==0xffc4)
        {
            DHTs[j].id = 0xFFC4;
            DHTs[j].offset = i;
            DHTs[j].lenth = Big2Little16(Jbin + i + 2);
            i += (1 + DHTs[j].lenth);
            j++;
            numDHT = j;
        }
    }
    return numDHT;
}

void readHuffman(byte Jbin[],struct DHTstruct *DHTdata)
{
    int num = DHTdata->num;
    struct JNode *location = DHTdata->location;
    ushort ****Huffman;
    Huffman = (ushort ****)malloc(sizeof(ushort ***) * 2);
    Huffman[0] = (ushort ***)malloc(sizeof(ushort **) * 2);
    Huffman[1] = (ushort ***)malloc(sizeof(ushort **) * 2);
    //只考虑 一个DHT有四个huffman 树和 四个DHT 的情况

    if(num==1)
    {
        int offset=location[0].offset+4;
        for (int i = 0; i < 4;i++)//待修改
        {
            uint dcac = bytehigh4(Jbin[offset]);
            uint id = bytelow4(Jbin[offset]);
            offset++;
            Huffman[dcac][id] = genHuffman(Jbin, &offset);
        }
    }
    else
    {
        for (int i = 0; i < num;i++)
        {
            int offset=location[i].offset+4;
            uint dcac = bytehigh4(Jbin[offset]);
            uint id = bytelow4(Jbin[offset]);
            offset++;
            Huffman[dcac][id] = genHuffman(Jbin, &offset);
        }
    }
    DHTdata->Huffman = Huffman;
    return;
    //测试 huffman
    // for (int a = 0;a<2;a++)
    //     for (int b = 0; b < 2;b++)
    //     {
    //         ushort **data = Huffman[a][b];
    //         printf("-------------\n");
    //         for (int i = 0; i < 17; i++)
    //         {
    //             printf("%d ", data[0][i]);
    //         }
    //         printf("\n");
    //         for (int i = 1; i < 17;i++)
    //         {
    //             for (int k = 0;k<=data[0][i];k++)
    //             {
    //                 printf("%d ", data[i][k]);
    //             }
    //             printf("\n");
    //         }
    // }
}

struct Color{
    uint horizontal;
    uint vertical;
    uint DC;
    uint AC;
    uint block;
    int offset;//dcform的offset
};
struct DCform
{
    int value;
    uint offset;
    uint lenth;
};

struct Packet{
    struct Color *colors;
    byte *bits;
    struct DCform **dcform;
    uint bitsize;
};
struct Packet *translate(struct JNode *SOF0, struct JNode *SOS, ushort ****Huffman, byte Jbin[], uint lenth)
{
    uint offset = SOF0->offset;
    offset += 9;//颜色分量
    struct Color *colors = (struct Color *)malloc(sizeof(struct Color) * 4);
    uint numColor = Jbin[offset];
    offset++;
    for (int i = 0; i < numColor; i++)
    {
        byte id = Jbin[offset];
        offset++;
        colors[id].horizontal = bytehigh4(Jbin[offset]);
        colors[id].vertical = bytelow4(Jbin[offset]);
        colors[id].block = bytelow4(Jbin[offset]) * bytehigh4(Jbin[offset]);
        offset += 2;
    }

    //颜色分量
    // for(int i=1;i<4;i++)
    // {
    //     printf("%d:%d:%d\n", i, colors[i].horizontal, colors[i].vertical);
    // }


    offset = SOS->offset;
    offset += 5;
    for (int i = 0; i < numColor; i++)
    {
        byte id = Jbin[offset];
        offset++;
        colors[id].DC = bytehigh4(Jbin[offset]);
        colors[id].AC = bytelow4(Jbin[offset]);
        colors[id].offset = 0;//分别存储dc系数表的offset
        offset++;
    }

    // 颜色分量对应哈夫曼表
    // for(int i=1;i<4;i++)
    // {
    //     printf("%d:%d:%d\n", i, colors[i].DC, colors[i].AC);
    // }


    offset = SOS->offset;
    offset += Big2Little16(Jbin + offset + 2) + 2;

    //以位存储
    byte *bits = (byte*)malloc(sizeof(byte)*MAXSIZE * 8);
    uint bitsize = 0;

    for (int i = offset, k = 0; i < lenth - 2; i++, k++)
    {
        byte temp = Jbin[i];
        for (int j = 7; j >= 0; j--)
        {
            bits[k*8+j] = temp&1;
            temp = temp >> 1;
            bitsize++;
        }
        if(Jbin[i]==0xff)
        {
            i++;
        }
    }


    struct DCform **dcform = (struct DCform **)malloc(sizeof(struct DCform *)*4);
    dcform[1] = (struct DCform *)malloc(sizeof(struct DCform) * MAXSIZE / 2);
    dcform[2] = (struct DCform *)malloc(sizeof(struct DCform) * MAXSIZE / 2);
    dcform[3] = (struct DCform *)malloc(sizeof(struct DCform) * MAXSIZE / 2);

    //译码
    for (int k = 0; k < bitsize;)
    {
        for(int a=1;a<4;a++)//颜色分量
        {
            ushort **dc = Huffman[0][colors[a].DC];
            ushort **ac = Huffman[1][colors[a].AC];
            for (int i = 0; i < colors[a].block; i++)
            {

                //block

                //dc
                //int temp = k;
                ushort code = hufDecode(&k, dc, bits); //移动k
                if(k>=bitsize)
                    goto BR;
                if (code == 0xff)
                {
                    printf("DC\n");
                    printf("hufdecode failed\n");
                    printf("k=%d,bitsize=%d", k, bitsize);
                    return NULL;
                }

                int    dcNum    = numDecode(k,code, bits);
                int coloroffset = colors[a].offset;
                dcform[a][coloroffset].value  = dcNum;
                dcform[a][coloroffset].lenth  = code;
                dcform[a][coloroffset].offset = k;
                colors[a].offset++;

                k = k + code;

                //ac ac 译码 -> 跳过
                for(int m=1;m<64;m++)//dc占一位
                {
                    code = hufDecode(&k, ac, bits);

                    if(code == 0xff)
                    {
                        printf("AC\n");
                        printf("hufdecode failed\n");
                        printf("k=%d,bitsize=%d\n", k, bitsize);
                        return NULL;
                    }
                    if(code==0)
                    {
                        break;
                    }
                    m+=bytehigh4((byte)code);//0的个数
                    k+=bytelow4((byte)code);//k跳过ac系数  
                }
            }

        }
    }
    BR:
    //dc系数 diff
    for (int a = 1; a < numColor+1;a++)
    {
        for (int i = 1; i < colors[a].offset; i++)
        {
            dcform[a][i].value += dcform[a][i - 1].value;
        }
    }
    struct Packet *temp = (struct Packet *)malloc(sizeof(struct Packet));
    temp->bits = bits;
    temp->dcform =dcform;
    temp->colors = colors;
    temp->bitsize = bitsize;
    return temp;
}

byte * encrypt(struct Packet *Info, uint *eSize)
{
    byte *bits = Info->bits;
    struct DCform **dcform = Info->dcform;
    struct Color *colors = Info->colors;
    uint bitsize = Info->bitsize;


    //初始化SBOX
    byte s[256] = {0}; //S-box

    rc4_init(s, (unsigned char *)key, strlen(key));
    //s盒->Sboxbits
    byte *Sboxbits = (byte *)malloc(sizeof(byte) * SBOXSIZE);
    for (int i = 0, k = 0; i < 256; i++, k++)
    {
        byte temp = s[i];
        for (int j = 7; j >= 0; j--)
        {
            Sboxbits[k*8+j] = temp&1;
            temp = temp >> 1;
        }
    }

    for (int a = 1; a < 4;a++)
    {
        int sizeDC =colors[a].offset;
        for (int i = 0,j=0; i < sizeDC;i++)
        {
            uint Boffset = dcform[a][i].offset;
            uint codeLenth = dcform[a][i].lenth;
            xorC(bits, Sboxbits, Boffset, j, codeLenth);
            j = (j + codeLenth) % SBOXSIZE;
        }
    }

    byte *encryptData = (byte *)malloc(sizeof(byte) * MAXSIZE);
    for (int i = 0, k = 0; k < bitsize/8; i++, k++)
    {
        byte temp = 0;
        for (int j = 0; j < 8; j++)
        {
            temp = temp * 2 + bits[k * 8 + j];
        }
        encryptData[i] = temp;
        if(encryptData[i]==0xff)
        {
            i++;
            encryptData[i] = 0;
        }
        *eSize = i + 1;
    }

    return encryptData;
}

void writeJpeg(char *filePath,byte Jbin[],uint lenth, byte * encryptData,uint eSize,struct JNode *SOS)
{
    uint offset = SOS->offset;
    offset += Big2Little16(Jbin + offset + 2) + 2;
    int i=offset;
    int j = 0;
    for (; j < eSize; i++, j++)
    {
        Jbin[i] = encryptData[j];
    }
    Jbin[i]=0xff;
    Jbin[i + 1] = 0xd9;


    FILE *fout = fopen(filePath, "wb+");
    if(fout==NULL)
    {
        printf("FILE failde to open\n");
        exit(EXIT_FAILURE);
    }
    fwrite(Jbin, sizeof(byte), i + 2, fout);
    fclose(fout);
    return;
}


void generateDecodeInfo(struct Packet *Info,ushort ****Huffman)
{
    FILE *fout = fopen("Decode.txt", "wb+");
    struct Color *colors = Info->colors;
    char *ColorCom[4] = {"0", "Y", "Cb", "Cr"};
    for (int a = 1; a < 4; a++)
    {
        fprintf(fout, "Color:%s\n", ColorCom[a]);
        fprintf(fout, " Huffman.DC.id:%d\n", colors[a].DC);
        fprintf(fout, " Huffman.AC.id:%d\n", colors[a].AC);
        fprintf(fout, " HorizontalCom:%d\n", colors[a].horizontal);
        fprintf(fout, " Vertical:%d\n", colors[a].vertical);
        fprintf(fout,"---------------\n");
    }
    fprintf(fout, "Huffman Code\n");
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2;b++)
        {
            char *classC[2] = {"DC", "AC"};
            fprintf(fout,"-------------\n");
            fprintf(fout, "ID=%d Class=%s\n", b, classC[a]);
            ushort **data = Huffman[a][b];
            for (int i = 1; i < 17;i++)
            {
                fprintf(fout,"Lenth=%d: ", i);
                for (int k = 0;k<data[0][i];k++)
                {
                    fprintf(fout,"%x ", data[i][k]);
                }
                fprintf(fout,"\n");
            }
        }

    struct DCform **dcform = Info->dcform;
    for (int a = 1;a<4;a++)
    {
        fprintf(fout,"-------------\n");
        fprintf(fout, "%s Componets DC:\n", ColorCom[a]);
        for (int i = 0; i < colors[a].offset; i++)
        {
            fprintf(fout, "%d ", dcform[a][i].value);
        }
        fprintf(fout,"-------------\n");
    }

    fclose(fout);
    return;
}

int main()
{
    char filename[30];
    byte Jbin[MAXSIZE]; //jpeg的二进制
    printf("Default File Diretory: \n");
    printf("Input the filename: such as 1.jpg\n");
    scanf("%s", filename);
    printf("Input the key\n");
    scanf("%s", key);
    char filein[60] = "";
    char fileout[60] = "";
    strcat(filein,  filename);
    strcat(fileout, filename);
    uint lenth = JpegRead(Jbin, filein);

    isJPEG(Jbin,lenth);

    //定位每个段的位置长度
    struct JNode SOF0,DHT,SOS,DQT;

    DQT.id  = 0xFFDB;
    SOF0.id = 0xFFC0;
    DHT.id  = 0xFFC4;
    SOS.id  = 0xFFDA;
    Seglocate(Jbin,lenth,&DQT);
    Seglocate(Jbin,lenth,&SOF0);
    Seglocate(Jbin,lenth,&DHT);
    Seglocate(Jbin,lenth,&SOS);


    //读取DQT量化表

    //readDQT(Jbin, &DQT);

    //获取多个DHT（如果有）
    struct DHTstruct DHTdata;
    struct JNode DHTs[4];
    DHTs[0] = DHT;
    DHTdata.num = DHTmutiLocate(Jbin, lenth, DHTs);
    DHTdata.location = DHTs;

    // for (int i = 0; i < DHTdata.num;i++)
    // {
    //     printf("%d : %x : %d\n", i, DHTdata.location[i].offset, DHTdata.location[i].lenth);
    // }

    //读取哈夫曼树
    readHuffman(Jbin,&DHTdata);

    //测试读取DHT中的Huffman
    // for (int a = 0;a<2;a++)
    //     for (int b = 0; b < 2;b++)
    //     {
    //         ushort **data = DHTdata.Huffman[a][b];
    //         printf("-------------\n");
    //         for (int i = 0; i < 17; i++)
    //         {
    //             printf("%d ", data[0][i]);
    //         }
    //         printf("\n");
    //         for (int i = 1; i < 17;i++)
    //         {
    //             for (int k = 0;k<=data[0][i];k++)
    //             {
    //                 printf("%d ", data[i][k]);
    //             }
    //             printf("\n");
    //         }
    // }
    struct Packet *Info;
    Info = translate(&SOF0, &SOS, DHTdata.Huffman, Jbin, lenth);
    //byte *bits = Info->bits;
    //struct Color *colors = Info->colors; 
    //colors[1] 1,2,3 颜色分量
    //struct DCform **dcform = Info->dcform;
    //dcform[1][0] 1->颜色分量1,2,3 || 0...colors[1].offset-1

    //加密dc系数
    uint eSize;
    byte *encryptData = encrypt(Info, &eSize);

    //写入
    writeJpeg(fileout,Jbin, lenth, encryptData,eSize,&SOS);
    printf("Generate the file\n");
    printf("Generate the HuffmanCode and DC\n");
    generateDecodeInfo(Info,DHTdata.Huffman);
    return 0;
}


uint Big2Little16(byte data[])
{
    uint rst = (data[0]<<8)+data[1];
    return rst;
}

uint bytelow4(byte b)
{
    uint rst = b & 0xf;
    return rst;
}

uint bytehigh4(byte b)
{
    uint rst = (b >> 4) & 0xf;
    return rst;
}

ushort** genHuffman(byte JbinDHT[],int *offset)
{
    int j = *offset;//移动dht的指针
    ushort **data = (ushort **)malloc(sizeof(ushort *) * 17);
    data[0]=(ushort *)malloc(sizeof(ushort)*17);//0用来保存各个码长的数量 
    data[0][0] = 0;//递推值 基址


    for (int i = 1; i < 17; i++,j++)
    {
        data[0][i] = JbinDHT[j];
        data[i] = (ushort *)malloc(sizeof(ushort)*(data[0][i]+1)); //data[i][]相当于存储基址 
    }
    for (int i = 1; i < 17; i++)
    {
        for (int k = 0; k < data[0][i];k++,j++)
        {
            data[i][k] = JbinDHT[j];
        }
        ushort lastbase = data[0][i - 1];
        ushort base = data[0][i];
        data[i][base] = (data[i - 1][lastbase] + lastbase) * 2;
    }

    //访问huffman树 j=数-data[i-1][data[0][i-1]]<[data[0][i-1] =>[码长i][j]

    // printf("-------------\n");
    // for (int i = 0; i < 17; i++)
    // {
    //     printf("%d ", data[0][i]);
    // }
    // printf("\n");
    // for (int i = 1; i < 17;i++)
    // {
    //     for (int k = 0;k<=data[0][i];k++)
    //     {
    //         printf("%d ", data[i][k]);
    //     }
    //     printf("\n");
    // }


    *offset = j;
    return data;
}





ushort hufDecode(int *k, ushort **code,byte bits[])
{
    int offset = *k;
    uint temp = 0;
    for (int i = 1; i < 17; i++)
    {
        temp = temp * 2 + bits[offset + i-1];
        int nodes = code[0][i];
        if (nodes == 0)
            continue;
        else
        {
            int index = temp - code[i][nodes];
            if(index<0||index>=nodes)
            {
                continue;
            }
            else
            {
                *k = offset + i;
                return code[i][index];
            }
        }
    }
    return 0xff;//error
}

int numDecode(int k, ushort code,byte bits[])
{
    int temp = 0;
    int limit = 1 << (code- 1) ;  //2^^k -1
    for (int i = 0; i < code; i++)
    {
        temp = temp * 2 + bits[i+k];
    }
    if(temp < limit)
    {
        temp = temp - (1<<code) + 1;
    }
    return temp;
}

void xorC (byte  bits[], byte Sboxbits[], uint Boffset, int j, uint codeLenth)
{
for (int i = 0; i < codeLenth;i++)
{
bits[Boffset + i] = bits[Boffset + i] ^ Sboxbits[(j + i) % SBOXSIZE];
}
}
/*
1.超过2m会直接段错误？ 使用malloc
*/