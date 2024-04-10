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




/* Our custom functions __________________________________ */

void mycalc(char *argv[]);
void myhistory(char *argv[]);

void sigchldhandler(int param);
void bg_sigchldhandler(int param);

void restore_stdfd(int *stdfd_backup);
int open_file(int *stdfd_backup, int fileno);

/* Our custom variables __________________________________ */

int stdfd_backup[3] = {0};  // Array to store the backup of the main file descriptors: stdin, stdout, stderr. Initialized to zero
long mycalc_acc = 0;        // Accumulator for mycalc operations

/* _______________________________________________________ */

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
      
        if (command_counter < 1) continue;          //  Guardian clause to filter out empty entries

        if (command_counter > MAX_COMMANDS) {       //  Guardian clause to filter out a command sequence with excessive number of commands
            fprintf(stderr, "Error: Maximum number of commands is %d, you have introduced %d \n", MAX_COMMANDS, command_counter);
            continue;
        }

        // SIGCHLD signal handler
        signal(SIGCHLD, sigchldhandler);
        
        // Redirect error output if there is file in filev
        if (strcmp(filev[STDERR_FILENO], "0") != 0) {     
            if (open_file(stdfd_backup, STDERR_FILENO) < 0) continue;
        }   

        // Redirect input if there is file in filev
        if (strcmp(filev[STDIN_FILENO], "0") != 0) {     // Redirect if entry of filev is not "0"
            if (open_file(stdfd_backup, STDIN_FILENO) < 0) continue;
        }
        
        // Execution of command or sequence of commands
        if (strcmp(argvv[0][0], "myhistory") == 0) {        // If command is myhistory
            myhistory(argvv[0]);
        }
        else if (strcmp(argvv[0][0], "mycalc") == 0) {      // If command is mycalc
            mycalc(argvv[0]);
        }
        // If command is any other than myhistory or mycalc -> then it is executed by execvp on a child process
        else {        
            // The process that has pid = 0 coordinates the command sequence
            int pid = 0;
            if (in_background) { 
                if ((pid = fork()) != 0) {  
                    signal(SIGCHLD, bg_sigchldhandler);
                }
            }
            else if (command_counter) {
                stdfd_backup[STDIN_FILENO] = dup(STDIN_FILENO);    // Backup stdin
            }
            
            if (pid == 0) {
                int i = 0, pipe_pid[2];
                while (i < command_counter - 1) {
                    // Creates two fid for an unnamed pipe. 
                    if (pipe(pipe_pid) < 0) {
                        fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
                        restore_stdfd(stdfd_backup);
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
                    
                    // Input redirection (parent process)
                    close(STDIN_FILENO);
                    dup(pipe_pid[STDIN_FILENO]);
                    close(pipe_pid[STDIN_FILENO]);
                    close(pipe_pid[STDOUT_FILENO]);

                    i++;
                }
                
                // Output redirection
                if (strcmp(filev[STDOUT_FILENO], "0") != 0) {
                    if (open_file(stdfd_backup, STDOUT_FILENO) < 0) continue;
                }
                
                if ((pid = fork()) == 0) {
                    // EXECUTE COMMAND
                    execvp(argvv[i][0], argvv[i]);
                    exit(errno);
                }
                else if (in_background) {
                    fprintf(stderr, "[%d]\n", pid);
                
                    if (wait(&status) == -1) 
                        fprintf(stderr, "Error waiting for child process: %s\n", strerror(errno));
                    exit(errno);
                }
                else {
                    if (wait(&status) == -1) 
                        fprintf(stderr, "Error waiting for child process: %s\n", strerror(errno));                    
                }
            }
        }

        // Restore file descriptor to default ones: stdin, stdout, stder
        restore_stdfd(stdfd_backup);
       
        // Print command
        // esto creo q se borra pq es apoyo visual para el desarrollo del msh
        // print_command(argvv, filev, in_background);
	}
	
	return 0;
}

