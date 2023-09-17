#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define     DEBUG 1u
#define     BUFF_SIZE 256
#define     MAX_CORES_NUM 12

 typedef struct cpu_stats{
    uint32_t user;
    uint32_t nice;
    uint32_t system;
    uint32_t idle;
    uint32_t ioWait;
    uint32_t irq;
    uint32_t softIrq;
    uint32_t steal;
    uint32_t guest;
    uint32_t guestNice;
} cpu_stats_t;

typedef struct cpu_stats_object{
    cpu_stats_t cpuStats_raw[MAX_CORES_NUM];
    cpu_stats_t cpuStats_view[MAX_CORES_NUM];
} cpu_stats_object_t;


static void error_handler(void){
    // temporary error handler for debug purposes
    perror("");
    exit(errno);
}

static void load_procStats(cpu_stats_t * pCpuStats){
    char* ret = {0};
    FILE *fp = {0};

#if DEBUG != 1u
    fp = fopen("/proc/stat", "r");
#else
    fp = fopen("/Users/patryk/github/cut/test_path/stat", "r");
#endif

    if(fp == NULL && errno > 0){
        error_handler();
    } else{

        uint8_t buffer_arr[MAX_CORES_NUM][BUFF_SIZE] = {0};
        uint8_t buffer[BUFF_SIZE] = {0};
        uint8_t proc_counter = 0;

        for(;;){
            ret = fgets(buffer, sizeof(buffer) - 1, fp);
            if (ret == NULL) {
                error_handler();
            }
            ret = strncmp(buffer, "cpu", 3) == 0 ? strncpy(buffer_arr[proc_counter], buffer, sizeof(buffer) - 1) : 0;
            if(ret != 0) {
                proc_counter++;
            } else {
                break;
            }

        }
        fclose(fp);
        for(uint8_t i = 0; i < proc_counter; i++){
            sscanf(buffer_arr[i],
                   "cpu   %u %u %u %u %u %u %u %u %u %u",
                   &pCpuStats[i].user, &pCpuStats[i].nice, &pCpuStats[i].system, &pCpuStats[i].idle, &pCpuStats[i].ioWait,
                   &pCpuStats[i].irq, &pCpuStats[i].softIrq, &pCpuStats[i].steal, &pCpuStats[i].guest, &pCpuStats[i].guestNice);
        }
    }

}


int main(void) {
    cpu_stats_object_t CpuStats = {0};
    load_procStats(CpuStats.cpuStats_raw);


    return 0;
}
