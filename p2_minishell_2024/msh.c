//P2-SSOO-23/24

//  MSH main file
// Write your msh source code here

//#include "parser.h"
#include <stddef.h>			/* NULL */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>      // errno

#include <sys/wait.h>
#include <signal.h>

#define MAX_COMMANDS 8


// files in case of redirection
char filev[3][64];

//to store the execvp second parameter
char *argv_execvp[8];

void siginthandler(int param)
{
	printf("****  Exiting MSH **** \n");
	//signal(SIGINT, siginthandler);
	exit(0);
}

/* myhistory */

struct command
{
  // Store the number of commands in argvv
  int num_commands;
  // Store the number of arguments of each command
  int *args;
  // Store the commands
  char ***argvv;
  // Store the I/O redirection
  char filev[3][64];
  // Store if the command is executed in background or foreground
  int in_background;
};

int history_size = 20;
struct command * history;
int head = 0;
int tail = 0;
int n_elem = 0;

void free_command(struct command *cmd)
{
    if((*cmd).argvv != NULL)
    {
        char **argv;
        for (; (*cmd).argvv && *(*cmd).argvv; (*cmd).argvv++)
        {
            for (argv = *(*cmd).argvv; argv && *argv; argv++)
            {
                if(*argv){
                    free(*argv);
                    *argv = NULL;
                }
            }
        }
    }
    free((*cmd).args);
}

void store_command(char ***argvv, char filev[3][64], int in_background, struct command* cmd)
{
    int num_commands = 0;
    while(argvv[num_commands] != NULL){
        num_commands++;
    }

    for(int f=0;f < 3; f++)
    {
        if(strcmp(filev[f], "0") != 0)
        {
            strcpy((*cmd).filev[f], filev[f]);
        }
        else{
            strcpy((*cmd).filev[f], "0");
        }
    }

    (*cmd).in_background = in_background;
    (*cmd).num_commands = num_commands-1;
    (*cmd).argvv = (char ***) calloc((num_commands) ,sizeof(char **));
    (*cmd).args = (int*) calloc(num_commands , sizeof(int));

    for( int i = 0; i < num_commands; i++)
    {
        int args= 0;
        while( argvv[i][args] != NULL ){
            args++;
        }
        (*cmd).args[i] = args;
        (*cmd).argvv[i] = (char **) calloc((args+1) ,sizeof(char *));
        int j;
        for (j=0; j<args; j++)
        {
            (*cmd).argvv[i][j] = (char *)calloc(strlen(argvv[i][j]),sizeof(char));
            strcpy((*cmd).argvv[i][j], argvv[i][j] );
        }
    }
}


/**
 * Get the command with its parameters for execvp
 * Execute this instruction before run an execvp to obtain the complete command
 * @param argvv
 * @param num_command
 * @return
 */
void getCompleteCommand(char*** argvv, int num_command) {
	//reset first
	for(int j = 0; j < 8; j++)
		argv_execvp[j] = NULL;

	int i = 0;
	for ( i = 0; argvv[num_command][i] != NULL; i++)
		argv_execvp[i] = argvv[num_command][i];
}

/* Our custom functions */
void mycalc(char *argv[]);
void myhistory(char *argv[]);
void refresh_stdfd(int *std_fd_copy);

/**
 * Main sheell  Loop  
 */
