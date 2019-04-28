#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>

#include <pwd.h>

typedef struct _proc_info {
    char*   User;
    int     PID;
    float   CPU;
    float   MEM;
    int     VSZ;
    int     RSS;
    char    TTY;
    char*   STAT;
    char*   TIME;
    char*   START;
    char*   COMMAND;
} proc_info;

char* read_file(char* path);
proc_info* parse_status(char* status, proc_info *proc);

proc_info* parse_status(char* status, proc_info *proc) {
    //    printf("%s\n", status);

    //Get UID 
    char* uid_line = strstr(status, "Uid:");
    if(!uid_line) {
        return NULL;
    } 
    int uid;
    sscanf(uid_line, "Uid:\t%d", &uid);

    //Use UID to get username
    struct passwd *pwd;
    pwd = getpwuid(uid);
    proc->User = pwd->pw_name;

    //Get PID
    char* PID_line = strstr(status, "Pid:");
    if(!PID_line) {
        return NULL;
    } 

    int PID;
    sscanf(PID_line, "Pid:\t%d\n", &PID);
    proc->PID = PID;

    //Get VmSize
    //If VmSize can't be found just set VSZ field to 0
    char* VSZ_line = strstr(status, "VmSize:");
    if (!VSZ_line) {
        proc->VSZ = 0;
    } else {
        int VmSize;
        sscanf(VSZ_line,"VmSize:\t%d ", &VmSize);
        proc->VSZ = VmSize;
    }

    //Get RSS:
    //Set it to 0 if it can't be found
    char* RSS_line = strstr(status, "VmRSS:");
    if (!RSS_line) {
        proc->RSS = 0;
    } 
    else {
        int VmRSS;
        sscanf(RSS_line, "VmRSS:\t%d ", &VmRSS);
        proc->RSS = VmRSS;
    } 

    char *VmLck_line = strstr(status, "VmLck:");
    if(VmLck_line) {
        char VmLck;
        sscanf(VmLck_line, "VmLck:\t%c ", &VmLck);
        proc->TTY = VmLck;
    } else {
        proc->TTY = '0';
    }

    return proc;
}

//Uptime is first value in /proc/uptime 
double get_uptime() {
    char* path = "/proc/uptime";
    char *buff = read_file(path);
    
    //Convert uptime string to double
    double uptime;
    sscanf(buff, "%lf", &uptime);
    return uptime;
}

char** split(char *file) {

    //Count # of spaces in file
    int spaces = 0;
    int i = 0;
    while (i < strlen(file)) {
        if(file[i] == ' ') {
            spaces += 1;
        }
        i += 1;
    }

    //Initialize array for tokens
    char **sp = (char**) malloc(sizeof(char*) * (spaces+1));

    //Go through file one token at a time and store it in split array
    char delim[2] = " ";
    char *token = strtok(file, delim);
    sp[0] = token;

    i = 1; 
    while(token != NULL) {
        token = strtok(NULL, delim);
        sp[i] = token;
        i += 1;
    }

    return sp;
} 

int ismultithreaded(int PID) {
    char path[50];
    sprintf(path, "/proc/%d/task", PID);

    DIR *dir = opendir(path);
    if (dir == NULL)  // opendir returns NULL if couldn't open directory 
    { 
        return 0; 
    } 

    int i = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) 
        i += 1;
    
    return i > 3 ? 1 : 0;
}

proc_info* parse_statfile(char* statfile, proc_info *proc) {
    /*Get info for CPU field */
    double uptime = get_uptime();
    char** stat = split(statfile);

    int utime, stime, cutime, cstime, starttime;
    utime = atof(stat[13]);
    stime = atof(stat[14]);
    cutime = atof(stat[15]);
    cstime = atof(stat[16]);
    starttime = atof(stat[21]);
    long Hertz = sysconf(_SC_CLK_TCK);  

    int total_time = utime + stime + cutime + cstime;
    float seconds = uptime - (starttime / Hertz);

    float CPU = 100 * ((total_time / Hertz) / seconds);
    proc->CPU = CPU;

    //Fill in major code
    proc->STAT = (char*) malloc(15);
    memset(proc->STAT, '\0', 15); 

    proc->STAT[0] = stat[2][0];
    int end = 1;

    //Minor code < or N: priority
    if(stat[18][0] == '-' ){
        proc->STAT[end] = '<';
        end += 1;
    }

    else if (stat[18][0] != '0') {
        proc->STAT[end] = 'N';
        end += 1;
    }

    //Minor code L: pages locked into memory 
    //Value from status file saved in TTY field
    if(proc->TTY != '0') {
        proc->STAT[end] = 'L';
        end += 1;
    }
    proc->TTY = '?';

    //Minor code l: is multi-threaded
    if(ismultithreaded(proc->PID)){
        proc->STAT[end] = 'l';
        end += 1;
    }

    //Minor code s: session leader
    int sid;
    sscanf(stat[5], "%d", &sid);

    if(proc->PID == sid){
        proc->STAT[end] = 's';
        end += 1;
    }

    //Minor code +: foreground process group
    int tpig;
    sscanf(stat[7], "%d", &tpig);
    if(tpig == proc->PID) {
        proc->STAT[end] = '+';
        end += 1;
    } 

    proc->STAT[end] = '\0'; 

    /*CALCULATE START TIME */
    int minutes = seconds / 60;
    int hours = minutes / 60;
    
    return proc;
}

