#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

//定义变量
#define PAGE_SIZE 256 //页面大小256字节
#define PAGE_ENTRIES 256 // page table大小
#define PAGE_NUM_BITS 8 // 页号大小 bit
#define FRAME_SIZE 256 // Frame大小 bytes.
#define FRAME_ENTRIES 256 // 物理内存中帧大小bytes.
#define MEM_SIZE (FRAME_SIZE * FRAME_ENTRIES) // 物理内存帧大小 bytes
#define TLB_ENTRIES 16 // TLB大小


int virtual; 
int page_number; 
int offset; 
int physical; 
int frame_number; 
int value;
int page_table[PAGE_ENTRIES]; // 页表
int tlb[TLB_ENTRIES][2]; 
int tlb_front = -1; // TLB队列索引
int tlb_back = -1; 
char memory[MEM_SIZE]; // 物理内存
int mem_index = 0; //指向第一个空帧
int updateTLBcount = 0;  //记录TLB与PAGE表的更新次数

//计数器
int fault_counter = 0; // page faults计数器
int tlb_counter = 0; // TLB 命中计数
int address_counter = 0; // 从内存读取address数
float fault_rate; 
float tlb_rate; 

/* Functions declarations. */
int get_physical(int virtual);
int get_offset(int virtual);
int get_page_number(int virtual);
void initialize_page_table(int n);
void initialize_tlb(int n);
int consult_page_table(int page_number);
int consult_tlb(int page_number);
void update_tlb(int page_number, int frame_number);
void update_tlb_LRU(int page_number, int frame_number);

int main(int argc, char *argv[]) {
    
    char* in_file; //输入文件名
    char* out_file; //输出文件名.
    char* store_file; //保存文件名
    char* store_data; // 保存文件数据
    int store_fd; // 存储文件描述符
    char line[8]; //临时存储输入文件数组
    FILE* in_ptr;  //输入与输出文件指针
    FILE* out_ptr;  

    //页表与TLB表都初始化初值-1
    initialize_page_table(-1);
    initialize_tlb(-1);

    if (argc != 4) {
        printf("Enter input, output, and store file names!");

        exit(EXIT_FAILURE);
    }
    else {
       
        in_file = argv[1];
        out_file = argv[2];
        store_file = argv[3];

        if ((in_ptr = fopen(in_file, "r")) == NULL) {
            printf("Input file could not be opened.\n");
            exit(EXIT_FAILURE);
        }

        if ((out_ptr = fopen(out_file, "a")) == NULL) {
            printf("Output file could not be opened.\n");
            exit(EXIT_FAILURE);
        }

        //打开store文件，通过map将其映射至内存
        store_fd = open(store_file, O_RDONLY);
        store_data = mmap(0, MEM_SIZE, PROT_READ, MAP_SHARED, store_fd, 0);
       
        if (store_data == MAP_FAILED) {
            close(store_fd);
            printf("Error mmapping the backing store file!");
            exit(EXIT_FAILURE);
        }

        //循环读取每行input文件
        while (fgets(line, sizeof(line), in_ptr)) {
            //读取虚拟地址
            virtual = atoi(line);
           
            address_counter++; // 读取地址数+1 

            page_number = get_page_number(virtual);//从虚拟地址获取页号与偏移量
           
            offset = get_offset(virtual);
            /* 先使用页号在TLB中寻找帧号*/
            frame_number = consult_tlb(page_number);

            //如果返回结果不等于-1，即在TLB中成功查找到，直接输出物理地址与value
            if (frame_number != -1) {
                
                //无需对TLB进行update (FIFO), 将最近使用的TLB移至栈尾(LRU）
                physical = frame_number + offset;
                value = memory[physical];
                update_tlb_LRU(page_number, frame_number);
            }
            else {
                // TLB未命中，直接从页表中查找帧号
               
                frame_number = consult_page_table(page_number);


                //如果找到了帧号，即未发生页缺失错误
                if (frame_number != -1) {
                   
                    physical = frame_number + offset;//输出物理地址与value
                    value = memory[physical];
                    
                    update_tlb(page_number, frame_number);//更新TLB表
                 
                }
                else {
                    //出现页缺失，从BACKING_STORE读取页数据并将帧号存储至物理内存  
                    
                    int page_address = page_number * PAGE_SIZE;

                    //检查是否有空Frame 
                    if (mem_index != -1) {
                        
                        /* Store the page from store file into memory frame. */
                        memcpy(memory + mem_index, 
                               store_data + page_address, PAGE_SIZE);

                        //计算物理地址
                        frame_number = mem_index;
                        physical = frame_number + offset;
                       
                        value = memory[physical];

                        //更新page_table
                         //update_page_table(page_number, frame_number);
                  
                        //正常更新TLB
                        update_tlb(page_number, frame_number);

                        /* Increment mem_index. */
                        if (mem_index < MEM_SIZE - FRAME_SIZE) {
                            mem_index += FRAME_SIZE;
                        }
                        else {
                            //设置mem索引为-1，表示内存已满 
                            mem_index = -1;
                        }
                    }
                    else {
                        //内存中无空闲帧，跳过
                    }
                }
            }

            //将输出结果添加到out文件
            fprintf(out_ptr, "Virtual address: %d ", virtual); 
            fprintf(out_ptr, "Physical address: %d ", physical);
            fprintf(out_ptr, "Value: %d\n", value);
            printf( "Virtual address: %d ", virtual);
            printf( "Physical address: %d ", physical);
            printf( "Value: %d\n", value);
        }

        //计算错误率与tlb率
        fault_rate = (float) fault_counter / (float) address_counter;
        tlb_rate = (float) tlb_counter / (float) address_counter;

        /* Print the statistics to the end of the output file. */
        fprintf(out_ptr, "Number of Translated Addresses = %d\n", address_counter); 
        fprintf(out_ptr, "Page Faults = %d\n", fault_counter);
        fprintf(out_ptr, "Page Fault Rate = %.3f\n", fault_rate);
        fprintf(out_ptr, "TLB Hits = %d\n", tlb_counter);
        fprintf(out_ptr, "TLB Hit Rate = %.3f\n", tlb_rate);
        printf( "Number of Translated Addresses = %d\n", address_counter);
        printf("Page Faults = %d\n", fault_counter);
        printf( "Page Fault Rate = %.3f\n", fault_rate);
        printf( "TLB Hits = %d\n", tlb_counter);
        printf( "TLB Hit Rate = %.3f\n", tlb_rate);
        printf("updateTLBcount=%d", updateTLBcount);

       
        fclose(in_ptr);
        fclose(out_ptr);
        close(store_fd);
    }

    return EXIT_SUCCESS;
}


