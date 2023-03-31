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

/*由于块大小为64B，所以数组数组大小为大小为16的32位无符号数数组*/
/*此外我自己写了一些宏定义和全局静态变量方便写代码，如下*/
#define cache_size(1 << 14)//16KB
#define line_cnt cache_size / BLOCK_SIZE//cache总行数
static int grp_cnt;//cache组数
static int grp_width;//主存地址划分中cache组号位数
static int tag_width;//主存地址划分中tag（标记）位数
static int total_width = 15;//主存地址位数
static line cache[line_cnt];//这就是表示cache的数组

uint32_t cache_read(uintptr_t addr) {
	try_increase(1);
	int grp_line_cnt = line_cnt / grp_cnt;//每组行数
	int grp = (addr >> BLOCK_WIDTH) & 0x3;//主存地址组号
	int tag = (addr >> 8) & 0x7f;//主存地址tag
	int low = grp_line_cnt * grp;
	int high = grp_line_cnt * (grp + 1);
	//printf("%d %d %d %d %d 0x%x\n",grp_line_cnt,grp,tag,low,high,addr);//测试
		for (int i = low;i < high;i++)
		{
		    if (cache[i].valid == true && cache[i].tag == tag)
			    {
			        hit_increase(1);//命中
			        //printf("hit\n");
				    return cache[i].data[((addr >> 2) & 0xf)];//返回4字节数据
			    }
		}
	//printf("miss\n");//到这里说明未命中
		
		//bool spare=false;
		for (int i = low;i < high;i++)
		{
		    if (cache[i].valid == false)//找到对应组内一个暂未使用的cache行
			    {
			        //printf("ok\n");
				    //spare=true;
				    cache[i].valid = true;//设置有效位和tag
			        cache[i].tag = tag;
			        uint8_t* buf = (uint8_t*)cache[i].data;
			        uintptr_t block_num = (addr >> BLOCK_WIDTH) & 0x1ff;
			        mem_read(block_num, buf);
			        cache[i].dirty = false;
			        return cache[i].data[((addr >> 2) & 0xf)];//返回4字节数据;
			    }
		}
	//到这里为miss且cache对应组号中无空闲cache行
	//printf("miss\n");
	int i = rand() % grp_line_cnt;
	i = i + grp * grp_line_cnt;//这里i为随机选取的要替换的cache行号
	uint8_t* buf = (uint8_t*)cache[i].data;
	uintptr_t block_num = (cache[i].tag << 2) | grp;
	if (cache[i].dirty == true)//如果脏位为1则写内存
	     mem_write(block_num, buf);
	block_num = (addr >> BLOCK_WIDTH) & 0x1ff;
	mem_read(block_num, buf);
	cache[i].tag = tag;
	cache[i].dirty = false;
	return cache[i].data[((addr >> 2) & 0xf)];//返回4字节数据;
}

void cache_write(uintptr_t addr, uint32_t data, uint32_t wmask) {
	try_increase(1);
    int grp_line_cnt = line_cnt/grp_cnt;//每组行数
    int grp = (addr>>BLOCK_WIDTH)&0x3;//主存地址组号
    int tag = (addr>>8)&0x7f;//主存地址tag
    int low = grp_line_cnt*grp;
    int high = grp_line_cnt*(grp+1);
    for(int i=low;i<high;i++)
    {
        if(cache[i].valid==true&&cache[i].tag==tag)
        {
            hit_increase(1);//命中
            //printf("HIT\n");
            uint32_t temp=cache[i].data[((addr>>2)&0xf)];
            temp = (temp & ~wmask) | (data & wmask);//这里借鉴了mem.c中mem_uncache_write的写法
            cache[i].data[((addr>>2)&0xf)]=temp;//代码太长所以引入temp变量使代码简洁一些
            cache[i].dirty=true;
            return;
        }
    }
    //到这里说明未命中
    for(int i=low;i<high;i++)
    {
        if(cache[i].valid==false)//找到对应组内一个暂未使用的cache行
        {
           //printf("OK\n");
           cache[i].valid=true;//设置有效位和tag
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
   //到这里为miss且cache对应组号中无空闲cache行
   int i=rand()%grp_line_cnt;
   i = i+grp*grp_line_cnt;//这里i为随机选取的要替换的cache行号
   uint8_t* buf=(uint8_t*)cache[i].data;
   uintptr_t block_num=(cache[i].tag<<2)|grp;
   if(cache[i].dirty==true)//如果脏位为1则写内存
       mem_write(block_num,buf);
   block_num=(addr>>BLOCK_WIDTH)&0x1ff;//再将新内容读入
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
	//初始化静态变量
		//printf("%d %d %d %d \n",total_width,tag_width,grp_width,line_cnt);//测试
		for (int i = 0;i < line_cnt;i++)
		{
		    cache[i].valid = 0;
		    cache[i].dirty = 0;//脏位，有效位置零
		}
}