char* read_file(char* path) {

    int fd = open(path, O_RDONLY);
    int size = 1000;
    char* buff = (char*) malloc(size);
    memset(buff, '\0', size);

    int status = 1;
    int i = 0;
    while(status > 0) {
        status = read(fd, buff + i, 1);
        i += 1;

        //Resize buffer if there's not enough space
        if(i >= size) {
            char* buff2 = (char*)realloc(buff, 2*size);
            size = size * 2;
            buff = buff2;
        }
    }
    
    close(fd);
    return buff;
}

proc_info* get_mem(proc_info* proc) {
    char* buff = read_file("/proc/meminfo");
    int memTotal;
    sscanf(buff, "MemTotal:       %d kB", &memTotal);
    proc->MEM = 100 * ((float)proc->RSS / (float)memTotal);
    free(buff);

    return proc;
}

proc_info* get_command(proc_info* proc) {
    char path[50];
    sprintf(path, "/proc/%d/cmdline", proc->PID);
    char *buff = (char*) malloc(1024);
    memset(buff, '\0', 1024);

    int fd = open(path, O_RDONLY);
    
    int status = 1;
    int i = 0;
    char c;
    while (status > 0) {
        status = read(fd, &c, 1);
        if(c == '\0') {
            buff[i] = ' ';
        } else {
            buff[i] = c;
        }
        i += 1;
    }
    close(fd);

    //If there weren't any command line arguments in cmdline
    //we need to look in proc/[pid]/comm
    if(i == 1) {
        memset(buff, '\0', 1024);
        memset(path, '\0', 50);
        sprintf(path, "/proc/%d/comm", proc->PID);
        fd = open(path, O_RDONLY);
        status = 1;
        buff[0] = '[';
        i = 1;
        while (status > 0) {
            status = read(fd, &c, 1);
            if(c == '\0') {
                buff[i] = ' ';
            } else {
                buff[i] = c;
            }
            i += 1;
        }
        buff[i-2] = ']';
        buff[i-1] = '\0';
    }

    proc->COMMAND = buff;
    
    close(fd);
    return proc;
}

int main() {
    //printf("USER      PID\t%%CPU\t%%MEM\tVSZ\tRSS\tTTY\tSTAT\tSTART\tTIME\tCOMMAND\n");
    printf("%-16s%8s%8s%8s%10s%8s%8s%8s%8s%8s%10s\n", "USER", "PID", "%CPU", "%MEM", "VSZ", "RSS", "TTY", "STAT", "START", "TIME", "COMMAND");
    DIR *dir = opendir("/proc");
    struct dirent *dp;
    while((dp = readdir(dir) ) != NULL) {

        proc_info *proc = (proc_info*) malloc(sizeof(proc_info));
        char path[100];

        //Get user from status file
        sprintf(path, "/proc/%s/status", dp->d_name);
        char* status_file = read_file(path);   
        if(!status_file) 
            continue;

        proc = parse_status(status_file, proc);
        free(status_file);
        if(!proc) 
            continue;

        memset(path, '\0', 100);
        sprintf(path, "/proc/%s/stat", dp->d_name);
        char* statfile = read_file(path);                
        proc = parse_statfile(statfile, proc);
        proc = get_mem(proc);
        proc = get_command(proc);
        //printf("%s%8d%8.1f%8.1f%8d%8d\t?%8s\tSTART\tTIME%8s\n", proc->User, proc->PID, proc->CPU, proc->MEM, proc->VSZ, proc->RSS, proc->STAT, proc->COMMAND);
        printf("%-16s%8d%8.1f%8.1f%10d%8d%8s%8s%8s%8s   %s\n",proc->User, proc->PID, proc->CPU, proc->MEM, proc->VSZ, proc->RSS, "?", proc->STAT, "START", "TIME", proc->COMMAND);

        free(proc->STAT);
        free(proc->COMMAND);
        free(proc);
        free(statfile);
    }
}