int get_physical(int virtual) {
    //返回物理地址
    physical = get_page_number(virtual) + get_offset(virtual);
    return physical;
}

//计算页号
int get_page_number(int virtual) {
   
    return (virtual >> PAGE_NUM_BITS);
}

//计算偏移量offset
int get_offset(int virtual) {
    // MASK是8个二进制1位的十进制表示即1111111
     
    int mask = 255;

    return virtual & mask;
}

//初始化页表
void initialize_page_table(int n) {
    for (int i = 0; i < PAGE_ENTRIES; i++) {
        page_table[i] = n;
        
    }
}

//初始化TLB表
void initialize_tlb(int n) {
    for (int i = 0; i < TLB_ENTRIES; i++) {
        tlb[i][0] = -1;
        tlb[i][1] = -1;
    }
}

//获取一个page_number并检查对应的帧号。
int consult_page_table(int page_number) {
    if (page_table[page_number][0] == -1) {
        fault_counter++;   
    }
    return page_table[page_number];
}

// 通过page_number检查对应的帧号 
int consult_tlb(int page_number) {
    // 如果page_number成功找到，返回frame number. 
    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (tlb[i][0] == page_number) {
            /* TLB hit! */
            tlb_counter++;

            return tlb[i][1];
        }
    }
    //tlb未命中
    return -1;
}

void update_tlb(int page_number, int frame_number) {
    // 采用FIFO策略 
    if (tlb_front == -1) {
        //初始化
        tlb_front = 0;
        tlb_back = 0;

        //更新TLB
        tlb[tlb_back][0] = page_number;
        tlb[tlb_back][1] = frame_number;
    }
    else {
            
        tlb_front = (tlb_front + 1) % TLB_ENTRIES;
        tlb_back = (tlb_back + 1) % TLB_ENTRIES;

        //更新TLB. 
        tlb[tlb_back][0] = page_number;
        tlb[tlb_back][1] = frame_number;
    }  
    return;
}

void update_tlb_LRU(int page_number, int frame_number) {//采用LRU策略：

        if (address_counter > 16) {
            int temp1 = tlb[tlb_back][0];
            int temp2 = tlb[tlb_back][1];
            for (int i = tlb_back; i < TLB_ENTRIES - 1; i++)//从tlb_back+1至队列尾单位左移
            {
                tlb[i][0] = tlb[i + 1][0];
                tlb[i][1] = tlb[i + 1][1];
            }
            tlb[TLB_ENTRIES - 1][0] = temp1;//将tlb_back保存到在队列尾部
            tlb[TLB_ENTRIES - 1][1] = temp2;
            tlb_front = (tlb_front + 1) % TLB_ENTRIES;
            tlb_back = (tlb_back + 1) % TLB_ENTRIES;
            updateTLBcount++;
        }

        else { 
            tlb[tlb_back][0] = page_number;
            tlb[tlb_back][1] = frame_number;
            tlb_front = (tlb_front + 1) % TLB_ENTRIES;
            tlb_back = (tlb_back + 1) % TLB_ENTRIES;
            updateTLBcount++;
        }
    }


  
 
      