int main(int argc, char* argv[])
{
	/**** Do not delete this code.****/
	int end = 0; 
	int executed_cmd_lines = -1;
	char *cmd_line = NULL;
	char *cmd_lines[10];

	if (!isatty(STDIN_FILENO)) {
		cmd_line = (char*)malloc(100);
		while (scanf(" %[^\n]", cmd_line) != EOF){
			if(strlen(cmd_line) <= 0) return 0;
			cmd_lines[end] = (char*)malloc(strlen(cmd_line)+1);
			strcpy(cmd_lines[end], cmd_line);
			end++;
			fflush (stdin);
			fflush(stdout);
		}
	}

	/*********************************/

	char ***argvv = NULL;
	int num_commands;

	history = (struct command*) malloc(history_size *sizeof(struct command));
	int run_history = 0;

	while (1) 
	{
		int status = 0;
		int command_counter = 0;
		int in_background = 0;
		signal(SIGINT, siginthandler);

		if (run_history)
        {
            run_history=0;
        }
        else {
            // Prompt 
            write(STDERR_FILENO, "MSH>>", strlen("MSH>>"));

            // Get command
            //********** DO NOT MODIFY THIS PART. IT DISTINGUISH BETWEEN NORMAL/CORRECTION MODE***************
            executed_cmd_lines++;
            if( end != 0 && executed_cmd_lines < end) {
                command_counter = read_command_correction(&argvv, filev, &in_background, cmd_lines[executed_cmd_lines]);
            }
            else if( end != 0 && executed_cmd_lines == end)
                return 0;
            else
                command_counter = read_command(&argvv, filev, &in_background); //NORMAL MODE
        }
		//************************************************************************************************


		/************************ STUDENTS CODE ********************************/

                    
        if (command_counter < 1) continue;          //  Guardian clause to filter out non-commands

        if (command_counter > MAX_COMMANDS) {       //  Guardian clause to filter out a command sequence with excessive number of commands
            printf("Error: Maximum number of commands is %d, you have introduced %d \n", MAX_COMMANDS, command_counter);
            continue;
        }

        // Redirections of file descriptors
        int fd, std_fd_copy[3] = {0, 0, 0};      // Array to store the main file descriptors: stdin, stdout, stderr
        
        // Redirect error output if there is file
        if (strcmp(filev[STDERR_FILENO], "0") != 0) {     
            // Open file: https://stackoverflow.com/questions/59602852/permission-denied-in-open-function-in-c
            if ((fd = open(filev[STDERR_FILENO],  O_CREAT | O_RDWR | O_TRUNC , S_IRUSR | S_IWUSR)) < 0) {
                // perror("Error opening error output file");
                fprintf(stderr, "Error opening error output file: %s\n", strerror(errno));
                refresh_stdfd(std_fd_copy);
                continue;
            } else {
                std_fd_copy[STDERR_FILENO] = dup(STDERR_FILENO);                                 // Save std file descriptor
                close(STDERR_FILENO);           /* close std  */
                dup(fd);            // Redirect file descriptor to i: {0, 1, 2}. Saving it is trivial since we know it's in 0, 1 or 2
                close(fd);          /* close file */
            }
        }   

        // Redirect error output if there is file
        if (strcmp(filev[STDIN_FILENO], "0") != 0) {     // Redirect if entry of filev is not "0"
            // Open file: https://stackoverflow.com/questions/59602852/permission-denied-in-open-function-in-c
            if ((fd = open(filev[STDIN_FILENO], O_RDONLY, S_IRUSR | S_IWUSR)) < 0) {
                fprintf(stderr, "Error opening input file: %s\n", strerror(errno));
                refresh_stdfd(std_fd_copy);
                continue;
            } else {
                std_fd_copy[STDIN_FILENO] = dup(STDIN_FILENO);                                 // Save std file descriptor
                close(STDIN_FILENO);           /* close std  */
                dup(fd);            // Redirect file descriptor to i: {0, 1, 2}. Saving it is trivial since we know it's in 0, 1 or 2
                close(fd);          /* close file */
            }
        }

        // fork
        // parent
            // establish output redirection - not-last
            // execute command - not-first
            // wait for child - not-bg
            // exit
        // child
            // establish input redirection - not-first
            // continue if not-last
            // execute command

        // Execution of command or sequence of commands
        if (strcmp(argvv[0][0], "myhistory") == 0) {        // If command is myhistory
            // do myhistory
            myhistory(argvv[0]);
        }
        else if (strcmp(argvv[0][0], "mycalc") == 0) {      // If command is mycalc
            // do mycalc
            mycalc(argvv[0]);
        }
        // If command is any other than myhistory or mycalc -> then it is executed by execvp on a child process
        else {        
            // If the sequence of commands is to be executed in the background, then create a subprocess that waits for all the child process to die
            int pid = fork();

            // The process that has pid = 0 coordinates the command sequence
            if (pid == 0) {
                int i = 0, pipe_pid[2];
                while (i < command_counter - 1) {
                    // Creates two fid for an unnamed pipe. 
                    if (pipe(pipe_pid) < 0) {
                        perror("Error creating pipe. Process terminated\n");
                        refresh_stdfd(std_fd_copy);
                        continue;
                    }
                
                    // Child process
                    if ((pid = fork()) == 0) {   
                        // Output redirection
                        close(STDOUT_FILENO);
                        dup(pipe_pid[STDOUT_FILENO]);
                        close(pipe_pid[STDOUT_FILENO]);
                        close(pipe_pid[STDIN_FILENO]);

                        // EXECUTE COMMAND
                        execvp(argvv[i][0], argvv[i]);
                        exit(errno);
                    }

                    // Best to wait, in case the next process execs before the current process finishes
                    // if (wait(&status) == -1) perror("Error while waiting child process");
                    
                    // Input redirection
                    close(STDIN_FILENO);
                    dup(pipe_pid[STDIN_FILENO]);
                    close(pipe_pid[STDIN_FILENO]);
                    close(pipe_pid[STDOUT_FILENO]);

                    i++;
                }
                
                if (strcmp(filev[STDOUT_FILENO], "0") != 0) {
                    // Open file: https://stackoverflow.com/questions/59602852/permission-denied-in-open-function-in-c
                    if ((fd = open(filev[STDOUT_FILENO],  O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR)) < 0) perror("Error opening file");

                    std_fd_copy[STDOUT_FILENO] = dup(STDOUT_FILENO);                                 // Save std file descriptor
                    close(STDOUT_FILENO);           /* close std  */
                    dup(fd);            // Redirect file descriptor to i: {0, 1, 2}. Saving it is trivial since we know it's in 0, 1 or 2
                    close(fd);          /* close file */
                }
                else {
                    if (std_fd_copy[STDOUT_FILENO]) {
                        close(STDOUT_FILENO);                       /* close current */
                        dup(std_fd_copy[STDOUT_FILENO]);            // STD = fd
                        close(std_fd_copy[STDOUT_FILENO]);          /* close file */
                    }
                }
                
                if ((pid = fork()) == 0) {
                    // EXECUTE COMMAND
                    execvp(argvv[i][0], argvv[i]);
                    exit(errno);
                }
                else if (in_background) {
                    fprintf(stdout, "[%d]\n", pid);
                }
                
                if (wait(&status) == -1) perror("Error while waiting child process");
                exit(errno);
            }
            // else if (!in_background) {
            //     // Shell waits for command handler if it is not executed in the background
            //     if (wait(&status) == -1) perror("Error while waiting child process");
            // }

            if (wait(&status) == -1) perror("Error while waiting child process");
        }

        // Restore file descriptor to default ones: stdin, stdout, stderr
        for (int i = 0; i < 3; i++) {
            if (std_fd_copy[i]) {
                close(i);                       /* close current */
                dup(std_fd_copy[i]);            // STD = fd
                close(std_fd_copy[i]);          /* close file */
            }
        }

        // Print command
        // esto creo q se borra pq es apoyo visual para el desarrollo del msh
        // print_command(argvv, filev, in_background);
	}
	
	return 0;
}


/* mycalc */
void mycalc(char *argv[]) {
    
};

/* myhistory */
void myhistory(char *argv[]) {

};

/* This function */
void refresh_stdfd(int *std_fd_copy) {
    for (int i = 0; i < 3; i++) {
        if (std_fd_copy[i]) {
            close(i);                       /* close current */
            dup(std_fd_copy[i]);            // STD = fd
            close(std_fd_copy[i]);          /* close file */
        }
    }
}