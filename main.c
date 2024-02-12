#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <ncurses.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>




#define     DEBUG 1u
#define     BUFF_SIZE 256
#define     MAX_CORES_NUM 12
#define     LOG_DIR "logs"
#define     LOG_FILE "log.txt"
#define     __LOG_PATH__  "logs/log.txt"
#define     LOG_BUFF_SIZE 50
#define     RE_WR_EX S_IREAD | S_IWRITE | S_IEXEC
#define     MSG_ERR "ERROR"
#define     LOG_PATH_LEN 50

uint8_t * session = NULL;
uint8_t * log_path = NULL;

 static const char title[] = "                                                      __                  __            \n"
                            "  _________  __  __   __  ___________ _____ ____     / /__________ ______/ /_____  _____\n"
                            " / ___/ __ \\/ / / /  / / / / ___/ __ `/ __ `/ _ \\   / __/ ___/ __ `/ ___/ //_/ _ \\/ ___/\n"
                            "/ /__/ /_/ / /_/ /  / /_/ (__  / /_/ / /_/ /  __/  / /_/ /  / /_/ / /__/ ,< /  __/ /    \n"
                            "\\___/ .___/\\__,_/   \\__,_/____/\\__,_/\\__, /\\___/   \\__/_/   \\__,_/\\___/_/|_|\\___/_/     \n"
                            "   /_/                              /____/                                              ";

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
    uint32_t idlePrev;
    uint32_t idleCurr;
    uint32_t nonIdlePrev;
    uint32_t nonIdleCurr;
    uint32_t totalPrev;
    uint32_t totalCurr;
    uint32_t totalCalc;
    uint32_t idleCalc;
    double cpuPercentage;

} cpu_stats_calc_t;

typedef struct cpu_stats_object{
    cpu_stats_raw_t cpuStats_raw;
    cpu_stats_raw_t cpuStats_prev;
    cpu_stats_calc_t cpuStats_view;
} cpu_stats_object_t;

static volatile prog_state_t program_state = INIT;
static volatile sig_atomic_t isSigTerm = 0;

static void sigTerm_handler(int signum){
    if(signum > 0){
        isSigTerm = 1;
    }
}

static void error_handler(void){
    // temporary error handler for debug purposes
    endwin();
    perror("");
    exit(errno);
}

static void put_to_log(const char* msg_type, const char* desc){
    FILE * log_file_p = fopen(log_path, "a");
    time_t utimestamp = time(NULL); 

    if((void *)log_file_p > NULL){
        fprintf(log_file_p, "%.24s => %s : %s \n\r", ctime(&utimestamp), msg_type, desc);
    } else {
        fprintf(log_file_p, "%.24s => %s : %s \n\r", ctime(&utimestamp), msg_type, desc);
        
    }
    fclose(log_file_p);
}

static void set_path(){
    log_path = malloc(sizeof(uint8_t) * LOG_PATH_LEN + 1);
    sprintf(log_path, "%s/LOG%s.txt", LOG_DIR, session);
}

static void create_log_file(){
    FILE * log_file_p = fopen(log_path, "w");
    fclose(log_file_p);

}

