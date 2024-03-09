#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
// #include <sys/signal.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
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
#define     LOG_DIR "logs"
#define     RE_WR_EX S_IREAD | S_IWRITE | S_IEXEC
#define     LOG_PATH_LEN 50
#define     MAX_CPU_NUM 64




static const char title[] = "                                                      __                  __            \n"
                            "  _________  __  __   __  ___________ _____ ____     / /__________ ______/ /_____  _____\n"
                            " / ___/ __ \\/ / / /  / / / / ___/ __ `/ __ `/ _ \\   / __/ ___/ __ `/ ___/ //_/ _ \\/ ___/\n"
                            "/ /__/ /_/ / /_/ /  / /_/ (__  / /_/ / /_/ /  __/  / /_/ /  / /_/ / /__/ ,< /  __/ /    \n"
                            "\\___/ .___/\\__,_/   \\__,_/____/\\__,_/\\__, /\\___/   \\__/_/   \\__,_/\\___/_/|_|\\___/_/     \n"
                            "   /_/                              /____/                                              ";

typedef enum{
    INIT,
    INIT_SUCCESS,
    INIT_FAILED,
    TERMINATING,
    INFO,
    WORK,
    LOGGER,
    READER,
    ANALYZER,
    PRINTER,
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


// function prototypes
static void update_state(prog_state_t);
static void sigTerm_handler(void);
static void error_handler(void);
static const char* get_state_char(prog_state_t current_state);
static void put_to_log(const char* msg_type, const char* desc);
static void set_path(void);
static void send_to_logger(const char* msg);
static void create_log_file(void);
static void set_session_time(void);
static uint8_t check_log_ava(void);
static void allocate_stats_buf(void);
static void load_cpuStats(void);
static void calculate_cpuStats(void);
static void print_cpuStats(void);
static void* loggerThread_func(void);
static void* readerThread_func(void);
static void* analyzerThread_func(void);
static void* printerThread_func(void);
static void set_sig_handler(void);
static void initialize_run(void);
static void run_threads(void);
static void shutting_down(void);

static pthread_mutex_t cpu_stats_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cpu_stats_cond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t current_state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t current_state_cond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t logger_wake_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t logger_wake_cond = PTHREAD_COND_INITIALIZER;


static volatile prog_state_t program_state;
static volatile sig_atomic_t isSigTerm = 0;
static uint8_t * session = NULL;
static uint8_t * log_path = NULL;
static FILE * log_file;
static cpu_stats_object_t *  CpuStats = {0};
static volatile int cpu_num = MAX_CPU_NUM;
static pthread_t log_thread, reader_thread, analyzer_thread, printer_thread ;
static volatile uint8_t logger_wake = 0;
static char logger_msg[50] = {0};


static void update_state(prog_state_t state){
        program_state = state;
        pthread_cond_broadcast(&current_state_cond);
}

static void sigTerm_handler(void){
#if DEBUG == 1u
    pthread_mutex_lock(&logger_wake_lock);
    send_to_logger("DEBUG => SIGNAL CAUGHT");
    pthread_mutex_unlock(&logger_wake_lock);
#endif
    isSigTerm = 1;
    update_state(TERMINATING);
}

static void error_handler(void){
    endwin();
    perror("");
    exit(errno);
}

const char* get_state_char(prog_state_t current_state){

    switch(current_state)
    {
        case INIT:
            return "INIT";
        case INIT_SUCCESS:
            return "INIT_SUCCESS";
        case WORK:
            return "WORK";
        case LOGGER:
            return "LOGGER";
        case READER:
            return "READER";
        case ANALYZER:
            return "ANALYZER";
        case PRINTER:
            return "PRINTER";
        case TERMINATING:
            return "TERMINATING";
        default:
            return "INFO";
    }

}


static void put_to_log(const char* msg_type, const char* desc){
    // FILE * log_file_p = fopen(log_path, "a");
    time_t utimestamp = time(NULL); 

    if((void *)log_file > NULL){
        fprintf(log_file, "%.24s => %s : %s \n\r", ctime(&utimestamp), msg_type, desc);
    } else {
        fprintf(log_file, "%.24s => %s : %s \n\r", ctime(&utimestamp), msg_type, desc);
        
    }
    fflush(log_file);
    // fclose(log_file_p);
}

static void set_path(void){
    log_path = malloc(sizeof(uint8_t) * LOG_PATH_LEN + 1);
    sprintf(log_path, "%s/LOG%s.txt", LOG_DIR, session);
}

static void send_to_logger(const char* msg){
    logger_wake = 1;
    strcpy(logger_msg, msg);
    pthread_cond_signal(&logger_wake_cond);
}

static void create_log_file(void){
    log_file = fopen(log_path, "w");
    fclose(log_file);
    log_file = fopen(log_path, "a");
    
}

static void set_session_time(void){

    time_t utimestamp = time(NULL);
    struct tm *tm = localtime(&utimestamp);
    uint8_t buffer[20] = {0};
    sprintf(buffer, "%d-%02d-%02d_%02d:%02d:%02d", tm->tm_year+1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

    session = malloc(strlen(buffer) + 1);
    strcpy((char*)session, (const char*)buffer);
}

static uint8_t check_log_ava(void){

    if(access(LOG_DIR, F_OK) != 0){
        mkdir(LOG_DIR, RE_WR_EX);
        create_log_file();
    } else {
        if(access((const char*)log_path, F_OK) != 0) {
        create_log_file(); 
    }
    }

}

static void allocate_stats_buf(void){
    CpuStats = (cpu_stats_object_t *)malloc((sizeof(cpu_stats_object_t)) * (uint8_t)cpu_num);
    memset(CpuStats, 0, (uint8_t)cpu_num * sizeof(cpu_stats_object_t));
    program_state = WORK;
}

static void load_cpuStats(void){
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

        int set_cpu_num = 0;
        int buff_cpu_size;
        if(program_state == INIT){
            buff_cpu_size = MAX_CPU_NUM;
        } else {
            buff_cpu_size = cpu_num;
        }
        uint8_t buffer_arr[buff_cpu_size][BUFF_SIZE];
        memset(buffer_arr, 0, sizeof(buffer_arr));
        uint8_t buffer[BUFF_SIZE] = {0};
        while(!isSigTerm){

            fgets((char*)buffer, sizeof(buffer), fp);

            ret = (strncmp((const char*)buffer, "cpu", 3) == 0 ? strncpy((char *)buffer_arr[set_cpu_num], (const char*)buffer, sizeof(buffer)) : 0);
            if (ret == 0) {
                break;
            } else {
                set_cpu_num++; 
            }

        }
        fclose(fp);
        if(program_state == INIT){
            cpu_num = set_cpu_num;
            return;
        }

        for(int i = 0; i < cpu_num; i++){
            pCpuRaw = &CpuStats[i].cpuStats_raw;

            sscanf((const char *)(buffer_arr[i] + 5),
                   " %u %u %u %u %u %u %u %u %u %u",
                   &pCpuRaw->user, &pCpuRaw->nice, &pCpuRaw->system, &pCpuRaw->idle,
                   &pCpuRaw->ioWait, &pCpuRaw->irq, &pCpuRaw->softIrq, &pCpuRaw->steal,
                   &pCpuRaw->guest, &pCpuRaw->guestNice);
        }
    }
    return;
}

static void calculate_cpuStats(void){
    cpu_stats_raw_t  * pCpuRaw = {0};
    cpu_stats_raw_t * pCpuPrev = {0};
    cpu_stats_calc_t * pCpuCalc = {0};

        for(int i = 0; i < cpu_num ; i++) {
            if(program_state != INIT) {
                pCpuCalc = &CpuStats[i].cpuStats_view;
                pCpuRaw = &CpuStats[i].cpuStats_raw;
                pCpuPrev = &CpuStats[i].cpuStats_prev;

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
            }
            memccpy(pCpuPrev, pCpuRaw, sizeof(cpu_stats_raw_t), sizeof(cpu_stats_raw_t));
    }
}


static void print_cpuStats(void){
    printw("%s\n", title);
    printw("--------------------------------------------------------------------------------------------\n");

    printw("%-14s %-12s %-10s %-10s\n", "<CPU>", "TOTAL[%]", "IDLE", "NONIDLE");
    cpu_stats_calc_t *  pCpuCalc = &CpuStats[0].cpuStats_view;

    printw("\rcpu%-12s %-12.2f %-10u %-10u \n", "", pCpuCalc->cpuPercentage, pCpuCalc->idleCalc,
           pCpuCalc->nonIdleCurr);

    for(int i=1; i < cpu_num; i++) {
        pCpuCalc = &CpuStats[i].cpuStats_view;
        printw("\rcpu%-12u %-12.2f %-10u %-10u \n", i, pCpuCalc->cpuPercentage, pCpuCalc->idleCalc,
               pCpuCalc->nonIdleCurr);
    }
    move(0,0);
    refresh();
}



static void* loggerThread_func(void){
    set_session_time();
    set_path();
    check_log_ava();
   
    while(1){
        pthread_mutex_lock(&logger_wake_lock);
        pthread_cond_wait(&logger_wake_cond, &logger_wake_lock);
        if(logger_wake == 1){
            put_to_log("INFO", logger_msg);
            logger_wake = 0;
        }
        pthread_mutex_unlock(&logger_wake_lock);

        // if((int)program_state <= (int)TERMINATING){
        //     put_to_log("STATUS", get_state_char(program_state));
            // if(program_state == TERMINATING){
            //     pthread_mutex_unlock(&current_state_lock);
            //     printf("terminating logger\n");
            //     pthread_exit(NULL);
            // }
        // }
        // pthread_mutex_unlock(&logger_wake_lock);
    }
    // put_to_log("STATUS", "Terminating");
    pthread_exit(NULL);
}

static void* readerThread_func(void){
    load_cpuStats();
    allocate_stats_buf();
    update_state(INIT_SUCCESS);
    sleep(1);
    update_state(ANALYZER);
    while(!isSigTerm){
        pthread_mutex_lock(&current_state_lock);
        while(program_state != READER){
            pthread_cond_wait(&current_state_cond, &current_state_lock);
        }
        sleep(1);
        load_cpuStats();
        update_state(ANALYZER);
        pthread_mutex_unlock(&current_state_lock);
            
    }
    pthread_exit(NULL);
}


static void* analyzerThread_func(void){
    while(!isSigTerm){
        pthread_mutex_lock(&current_state_lock);
        while(program_state != ANALYZER){
            pthread_cond_wait(&current_state_cond, &current_state_lock);
            // if(program_state == TERMINATING){
            //     pthread_mutex_unlock(&current_state_lock);
            //     pthread_exit(NULL);
            // }
        }
        calculate_cpuStats();
        update_state(PRINTER);
        pthread_mutex_unlock(&current_state_lock);
            
    }
    pthread_exit(NULL);
}

static void* printerThread_func(void){
    initscr();
    while(!isSigTerm){
        pthread_mutex_lock(&current_state_lock);
        while(program_state != PRINTER){
            pthread_cond_wait(&current_state_cond, &current_state_lock);
            if(program_state == TERMINATING){
                pthread_mutex_unlock(&current_state_lock);
                endwin();
                pthread_exit(NULL);
            }
        }
        print_cpuStats(); 
        update_state(READER);
        pthread_mutex_unlock(&current_state_lock);

    }
    endwin();
    pthread_exit(NULL);
}

void set_sig_handler(void)
{
        struct sigaction sigTerm_action;

        sigTerm_action.sa_flags = SA_SIGINFO; 
        sigTerm_action.sa_sigaction = (void *)sigTerm_handler;

        if (sigaction(SIGINT, &sigTerm_action, NULL) == -1) { 
            perror("sigaction");
            _exit(1);
        }

        if (sigaction(SIGTERM, &sigTerm_action, NULL) == -1) { 
            perror("sigterm");
            _exit(1);
        }
        if (sigaction(SIGQUIT, &sigTerm_action, NULL) == -1) { 
            perror("sigquit");
            _exit(1);
        }

}

static void initialize_run(void){
    pthread_mutex_lock(&logger_wake_lock);
    send_to_logger("Program started");
    pthread_mutex_unlock(&logger_wake_lock);

    pthread_mutex_lock(&current_state_lock);
    update_state(INIT);
    pthread_mutex_unlock(&current_state_lock);

}

static void run_threads(void){
    pthread_create(&reader_thread, NULL,  (void*)readerThread_func,  NULL);
    pthread_create(&analyzer_thread, NULL,(void*)analyzerThread_func, NULL);
    pthread_create(&printer_thread, NULL, (void*)printerThread_func, NULL);
    sleep(1);

}

static void shutting_down(void){
    pthread_mutex_lock(&logger_wake_lock);
    send_to_logger("Terminating");
    pthread_mutex_unlock(&logger_wake_lock);
    sleep(1);
    // pthread_join(log_thread,  NULL);
    pthread_kill(log_thread,  SIGTERM);
    pthread_kill(reader_thread,  SIGTERM);
    pthread_kill(analyzer_thread,SIGTERM);

}

int main(void) {

	set_sig_handler();
    errno = 0;

    pthread_create(&log_thread, NULL, (void*)loggerThread_func, NULL);
    sleep(1);
    initialize_run();
    
    run_threads();
    
    pthread_join(printer_thread,  NULL);
    shutting_down();
    
    free(CpuStats);
    fclose(log_file);
    free(session);
    free(log_path);
    return 0;
}
