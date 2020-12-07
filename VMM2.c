
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <alloca.h>

#define FRAME_SIZE 256        // 帧大小
#define TOTAL_NUMBER_OF_FRAMES 128  // 物理内存中的帧总数
#define ADDRESS_MASK  0xFFFF  //物理内存地址
#define OFFSET_MASK  0xFF //页偏移
#define TLB_SIZE 16       // TLB条目
#define PAGE_TABLE_SIZE 256  // 页面大小

int pageavailable[PAGE_TABLE_SIZE] = { 0 };//是否可用
int pageTableNumbers[128];  // 保存页表中的可用页码的使用顺序
int pageTableFrames[PAGE_TABLE_SIZE];   // 保存页表中的帧号

int TLB_page_num[TLB_SIZE];  // 保存TLB中的页码
int TLB_frame_num[TLB_SIZE]; // 保存TLB中的帧号

int physicalMemory[TOTAL_NUMBER_OF_FRAMES][FRAME_SIZE]; // 物理存储器二维数组

int pageFaults = 0;   // 记录页面错误次数
int TLBHits = 0;      // 记录TLB命中
int firstAvailableFrame = 0;  // 记录第一个可用帧

int TLB_ENTRIES = 0;             // 记录TLB中的条目数

// 从输入文件中为每行读取的字符数
#define BUFFER_SIZE         10

// 要读取的字节数
#define CHUNK               256

// 输入文件和后备存储器
FILE* address_file;
FILE* backing_store;

// 存储从输入文件读取的数据
char    address[BUFFER_SIZE];
int     logical_address;

//从后备存储器读取数据的缓冲区
signed char     buffer[CHUNK];

// 内存中字节(带符号字符)的值
signed char     value;
int num = 0;
// 函数申明


void update_tlb_lru(int pageNumber, int frameNumber);
void updateTLBLRU(int pageNumber, int frameNumber);
void read_Store_LRU(int pageNumber);
void updatepage(int pageNumber);

// FIFO页置换获取逻辑地址并获得物理地址和存储在该地址上的值
void getPageFIFO(int logical_address);
void read_Store_FIFO(int pageNumber);
//对TLB更新

//如果删除的物理内存第一个在TLB中含有则一并删除
void deleteTLBFIFO(int pageNumber);
void into_TLB_FIFO(int pageNumber, int frameNumber);

