
#include <stddef.h>			/* NULL */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

// Each command (aside of mycalc and myhistory) must be executed from a child process 
void exec_command(char ***argvv, char **filev, int *bg) {
    int pid, status;
    for (int i = 0; i < command_counter; i++) {
        if (strcmp(argvv[i][0], "myhistory") == 0) {
            // do myhistory
        }
        else if (strcmp(argvv[i][0], "mycalc") == 0) {
            // do mycalc
        }
        else {        
            pid = fork();
            // if child
            if (pid == 0) {
                // do command...
                execvp(argvv[i], filev[i]);
                exit(0);
            }
            // if parent 
            else if (!in_background) {
                // wait
                if (wait(&status) == -1) perror("Error while waiting child process");
            }
        }
    }
}