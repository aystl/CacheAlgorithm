#include "common.h"

void mem_read(uintptr_t block_num, uint8_t *buf);
void mem_write(uintptr_t block_num, const uint8_t *buf);

typedef struct line {
	16 bool valid;
	17 bool dirty;
	18 uint8_t tag;
	19 uint32_t data[16];
	20
}line;

/*���ڿ��СΪ64B���������������СΪ��СΪ16��32λ�޷���������*/
/*�������Լ�д��һЩ�궨���ȫ�־�̬��������д���룬����*/
#define cache_size(1 << 14)//16KB
#define line_cnt cache_size / BLOCK_SIZE//cache������
static int grp_cnt;//cache����
static int grp_width;//�����ַ������cache���λ��
static int tag_width;//�����ַ������tag����ǣ�λ��
static int total_width = 15;//�����ַλ��
static line cache[line_cnt];//����Ǳ�ʾcache������

uint32_t cache_read(uintptr_t addr) {
	try_increase(1);
	int grp_line_cnt = line_cnt / grp_cnt;//ÿ������
	int grp = (addr >> BLOCK_WIDTH) & 0x3;//�����ַ���
	int tag = (addr >> 8) & 0x7f;//�����ַtag
	int low = grp_line_cnt * grp;
	int high = grp_line_cnt * (grp + 1);
	//printf("%d %d %d %d %d 0x%x\n",grp_line_cnt,grp,tag,low,high,addr);//����
		for (int i = low;i < high;i++)
		{
		    if (cache[i].valid == true && cache[i].tag == tag)
			    {
			        hit_increase(1);//����
			        //printf("hit\n");
				    return cache[i].data[((addr >> 2) & 0xf)];//����4�ֽ�����
			    }
		}
	//printf("miss\n");//������˵��δ����
		
		//bool spare=false;
		for (int i = low;i < high;i++)
		{
		    if (cache[i].valid == false)//�ҵ���Ӧ����һ����δʹ�õ�cache��
			    {
			        //printf("ok\n");
				    //spare=true;
				    cache[i].valid = true;//������Чλ��tag
			        cache[i].tag = tag;
			        uint8_t* buf = (uint8_t*)cache[i].data;
			        uintptr_t block_num = (addr >> BLOCK_WIDTH) & 0x1ff;
			        mem_read(block_num, buf);
			        cache[i].dirty = false;
			        return cache[i].data[((addr >> 2) & 0xf)];//����4�ֽ�����;
			    }
		}
	//������Ϊmiss��cache��Ӧ������޿���cache��
	//printf("miss\n");
	int i = rand() % grp_line_cnt;
	i = i + grp * grp_line_cnt;//����iΪ���ѡȡ��Ҫ�滻��cache�к�
	uint8_t* buf = (uint8_t*)cache[i].data;
	uintptr_t block_num = (cache[i].tag << 2) | grp;
	if (cache[i].dirty == true)//�����λΪ1��д�ڴ�
	     mem_write(block_num, buf);
	block_num = (addr >> BLOCK_WIDTH) & 0x1ff;
	mem_read(block_num, buf);
	cache[i].tag = tag;
	cache[i].dirty = false;
	return cache[i].data[((addr >> 2) & 0xf)];//����4�ֽ�����;
}

void cache_write(uintptr_t addr, uint32_t data, uint32_t wmask) {
	try_increase(1);
    int grp_line_cnt = line_cnt/grp_cnt;//ÿ������
    int grp = (addr>>BLOCK_WIDTH)&0x3;//�����ַ���
    int tag = (addr>>8)&0x7f;//�����ַtag
    int low = grp_line_cnt*grp;
    int high = grp_line_cnt*(grp+1);
    for(int i=low;i<high;i++)
    {
        if(cache[i].valid==true&&cache[i].tag==tag)
        {
            hit_increase(1);//����
            //printf("HIT\n");
            uint32_t temp=cache[i].data[((addr>>2)&0xf)];
            temp = (temp & ~wmask) | (data & wmask);//��������mem.c��mem_uncache_write��д��
            cache[i].data[((addr>>2)&0xf)]=temp;//����̫����������temp����ʹ������һЩ
            cache[i].dirty=true;
            return;
        }
    }
    //������˵��δ����
    for(int i=low;i<high;i++)
    {
        if(cache[i].valid==false)//�ҵ���Ӧ����һ����δʹ�õ�cache��
        {
           //printf("OK\n");
           cache[i].valid=true;//������Чλ��tag
           cache[i].tag=tag;
           uint8_t* buf=(uint8_t*)cache[i].data;
           uintptr_t block_num=(addr>>BLOCK_WIDTH)&0x1ff;
           mem_read(block_num,buf);
           uint32_t temp=cache[i].data[((addr>>2)&0xf)];
           temp = (temp & ~wmask) | (data & wmask);
           cache[i].data[((addr>>2)&0xf)]=temp;
           cache[i].dirty=true;
           return;
       }
   }
   //printf("MISS\n");
   //������Ϊmiss��cache��Ӧ������޿���cache��
   int i=rand()%grp_line_cnt;
   i = i+grp*grp_line_cnt;//����iΪ���ѡȡ��Ҫ�滻��cache�к�
   uint8_t* buf=(uint8_t*)cache[i].data;
   uintptr_t block_num=(cache[i].tag<<2)|grp;
   if(cache[i].dirty==true)//�����λΪ1��д�ڴ�
       mem_write(block_num,buf);
   block_num=(addr>>BLOCK_WIDTH)&0x1ff;//�ٽ������ݶ���
   mem_read(block_num,buf);
   cache[i].tag=tag;
   uint32_t temp=cache[i].data[((addr>>2)&0xf)];
   temp = (temp & ~wmask) | (data & wmask);
   cache[i].data[((addr>>2)&0xf)]=temp;
   cache[i].dirty=true;
   return;
}

void init_cache(int total_size_width, int associativity_width) {
	grp_cnt = exp2(associativity_width);
	grp_width = associativity_width;
	tag_width = total_width - grp_width - BLOCK_WIDTH;
	//��ʼ����̬����
		//printf("%d %d %d %d \n",total_width,tag_width,grp_width,line_cnt);//����
		for (int i = 0;i < line_cnt;i++)
		{
		    cache[i].valid = 0;
		    cache[i].dirty = 0;//��λ����Чλ����
		}
}
