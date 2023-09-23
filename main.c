#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <ncurses.h>

#define     DEBUG 1u
#define     BUFF_SIZE 256
#define     MAX_CORES_NUM 12

 typedef enum{
    INIT,
    WORK,
} prog_state_t;

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
} cpu_stats_raw_t;

typedef struct cpu_stats_calc{
    float idlePrev;
    float idleCurr;
    float nonIdlePrev;
    float nonIdleCurr;
    float totalPrev;
    float totalCurr;
    float totalCalc;
    float idleCalc;
    uint8_t cpuPercentage;

} cpu_stats_calc_t;

typedef struct cpu_stats_object{
    cpu_stats_raw_t cpuStats_raw;
    cpu_stats_raw_t cpuStats_prev;
    cpu_stats_calc_t cpuStats_view;
} cpu_stats_object_t;

prog_state_t program_state = INIT;

static void error_handler(void){
    // temporary error handler for debug purposes
    perror("");
    exit(errno);
}

static void load_cpuStats(cpu_stats_object_t * pCpuStats, uint8_t * pCpuNum){
    char* ret = {0};
    FILE *fp = {0};
    cpu_stats_raw_t *pCpuRaw = {0};
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
        uint8_t cpu_number = *pCpuNum;

        if(program_state == INIT){
            *pCpuNum = 0;
        }

        for(int i = 0; i < cpu_number; i ++){
            ret = fgets(buffer, sizeof(buffer) - 1, fp);
            if (ret == NULL) {
                error_handler();
            }

            ret = strncmp(buffer, "cpu", 3) == 0 ? strncpy(buffer_arr[i], buffer, sizeof(buffer) - 1) : 0;
            if (ret == 0) {
                break;
            }
            if(program_state == INIT){
                *pCpuNum+=1;
            }
        }
        fclose(fp);

        if(program_state == INIT){
            return;
        }

        for(int i = 0; i < *pCpuNum; i++){
            pCpuRaw = &pCpuStats[i].cpuStats_raw;

            sscanf(buffer_arr[i] + 5,
                   " %u %u %u %u %u %u %u %u %u %u",
                   &pCpuRaw->user, &pCpuRaw->nice, &pCpuRaw->system, &pCpuRaw->idle,
                   &pCpuRaw->ioWait, &pCpuRaw->irq, &pCpuRaw->softIrq, &pCpuRaw->steal,
                   &pCpuRaw->guest, &pCpuRaw->guestNice);
        }
    }
    return;
}

static void calculate_cpuStats(cpu_stats_object_t * pCpu,
                               uint8_t * pCpuNum){
    cpu_stats_raw_t  * pCpuRaw = {0};
    cpu_stats_raw_t * pCpuPrev = {0};
    cpu_stats_calc_t * pCpuCalc = {0};

        for(int i = 0; i < *pCpuNum; i++) {

            pCpuRaw = &pCpu[0].cpuStats_raw;
            pCpuPrev = &pCpu[0].cpuStats_prev;

            if(program_state != INIT) {
                pCpuCalc = &pCpu[0].cpuStats_view;

                pCpuCalc->idlePrev = pCpuPrev->idle + pCpuPrev->ioWait;
                pCpuCalc->idleCurr = pCpuRaw->idle + pCpuRaw->ioWait;

                pCpuCalc->nonIdlePrev = pCpuPrev->user + pCpuPrev->nice + pCpuPrev->system +
                                        pCpuPrev->irq + pCpuPrev->softIrq + pCpuPrev->steal;

                pCpuCalc->nonIdleCurr = pCpuRaw->user + pCpuRaw->nice + pCpuRaw->system + pCpuRaw->irq +
                                        pCpuRaw->softIrq + pCpuRaw->steal;

                pCpuCalc->totalPrev = pCpuCalc->idlePrev + pCpuCalc->nonIdlePrev;
                pCpuCalc->totalCurr = pCpuCalc->idleCurr + pCpuCalc->nonIdleCurr;

                pCpuCalc->totalCalc = pCpuCalc->totalCurr - pCpuCalc->totalPrev;
                pCpuCalc->idleCalc = pCpuCalc->idleCurr - pCpuCalc->idlePrev;
                pCpuCalc->cpuPercentage = (uint8_t)roundf((pCpuCalc->totalCalc - pCpuCalc->idleCalc)
                                            / pCpuCalc->totalCalc) * 100;

            } else{
                program_state = WORK;
            }
            memccpy(pCpuPrev, pCpuRaw, sizeof(cpu_stats_raw_t), sizeof(cpu_stats_raw_t));
    }
}

static void print_cpuStats(cpu_stats_object_t * pCpuStats,
                           uint8_t * pCpuNum){
    int ret = {0};
    printw("%-10s %-10s %-10s %-10s\n", "<CPU>", "TOTAL[%]", "IDLE", "NONIDLE");
    cpu_stats_calc_t *  pCpuCalc = {0};
    pCpuCalc = &pCpuStats[0].cpuStats_view;

    printw("\rcpu%-10s %-10s %-10.2f %-10.2f \n", "", &pCpuCalc->cpuPercentage, &pCpuCalc->idleCalc,
           &pCpuCalc->nonIdleCurr);


    for(uint8_t i=1; i <  *pCpuNum; i++) {
        pCpuCalc = &pCpuStats[i].cpuStats_view;
        printw("\rcpu%-10u %-10s %-10.2f %-10.2f \n", i, &pCpuCalc->cpuPercentage, &pCpuCalc->idleCalc,
               &pCpuCalc->nonIdleCurr);

    }

    move(0,0);
    refresh();
}
int main(void) {
    errno = 0;
    uint8_t  cpu_num = MAX_CORES_NUM;
    cpu_stats_object_t *  CpuStats = {0};

    initscr();
    for(;;){
        load_cpuStats(CpuStats, &cpu_num);
        if(program_state == INIT){
            CpuStats = (cpu_stats_object_t *)malloc(sizeof(cpu_stats_object_t) * cpu_num);
        }
        calculate_cpuStats(CpuStats, &cpu_num);
        print_cpuStats(CpuStats, &cpu_num);

        sleep(1);

    }
    endwin();
    return 0;
}
