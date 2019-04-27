#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    return proc;
}

//Uptime is first value in /proc/uptime 
double get_uptime() {
    char* path = "/proc/uptime";
    int fd = open(path, O_RDONLY);

    char buff[20];
    memset(buff, '\0', 20);
    int i = 0;

    //Read characters until we hit first space
    while(buff[i] != ' '){
        read(fd, buff+i, 1);
        i += 1;
    }
    buff[i] = '\0';
    double uptime;

    //Convert uptime string to double
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

proc_info* parse_statfile(char* statfile, proc_info *proc) {
    //Get info for CPU field
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

    //buff[i] = '\0';
    return buff;
}


int main() {
    printf("USER        PID\t%%CPU\t%%MEM\tVSZ\tRSS\tTTY\tSTAT\tSTART\tTIME\tCOMMAND\n");

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
        
        printf("%s\t%.1f\t%d\t%d\t%d\n", proc->User, proc->CPU, proc->PID, proc->VSZ, proc->RSS);
        printf("\n\n");
    }
}

