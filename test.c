#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>

#include <pwd.h>

typedef struct _proc_info {
    char *User;
    int PID;
    float CPU;
    float MEM;
    char *VSZ;
    char *RSS;
    char TTY;
    char *STAT;
    char *START;
    char *TIME;
    char *COMMAND;
} proc_info;

char* read_file(char *path);
proc_info* parse_status(char *status, proc_info *proc);

proc_info* parse_status(char *status, proc_info *proc) {
//    printf("%s\n", status);

    //Get UID 
    char *uid_line = strstr(status, "Uid:");
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
    char *PID_line = strstr(status, "Pid:");
    if(!PID_line) {
        return NULL;
    } 

    int PID;
    sscanf(PID_line, "Pid:\t%d\n", &PID);
    proc->PID = PID;

    //Get VmSize
    //If VmSize can't be found in /proc/[pid]/status,
    //it's probably hiding in stat. Set it to "" for now
    char *VSZ_line = strstr(status, "VmSize:");
    if (!VSZ_line) {
        proc->VSZ = "0";
    } else {
        char VmSize[25];
        sscanf(VSZ_line,"VmSize:\t%s\n", VmSize);
        proc->VSZ = VmSize;
    } 

    //Get RSS
    char *RSS_line = strstr(status, "VmRSS");
    if (!RSS_line) {
        proc->RSS = "0";
    } 
    else {
        char VmRSS[20];
        sscanf(RSS_line, "VmRSS:\t%s\n", VmRSS);
        proc->RSS = VmRSS;
    } 
    return proc;
}

char* read_file(char *path) {

    int fd = open(path, O_RDONLY);

    int size = 1000;
    char *buff = (char*) malloc(size);
    memset(buff, '\0', size);

    int status = 1;
    int i = 0;
    while(status > 0) {
        status = read(fd, buff + i, 1);
        i += 1;

        //Resize buffer if there's not enough space
        if(i >= size) {
            char *buff2 = (char*)realloc(buff, 2*size);
            size = size * 2;
            buff = buff2;
        }
    }

    buff[i] = '\0';
    return buff;
}


int main() {
    printf("USER        PID\t%%CPU\t%%MEM\tVSZ\tRSS\tTTY\tSTAT\tSTART\tTIME\tCOMMAND\n");

    DIR *dir = opendir("/proc");
    struct dirent *dp;
    while((dp = readdir(dir) ) != NULL) {
        //        printf("%s\n ", dp->d_name);

        proc_info *proc = (proc_info*) malloc(sizeof(proc_info));
        char path[100];

        //Get user from status file
        sprintf(path, "/proc/%s/status", dp->d_name);
        char *status_file = read_file(path);   

        proc = parse_status(status_file, proc);
        if(!proc) 
            continue;

        printf("%s\t%d\t%s\t%s\n", proc->User, proc->PID, proc->VSZ, proc->RSS);
        free(status_file);



    }
}