/* mycalc */
void mycalc(char *argv[]) {
    // Result in stderr
    // Error in stdout

    for (int i = 0; i < 4; i++) {
        if (argv[i] == NULL) {
            fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
            return;
        }
    }

    if (argv[4] != NULL) {
        fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
        return;
    }

    // https://stackoverflow.com/questions/8871711/atoi-how-to-identify-the-difference-between-zero-and-error
    char *nptr, *endptr = NULL;                            /* pointer to additional chars  */
    long operand1, operand2;

    nptr = argv[1];
    endptr = NULL;
    operand1 = strtol(nptr, &endptr, 10);
    if (nptr && *endptr != 0) {
        fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
        return;
    }

    nptr = argv[3];
    endptr = NULL;
    operand2 = strtol(nptr, &endptr, 10);
    if (nptr && *endptr != 0) {
        fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
        return;
    }

    if (strcmp(argv[2], "add") == 0) {
        fprintf(stderr, "[OK] %ld + %ld = %ld; Acc %ld\n", operand1, operand2, operand1 + operand2, (mycalc_acc += operand1 + operand2));
    } 
    else if (strcmp(argv[2], "mul") == 0) {
        fprintf(stderr, "[OK] %ld * %ld = %ld\n", operand1, operand2, operand1 * operand2);
    } else if (strcmp(argv[2], "div") == 0) {
        if (operand2 == 0) {
            fprintf(stdout, "[ERROR] Division by zero is not allowed.\n");
            return;
        }
        fprintf(stderr, "[OK] %ld / %ld = %ld; Remainder %ld\n", operand1, operand2, operand1 / operand2, operand1 % operand2);
    } else {
        fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
    }
}

/* myhistory */
void myhistory(char *argv[]) {
    if (argv[1] == NULL) {
        for (int i = 0; i < 20; i++) {
            printf("<%d> %s %s %s %s\n", i, history[i].args[0], history[i].args[1], history[i].args[2], history[i].args[3]);
        }
    } else {
        int index = atoi(argv[1]);
        if (index < 0 || index >= 20) {
            printf("ERROR: Command not found\n");
        } else {
            printf("Running command <%d>\n", index);
            char *args[4] = {NULL, NULL, NULL, NULL}; // Initialize all elements to NULL
            char arg0[12], arg1[12], arg2[12]; // Buffer to hold the string representation of the integers
            if (history[index].args[0] != 0) {
                sprintf(arg0, "%s", history[index].args[0]);
                args[0] = arg0;
            }
            if (history[index].args[1] != 0) {
                sprintf(arg1, "%s", history[index].args[1]);
                args[1] = arg1;
            }
            if (history[index].args[2] != 0) {
                sprintf(arg2, "%s", history[index].args[2]);
                args[2] = arg2;
            }
            mycalc(args);
        }
    }
}

// To effectively kill the child process
void sigchldhandler(int param)
{
	// printf("****  CHILD DEAD! **** \n");
    signal(SIGCHLD, sigchldhandler);
    return;  
}

// To effectively kill the child process
void bg_sigchldhandler(int param)
{
	// printf("****  BG CHILD DEAD! **** \n");

    /* Since SIGCHLD is only sent by inmmidiate children, we can make sure that
    that the signal is sent by the command sequence handler process */
    int status = 0;
    if (wait(&status) == -1) 
        fprintf(stderr, "Error waiting for child process: %s\n", strerror(errno)); 

    signal(SIGCHLD, sigchldhandler);
    return;
}

/* This function restores all  */
void restore_stdfd(int *stdfd_backup) {
    for (int i = 0; i < 3; i++) {
        if (stdfd_backup[i] != 0) {
            close(i);                       /* close current */
            dup(stdfd_backup[i]);            // STD = fd
            close(stdfd_backup[i]);          /* close file */
            stdfd_backup[i] = 0;
        }
    }
}

// Redirection of file descriptors
int open_file(int *stdfd_backup, int fileno) {
    int filev_fd;
    switch (fileno) {
        case STDIN_FILENO:
            // Open file: https://stackoverflow.com/questions/59602852/permission-denied-in-open-function-in-c
            if ((filev_fd = open(filev[STDIN_FILENO], O_RDONLY, S_IRUSR | S_IWUSR)) < 0) {
                fprintf(stderr, "Error opening input file: %s\n", strerror(errno));
                restore_stdfd(stdfd_backup);
                return filev_fd;
            }
            break;
        case STDOUT_FILENO:
            if ((filev_fd = open(filev[STDOUT_FILENO], O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
                fprintf(stderr, "Error opening output file: %s\n", strerror(errno));
                restore_stdfd(stdfd_backup);
                return filev_fd;
            }
            break;
        case STDERR_FILENO:
            if ((filev_fd = open(filev[STDERR_FILENO],  O_CREAT | O_WRONLY | O_TRUNC , S_IRUSR | S_IWUSR)) < 0) {
                fprintf(stderr, "Error opening error output file: %s\n", strerror(errno));
                restore_stdfd(stdfd_backup);
                return filev_fd;
            } 
            break;
    }

    stdfd_backup[fileno] = dup(fileno);                            
    close(fileno);          
    dup(filev_fd);            
    close(filev_fd);     

    return fileno;
}




// Cambiar mycalc_acc por un environment variable