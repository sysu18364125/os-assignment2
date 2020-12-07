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

//�������
#define PAGE_SIZE 256 //ҳ���С256�ֽ�
#define PAGE_ENTRIES 256 // page table��С
#define PAGE_NUM_BITS 8 // ҳ�Ŵ�С bit
#define FRAME_SIZE 256 // Frame��С bytes.
#define FRAME_ENTRIES 256 // �����ڴ���֡��Сbytes.
#define MEM_SIZE (FRAME_SIZE * FRAME_ENTRIES) // �����ڴ�֡��С bytes
#define TLB_ENTRIES 16 // TLB��С


int virtual; 
int page_number; 
int offset; 
int physical; 
int frame_number; 
int value;
int page_table[PAGE_ENTRIES]; // ҳ��
int tlb[TLB_ENTRIES][2]; 
int tlb_front = -1; // TLB��������
int tlb_back = -1; 
char memory[MEM_SIZE]; // �����ڴ�
int mem_index = 0; //ָ���һ����֡
int updateTLBcount = 0;  //��¼TLB��PAGE��ĸ��´���

//������
int fault_counter = 0; // page faults������
int tlb_counter = 0; // TLB ���м���
int address_counter = 0; // ���ڴ��ȡaddress��
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
    
    char* in_file; //�����ļ���
    char* out_file; //����ļ���.
    char* store_file; //�����ļ���
    char* store_data; // �����ļ�����
    int store_fd; // �洢�ļ�������
    char line[8]; //��ʱ�洢�����ļ�����
    FILE* in_ptr;  //����������ļ�ָ��
    FILE* out_ptr;  

    //ҳ����TLB����ʼ����ֵ-1
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

        //��store�ļ���ͨ��map����ӳ�����ڴ�
        store_fd = open(store_file, O_RDONLY);
        store_data = mmap(0, MEM_SIZE, PROT_READ, MAP_SHARED, store_fd, 0);
       
        if (store_data == MAP_FAILED) {
            close(store_fd);
            printf("Error mmapping the backing store file!");
            exit(EXIT_FAILURE);
        }

        //ѭ����ȡÿ��input�ļ�
        while (fgets(line, sizeof(line), in_ptr)) {
            //��ȡ�����ַ
            virtual = atoi(line);
           
            address_counter++; // ��ȡ��ַ��+1 

            page_number = get_page_number(virtual);//�������ַ��ȡҳ����ƫ����
           
            offset = get_offset(virtual);
            /* ��ʹ��ҳ����TLB��Ѱ��֡��*/
            frame_number = consult_tlb(page_number);

            //������ؽ��������-1������TLB�гɹ����ҵ���ֱ����������ַ��value
            if (frame_number != -1) {
                
                //�����TLB����update (FIFO), �����ʹ�õ�TLB����ջβ(LRU��
                physical = frame_number + offset;
                value = memory[physical];
                update_tlb_LRU(page_number, frame_number);
            }
            else {
                // TLBδ���У�ֱ�Ӵ�ҳ���в���֡��
               
                frame_number = consult_page_table(page_number);


                //����ҵ���֡�ţ���δ����ҳȱʧ����
                if (frame_number != -1) {
                   
                    physical = frame_number + offset;//��������ַ��value
                    value = memory[physical];
                    
                    update_tlb(page_number, frame_number);//����TLB��
                 
                }
                else {
                    //����ҳȱʧ����BACKING_STORE��ȡҳ���ݲ���֡�Ŵ洢�������ڴ�  
                    
                    int page_address = page_number * PAGE_SIZE;

                    //����Ƿ��п�Frame 
                    if (mem_index != -1) {
                        
                        /* Store the page from store file into memory frame. */
                        memcpy(memory + mem_index, 
                               store_data + page_address, PAGE_SIZE);

                        //���������ַ
                        frame_number = mem_index;
                        physical = frame_number + offset;
                       
                        value = memory[physical];

                        //����page_table
                         //update_page_table(page_number, frame_number);
                  
                        //��������TLB
                        update_tlb(page_number, frame_number);

                        /* Increment mem_index. */
                        if (mem_index < MEM_SIZE - FRAME_SIZE) {
                            mem_index += FRAME_SIZE;
                        }
                        else {
                            //����mem����Ϊ-1����ʾ�ڴ����� 
                            mem_index = -1;
                        }
                    }
                    else {
                        //�ڴ����޿���֡������
                    }
                }
            }

            //����������ӵ�out�ļ�
            fprintf(out_ptr, "Virtual address: %d ", virtual); 
            fprintf(out_ptr, "Physical address: %d ", physical);
            fprintf(out_ptr, "Value: %d\n", value);
            printf( "Virtual address: %d ", virtual);
            printf( "Physical address: %d ", physical);
            printf( "Value: %d\n", value);
        }

        //�����������tlb��
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
    //���������ַ
    physical = get_page_number(virtual) + get_offset(virtual);
    return physical;
}

//����ҳ��
int get_page_number(int virtual) {
   
    return (virtual >> PAGE_NUM_BITS);
}

//����ƫ����offset
int get_offset(int virtual) {
    // MASK��8��������1λ��ʮ���Ʊ�ʾ��1111111
     
    int mask = 255;

    return virtual & mask;
}

//��ʼ��ҳ��
void initialize_page_table(int n) {
    for (int i = 0; i < PAGE_ENTRIES; i++) {
        page_table[i] = n;
        
    }
}

//��ʼ��TLB��
void initialize_tlb(int n) {
    for (int i = 0; i < TLB_ENTRIES; i++) {
        tlb[i][0] = -1;
        tlb[i][1] = -1;
    }
}

//��ȡһ��page_number������Ӧ��֡�š�
int consult_page_table(int page_number) {
    if (page_table[page_number][0] == -1) {
        fault_counter++;   
    }
    return page_table[page_number];
}

// ͨ��page_number����Ӧ��֡�� 
int consult_tlb(int page_number) {
    // ���page_number�ɹ��ҵ�������frame number. 
    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (tlb[i][0] == page_number) {
            /* TLB hit! */
            tlb_counter++;

            return tlb[i][1];
        }
    }
    //tlbδ����
    return -1;
}

void update_tlb(int page_number, int frame_number) {
    // ����FIFO���� 
    if (tlb_front == -1) {
        //��ʼ��
        tlb_front = 0;
        tlb_back = 0;

        //����TLB
        tlb[tlb_back][0] = page_number;
        tlb[tlb_back][1] = frame_number;
    }
    else {
            
        tlb_front = (tlb_front + 1) % TLB_ENTRIES;
        tlb_back = (tlb_back + 1) % TLB_ENTRIES;

        //����TLB. 
        tlb[tlb_back][0] = page_number;
        tlb[tlb_back][1] = frame_number;
    }  
    return;
}

void update_tlb_LRU(int page_number, int frame_number) {//����LRU���ԣ�

        if (address_counter > 16) {
            int temp1 = tlb[tlb_back][0];
            int temp2 = tlb[tlb_back][1];
            for (int i = tlb_back; i < TLB_ENTRIES - 1; i++)//��tlb_back+1������β��λ����
            {
                tlb[i][0] = tlb[i + 1][0];
                tlb[i][1] = tlb[i + 1][1];
            }
            tlb[TLB_ENTRIES - 1][0] = temp1;//��tlb_back���浽�ڶ���β��
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


  
 
      