// LRU获取逻辑地址并获得物理地址和存储在该地址上的值
void update_LRU(int logical_address) {

	// 从逻辑地址获取页码和偏移量
	int pageNumber = ((logical_address & ADDRESS_MASK) >> 8);//页码
	int offset = (logical_address & OFFSET_MASK);//页偏移

	// 尝试从TLB获取页面
	int frameNumber = -1; // 初始化为-1，以判断它在下面的条件语句中是否有效
	int i;  // 通过TLB寻找匹配
	for (i = 0; i < TLB_SIZE; i++) {
		if (TLB_page_num[i] == pageNumber) {   // 如果TLB索引等于页号
			frameNumber = TLB_frame_num[i];  // 提取帧号
			TLBHits++;                // TLBHit计数器增加
			updatepage(pageNumber);//更新pageTableNumbers
			break;
		}
	}

	// 如果没有找到frameNumber
	if (frameNumber == -1) {
		if (pageavailable[pageNumber] == 1) {
			frameNumber = pageTableFrames[pageNumber];          // 从中提取frameNumber
			updatepage(pageNumber);//更新pageTableNumbers
		}
		else
		{                     // 如果在页表中找不到该页面
			read_Store_LRU(pageNumber);             // 页面错误，调用readFromStore将帧放入物理内存和页表中
			pageFaults++;                          // 增加页面错误的数量
			frameNumber = pageTableFrames[pageNumber];  // 并将frameNumber设置为当前firstAvailableFrame索引
		}

	}

	update_tlb_lru(pageNumber, frameNumber);  // 调用函数将页码和帧号插入到TLB中

	value = physicalMemory[frameNumber][offset];  // 用于获取存储在该地址的有符号值的帧编号和偏移量


	printf("Virtual address: %d Physical address: %d Value: %d \n", logical_address, (frameNumber << 8) | offset, value);
}
//更新页表顺序
void updatepage(int pageNumber) {
	int i;
	for (i = 0; i < firstAvailableFrame; i++) {
		if (pageTableNumbers[i] == pageNumber) {
			break;
		}
	}
	if (i != firstAvailableFrame) {
		for (; i < firstAvailableFrame - 1; i++) {
			pageTableNumbers[i] = pageTableNumbers[i + 1];
		}
		pageTableNumbers[i] = pageNumber;
	}

}
// 使用LRU替换将页码和帧号插入TLB中
void update_tlb_lru(int pageNumber, int frameNumber) {

	int i;  // 如果它已经在TLB中，则中断
	for (i = 0; i < TLB_ENTRIES; i++) {
		if (TLB_page_num[i] == pageNumber) {
			break;
		}
	}
	// 如果条目的数量等于索引即不再TLB中
	if (i == TLB_ENTRIES) {
		if (TLB_ENTRIES < TLB_SIZE) {  // TLB还有空间
			TLB_page_num[TLB_ENTRIES] = pageNumber;    // 在末尾插入页面和帧
			TLB_frame_num[TLB_ENTRIES] = frameNumber;
			TLB_ENTRIES++;// 增加条目的数量
		}
		else {                                            // 否则将TLB向前推进，即删除第一个其他前移
			for (i = 0; i < TLB_ENTRIES - 1; i++) {
				TLB_page_num[i] = TLB_page_num[i + 1];
				TLB_frame_num[i] = TLB_frame_num[i + 1];
			}
			TLB_page_num[TLB_ENTRIES - 1] = pageNumber;  // 然后在末尾插入页面和帧
			TLB_frame_num[TLB_ENTRIES - 1] = frameNumber;
		}
	}
	else {
		for (i = i; i < TLB_ENTRIES - 1; i++) {
			TLB_page_num[i] = TLB_page_num[i + 1];      // 移动数组中的所有内容
			TLB_frame_num[i] = TLB_frame_num[i + 1];
		}

		TLB_page_num[TLB_ENTRIES - 1] = pageNumber;
		TLB_frame_num[TLB_ENTRIES - 1] = frameNumber;

	}
}