static void set_session_time(){

    time_t utimestamp = time(NULL);
    struct tm *tm = localtime(&utimestamp);
    uint8_t buffer[20] = {0};
    sprintf(buffer, "%d-%02d-%02d_%02d:%02d:%02d", tm->tm_year+1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

    session = malloc(strlen(buffer) + 1);
    strcpy(session, buffer);
}

static uint8_t check_log_ava(){

    if(access(LOG_DIR, F_OK) != 0){
        mkdir(LOG_DIR, RE_WR_EX);
        create_log_file();
    } else {
        if(access(log_path, F_OK) != 0) {
        create_log_file(); 
    }
    }

}


static void load_cpuStats(cpu_stats_object_t * pCpuStats, volatile uint8_t * pCpuNum){
    char* ret = 0;
    cpu_stats_raw_t *pCpuRaw = {0};
    FILE *fp;

#if DEBUG != 1u
    fp = fopen("/proc/stat", "r");
#else
    fp = fopen("/Users/patryk/github/cut/test_path/stat", "r");

#endif
    if(fp < 0 && errno > 0){
        error_handler();
    } else{
        uint8_t cpu_number = *pCpuNum;
        uint8_t buffer_arr[cpu_number][BUFF_SIZE];
        memset(buffer_arr, 0, sizeof(buffer_arr));

        uint8_t buffer[BUFF_SIZE] = {0};

        if(program_state == INIT){
            *pCpuNum = 0;
        }

        for(int i = 0; i < cpu_number; i ++){
            ret = fgets(buffer, sizeof(buffer), fp);
            if (ret < 0) {
                error_handler();
            }

            ret = strncmp(buffer, "cpu", 3) == 0 ? strncpy(buffer_arr[i], buffer, sizeof(buffer)) : 0;
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
        free(buffer_arr);
    }
    return;
}

static void calculate_cpuStats(cpu_stats_object_t * pCpu, volatile uint8_t * pCpuNum){
    cpu_stats_raw_t  * pCpuRaw = {0};
    cpu_stats_raw_t * pCpuPrev = {0};
    cpu_stats_calc_t * pCpuCalc = {0};

        for(int i = 0; i < *pCpuNum ; i++) {
            if(program_state != INIT) {
                pCpuCalc = &pCpu[i].cpuStats_view;
                pCpuRaw = &pCpu[i].cpuStats_raw;
                pCpuPrev = &pCpu[i].cpuStats_prev;

                pCpuCalc->idlePrev = (pCpuPrev->idle + pCpuPrev->ioWait);
                pCpuCalc->idleCurr = (pCpuRaw->idle + pCpuRaw->ioWait);

                pCpuCalc->nonIdlePrev = (pCpuPrev->user + pCpuPrev->nice + pCpuPrev->system +
                                        pCpuPrev->irq + pCpuPrev->softIrq + pCpuPrev->steal);

                pCpuCalc->nonIdleCurr = (pCpuRaw->user + pCpuRaw->nice + pCpuRaw->system + pCpuRaw->irq +
                                        pCpuRaw->softIrq + pCpuRaw->steal);

                pCpuCalc->totalPrev = pCpuCalc->idlePrev + pCpuCalc->nonIdlePrev;
                pCpuCalc->totalCurr = pCpuCalc->idleCurr + pCpuCalc->nonIdleCurr;

                pCpuCalc->totalCalc = pCpuCalc->totalCurr - pCpuCalc->totalPrev;
                pCpuCalc->idleCalc = pCpuCalc->idleCurr - pCpuCalc->idlePrev;
                if(pCpuCalc->totalCalc != 0){
                    pCpuCalc->cpuPercentage = ((double )pCpuCalc->totalCalc - (double )pCpuCalc->idleCalc)* 100.0
                                                / (double )pCpuCalc->totalCalc;
                }
            } else{
                program_state = WORK;
            }
            memccpy(pCpuPrev, pCpuRaw, sizeof(cpu_stats_raw_t), sizeof(cpu_stats_raw_t));
    }
}

static void print_cpuStats(cpu_stats_object_t * pCpuStats, volatile uint8_t * pCpuNum){
    printw("%s\n", title);
    printw("--------------------------------------------------------------------------------------------\n");

    printw("%-14s %-12s %-10s %-10s\n", "<CPU>", "TOTAL[%]", "IDLE", "NONIDLE");
    cpu_stats_calc_t *  pCpuCalc = {0};
    pCpuCalc = &pCpuStats[0].cpuStats_view;

    printw("\rcpu%-12s %-12.2f %-10u %-10u \n", "", pCpuCalc->cpuPercentage, pCpuCalc->idleCalc,
           pCpuCalc->nonIdleCurr);

    for(uint8_t i=1; i <  *pCpuNum; i++) {
        pCpuCalc = &pCpuStats[i].cpuStats_view;
        printw("\rcpu%-12u %-12.2f %-10u %-10u \n", i, pCpuCalc->cpuPercentage, pCpuCalc->idleCalc,
               pCpuCalc->nonIdleCurr);
    }
    move(0,0);
    refresh();
}
int main(void) {
    fd_set inp; FD_ZERO(&inp); FD_SET(STDIN_FILENO,&inp); // select func

    struct sigaction sigTerm_action = {0};
    memset(&sigTerm_action, 0, sizeof(struct sigaction));
    sigTerm_action.sa_handler = sigTerm_handler;
    sigaction(SIGTERM, &sigTerm_action, NULL);
    sigaction(SIGINT, &sigTerm_action, NULL);

    errno = 0;
    volatile uint8_t cpu_num = 12u;
    cpu_stats_object_t *  CpuStats = {0};
    
    set_session_time();
    set_path();

    check_log_ava();
    put_to_log("STATUS", "initialization");
#if DEBUG == 1u
#else

    initscr();
    while(!isSigTerm){
        load_cpuStats(CpuStats, &cpu_num);
        if(errno > 0){
            error_handler();
        }
        if(program_state == INIT){
            CpuStats = (cpu_stats_object_t *)malloc((sizeof(cpu_stats_object_t)) * cpu_num);
            memset(CpuStats, 0, cpu_num * sizeof(cpu_stats_object_t));
            program_state = WORK;
        } else {

        calculate_cpuStats(CpuStats, &cpu_num);
        if(errno > 0){
            error_handler();
        }
        print_cpuStats(CpuStats, &cpu_num);
        if(errno > 0){
            error_handler();
        }
        }
        select(1,&inp,0,0,&(struct timeval){.tv_sec=1});
        //sleep(1);
    }
    
    endwin();
#endif /* if DEBUG  == 1u */
    put_to_log("STATUS", "terminating");
    free(CpuStats);
    free(session);
    free(log_path);
    return 0;
}