// 从后备存储器读取数据并将帧带入物理内存和页表LRU
void read_Store_LRU(int pageNumber) {


	// 首先在后备存储器中查找字节块
	// SEEK_SET文件开头   在fseek()中，从文件的开头查找
	if (fseek(backing_store, pageNumber * CHUNK, SEEK_SET) != 0) {
		fprintf(stderr, "Error seeking in backing store\n");
	}

	// 现在从后备存储器读取块字节到缓冲区
	if (fread(buffer, sizeof(signed char), CHUNK, backing_store) == 0) {
		fprintf(stderr, "Error reading from backing store\n");
	}

	// 将这些位加载到物理内存二维数组的第一个可用帧中
	int i;
	int memory[FRAME_SIZE];
	for (i = 0; i < CHUNK; i++) {//将读取到的加入物理内存最后
		memory[i] = buffer[i];
	}
	//物理内存还有空间
	if (firstAvailableFrame < TOTAL_NUMBER_OF_FRAMES) {
		// 将帧号加载到第一个可用帧中的页表中
		pageTableNumbers[firstAvailableFrame] = pageNumber;
		pageTableFrames[pageNumber] = firstAvailableFrame;
		pageavailable[pageNumber] = 1;
		for (i = 0; i < CHUNK; i++) {//将读取到的加入物理内存最后
			physicalMemory[firstAvailableFrame][i] = memory[i];
		}
		firstAvailableFrame++;

	}
	// 将帧号改为移除页的帧
	else {

		pageavailable[pageNumber] = 1;
		pageTableFrames[pageNumber] = pageTableFrames[pageTableNumbers[0]];
		deleteTLBFIFO(pageTableNumbers[0]);
		pageavailable[pageTableNumbers[0]] = 0;//将移除页设置为不可得
		for (int k = 0; k < CHUNK; k++) {//更新物理内存 
			//将获取的数据放入移除页对应帧的位置
			physicalMemory[pageTableFrames[pageTableNumbers[0]]][k] = memory[k];
		for (i = 0; i < 128 - 1; i++) {
			pageTableNumbers[i] = pageTableNumbers[i + 1];
		}
		pageTableNumbers[127] = pageNumber;
	}
}


//物理内存帧上移，对TLB中对应页的帧更新
void updateTLBLRU(int pageNumber, int frameNumber) {
	for (int i = 0; i < TLB_SIZE; i++) {
		if (TLB_page_num[i] == pageNumber) {
			TLB_frame_num[i]--;
			break;
		}
	}
}

//读取页FIFO页置换
void getPageFIFO(int logical_address) {
	// 从逻辑地址获取页码和偏移量
	int pageNumber = ((logical_address & ADDRESS_MASK) >> 8);//页码
	int offset = (logical_address & OFFSET_MASK);//页偏移
	int frameNumber = -1; // 初始化为-1，以判断它在下面的条件语句中是否有效
	int i;  //TLB中寻找
	for (i = 0; i < TLB_ENTRIES; i++) {
		if (TLB_page_num[i] == pageNumber) {   // 如果TLB索引等于页号
			frameNumber = TLB_frame_num[i];  // 提取帧号
			TLBHits++;                // TLBHit计数器增加
			break;
		}
	}

	// 如果没有找到frameNumber
	if (frameNumber == -1) {
		if (pageavailable[pageNumber] == 1) {
			frameNumber = pageTableFrames[pageNumber];          // 数组中提取frameNumber
		}
		// 如果在页表中找不到该页面
		else
		{
			read_Store_FIFO(pageNumber);             // 页面错误，调用readFromStore将帧放入物理内存和页表中
			pageFaults++;                          // 增加页面错误的数量
			frameNumber = pageTableFrames[pageNumber];  // 并将frameNumber设置为当前firstAvailableFrame索引
		}

	}
	into_TLB_FIFO(pageNumber, frameNumber);  // 调用函数将页码和帧号插入到TLB中
	value = physicalMemory[frameNumber][offset];  // 用于获取存储在该地址的有符号值的帧编号和偏移量

	int k = 0;
	int num = 0;
	for (k = 0; k < 256; k++) {
		if (pageavailable[k] > 0) {
			num++;
		}
	}

	//printf("Virtual address: %d Physical address: %d Value: %d \n", logical_address, (frameNumber << 8) | offset, value);
}

// 从后备存储器读取数据并将帧带入物理内存和页表FIFO
void read_Store_FIFO(int pageNumber) {


	// 首先在后备存储器中查找字节块
	// SEEK_SET文件开头   在fseek()中，从文件的开头查找
	if (fseek(backing_store, pageNumber * CHUNK, SEEK_SET) != 0) {
		fprintf(stderr, "Error seeking in backing store\n");
	}

	if (fread(buffer, sizeof(signed char), CHUNK, backing_store) == 0) {
		fprintf(stderr, "Error reading from backing store\n");
	}

	int i;
	int memory[FRAME_SIZE];
	for (i = 0; i < CHUNK; i++) {//将读取到的加入物理内存最后
		memory[i] = buffer[i];
	}
	//物理内存还有空间
	if (firstAvailableFrame < TOTAL_NUMBER_OF_FRAMES) {
		// 将帧号加载到第一个可用帧中的页表中
		pageTableNumbers[firstAvailableFrame] = pageNumber;
		pageTableFrames[pageNumber] = firstAvailableFrame;
		pageavailable[pageNumber] = 1;
		for (i = 0; i < CHUNK; i++) {//将读取到的加入物理内存最后
			physicalMemory[firstAvailableFrame][i] = memory[i];
		}
		firstAvailableFrame++;
	}
	//物理内存已满
	else {

		pageavailable[pageNumber] = 1;
		pageTableFrames[pageNumber] = pageTableFrames[pageTableNumbers[0]];	// 将帧号改为移除页的帧
		for (int k = 0; k < CHUNK; k++) {//更新物理内存 
				//将获取的数据放入移除页对应帧的位置
			physicalMemory[pageTableFrames[pageTableNumbers[i]]][k] = memory[k];
		}
		deleteTLBFIFO(pageTableNumbers[0]);

		pageavailable[pageTableNumbers[0]] = 0;//将移除页设置为不可得

		for (i = 0; i < 128 - 1; i++) {
			pageTableNumbers[i] = pageTableNumbers[i + 1];
		}
		pageTableNumbers[i] = pageNumber;

	}
}

// 使用FIFO替换将页码和帧号插入TLB中
void into_TLB_FIFO(int pageNumber, int frameNumber) {

	int i;  // 如果它已经在TLB中，则中断
	for (i = 0; i < TLB_ENTRIES; i++) {
		if (TLB_page_num[i] == pageNumber) {
			break;
		}
	}

	// 如果条目的数量等于索引即不再TLB中
	if (i == TLB_ENTRIES) {
		if (TLB_ENTRIES < TLB_SIZE) {  // TLB还有空间
			TLB_page_num[TLB_ENTRIES] = pageNumber;    // 在末尾插入页面和帧
			TLB_frame_num[TLB_ENTRIES] = frameNumber;
			TLB_ENTRIES++;
		}
		else {                                            // 否则将TLB向前推进，即删除第一个其他前移
			for (i = 0; i < TLB_ENTRIES - 1; i++) {
				TLB_page_num[i] = TLB_page_num[i + 1];
				TLB_frame_num[i] = TLB_frame_num[i + 1];
			}
			TLB_page_num[TLB_ENTRIES - 1] = pageNumber;  // 然后在末尾插入页面和帧
			TLB_frame_num[TLB_ENTRIES - 1] = frameNumber;
		}
	}


}

//如果删除的物理内存第一个在TLB中含有则一并删除
void deleteTLBFIFO(int pageNumber) {
	int i = 0;
	for (i = 0; i < TLB_ENTRIES; i++) {
		if (TLB_page_num[i] == pageNumber) {
			break;
		}
	}
	if (i != TLB_ENTRIES) {
		for (; i < TLB_ENTRIES - 1; i++) {
			TLB_page_num[i] = TLB_page_num[i + 1];
			TLB_frame_num[i] = TLB_frame_num[i + 1];
		}
		TLB_ENTRIES--;
	}
}



// 打开必要的文件，并对地址文件中的每个条目调用getPage
int main(int argc, char* argv[])
{

	// 执行基本错误检查 输入格式 ./a.out addresses.txt
	if (argc != 2) {
		fprintf(stderr, "Usage: ./a.out [input file]\n");
		return -1;
	}

	// 打开包含备份存储的文件
	backing_store = fopen("BACKING_STORE.bin", "rb");

	if (backing_store == NULL) {
		fprintf(stderr, "Error opening BACKING_STORE.bin %s\n", "BACKING_STORE.bin");
		return -1;
	}

	// 打开包含逻辑地址的文件
	address_file = fopen(argv[1], "r");

	if (address_file == NULL) {
		fprintf(stderr, "Error opening addresses.txt %s\n", argv[1]);
		return -1;
	}

	int numberOfTranslatedAddresses = 0;
	// 通读输入文件并输出每个逻辑地址
	while (fgets(address, BUFFER_SIZE, address_file) != NULL) {

		logical_address = atoi(address);

		// 获取物理地址和存储在该地址的值
	   //getPageFIFO(logical_address);
		update_LRU(logical_address);
		numberOfTranslatedAddresses++;  // 增加已翻译地址的数目        
	}

	// 计算并打印统计数据
	printf("Number of translated addresses = %d\n", numberOfTranslatedAddresses);
	float pfRate = pageFaults / (double)numberOfTranslatedAddresses;
	float TLBRate = TLBHits / (double)numberOfTranslatedAddresses;

	printf("Page Faults = %d\n", pageFaults);
	printf("Page Fault Rate = %.5f\n", pfRate);
	printf("TLB Hits = %d\n", TLBHits);
	printf("TLB Hit Rate = %.5f\n", TLBRate);

	// 关闭输入文件和备份存储
	fclose(address_file);
	fclose(backing_store);

	return 0;
}