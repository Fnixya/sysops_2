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
#include <limits.h>      // for overflow and underflow

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

int mycalc(char *argv[]);
int myhistory(char *argv[]);

void sigchldhandler(int param);
// void fg_sigchldhandler(int param);

int my_strtol(char *string, long *number, const char* var);
void restore_stdfd(int *stdfd_backup);
int open_file(int *stdfd_backup, int fileno);

/* Our custom global variables __________________________________ */

int stdfd_backup[3] = {0};  // Array to store the backup of the main file descriptors: stdin, stdout, stderr. Initialized to zero
char acc_str[11];

/* _______________________________________________________ */

/**
 * Main sheell  Loop  
 */
int main(int argc, char* argv[])
{
    /* Our code SETENV  __________________________________ */

    signal(SIGCHLD, sigchldhandler);
    setenv("Acc", "0", 0);
    
    /* _______________________________________________ */


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
        
        // Execution of command or sequence of commands
        if (strcmp(argvv[0][0], "myhistory") == 0) {        // If command is myhistory
            myhistory(argvv[0]);
        }
        else if (strcmp(argvv[0][0], "mycalc") == 0) {      // If command is mycalc
            mycalc(argvv[0]);
        }
        // If command is any other than myhistory or mycalc -> then it is executed by execvp on a child process
        else {                    
            // Redirect error output if there is file in filev
            if (strcmp(filev[STDERR_FILENO], "0") != 0) {     
                if (open_file(stdfd_backup, STDERR_FILENO) < 0) continue;
            }   

            // Redirect input if there is file in filev
            if (strcmp(filev[STDIN_FILENO], "0") != 0) {     // Redirect if entry of filev is not "0"
                if (open_file(stdfd_backup, STDIN_FILENO) < 0) continue;
            }
            else if (1 < command_counter) 
                stdfd_backup[STDIN_FILENO] = dup(STDIN_FILENO);    // Backup stdin
            
            // Forking and piping
            int pid, pipe_pid[2], i = 0;
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
                else {
                    // Input redirection (parent process)
                    close(STDIN_FILENO);
                    dup(pipe_pid[STDIN_FILENO]);
                    close(pipe_pid[STDIN_FILENO]);
                    close(pipe_pid[STDOUT_FILENO]);
                }
                i++;
            }

            // Redirect output if there is file in filev
            if (strcmp(filev[STDOUT_FILENO], "0") != 0) {
                if (open_file(stdfd_backup, STDOUT_FILENO) < 0) continue;
            }
            
            // if (!in_background) signal(SIGCHLD, fg_sigchldhandler);

            // Last command of the sequence (or the only one)
            if ((pid = fork()) == 0) {
                // EXECUTE COMMAND
                execvp(argvv[i][0], argvv[i]);
                exit(errno);
            }

            // Restore file descriptor to default ones: stdin, stdout, stder
            restore_stdfd(stdfd_backup);
            
            if (in_background) {
                fprintf(stderr, "[%d]\n", pid);
            
                // if (wait(&status) == -1) 
                //     fprintf(stderr, "Error waiting for child process: %s\n", strerror(errno));
                // exit(errno);
            }
            else {
                // Block the shell and wait for the last child process to finish
                waitpid(pid, &status, 0);
            }

            store_command(argvv, filev, in_background, &history[tail]);
            tail = ++tail % 20;
        }
       
        // Print command
        // esto creo q se borra pq es apoyo visual para el desarrollo del msh
        // print_command(argvv, filev, in_background);
	}
	
	return 0;
}

/* mycalc */
int mycalc(char *argv[]) {
    // Result in stderr
    // Error in stdout

    for (int i = 0; i < 4; i++) {
        if (argv[i] == NULL) {
            fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
            return -1;
        }
    }

    if (argv[4] != NULL) {
        fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
        return -1;
    }

    long operand1, operand2, acc_l;

    // Conversion of <openrand_1> from str to long int using strtol (more secure)
    if (my_strtol(argv[1], &operand1, "operand_1") < 0) return -1;


    // Conversion of <openrand_2> from str to long int using strtol (more secure)
    if (my_strtol(argv[3], &operand2, "operand_2") < 0) return -1;


    // Calculations
    if (strcmp(argv[2], "add") == 0) {
        // Conversion of Acc from str to long int using strtol (more secure)
        if (my_strtol(getenv("Acc"), &acc_l, "acc") < 0) return -1;

        // sprintf() more secure than itoa() for conversion of: int -> str
        sprintf(acc_str, "%ld", acc_l += operand1 + operand2);
        setenv("Acc", acc_str, 1);
        fprintf(stderr, "[OK] %ld + %ld = %ld; Acc %ld\n", operand1, operand2, operand1 + operand2, acc_l);
    } 
    else if (strcmp(argv[2], "mul") == 0) {
        int res = operand1 * operand2;
        if (res / operand1 != operand2) {
            fprintf(stdout, "[ERROR] Overflow at multiplication\n");
            return -1;
        }
        fprintf(stderr, "[OK] %ld * %ld = %ld\n", operand1, operand2, res);
    } 
    else if (strcmp(argv[2], "div") == 0) {
        if (operand2 == 0) {
            fprintf(stdout, "[ERROR] Division by zero is not allowed.\n");
            return -1;
        }
        fprintf(stderr, "[OK] %ld / %ld = %ld; Remainder %ld\n", operand1, operand2, operand1 / operand2, operand1 % operand2);
    } 
    else {
        fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
    }

    return 0;
}

/* myhistory */
int myhistory(char *argv[]) {
    if (argv[1] == NULL) {
        for (int i = 0; i < 20; i++) {
            if (history[i].argvv == NULL) break;

            fprintf(stderr, "<%d> ", i, history[i].args[0]);
            for (int ii = 0; ii < history[i].num_commands; ii++) {
                fprintf(stderr, "%d ", history[i].args[ii]);
            }
            fprintf(stderr, "\n");
        }
    } else {
        int index = atoi(argv[1]);
        // if (my_strtol(argv[1], &index, "index") < 0) return;

        if (index < 0 || index >= 20) {
            fprintf(stdout, "ERROR: Command not found\n");
        }
        else {
            fprintf(stderr, "Running command <%d>\n", index);
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

    return 0;
}

// To effectively kill the child process
void sigchldhandler(int param)
{
	// printf("****  CHILD DEAD! **** \n");
    int status = 0;
    wait(&status);

    signal(SIGCHLD, sigchldhandler);
    return;
}


// // To effectively kill the child process
// void fg_sigchldhandler(int param)
// {
// 	// printf("****  FG CHILD DEAD! **** \n");
//     int status = 0;
//     if (wait(&status) == -1) 
//         fprintf(stderr, "Error waiting for child process: %s\n", strerror(errno)); 
    
//     /* Since SIGCHLD is only sent by inmmidiate children, it can be activated 
//     before forking the shell so that it can wait and kill the child process effectively */
//     signal(SIGCHLD, fg_sigchldhandler);
//     return;
// }


/* Error -1 otherwise 0 */
int my_strtol(char *string, long *number, const char* var) {
    // https://stackoverflow.com/questions/8871711/atoi-how-to-identify-the-difference-between-zero-and-error
    char *nptr, *endptr = NULL;                            /* pointer to additional chars  */
    
    nptr = string;
    endptr = NULL;
    errno = 0;
    *number = strtol(nptr, &endptr, 10);
    if (nptr && *endptr != 0) {
        fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
        return -1;
    }
    else if (errno == ERANGE && *number == LONG_MAX)
    {
        fprintf(stdout, "[ERROR] Overflow at <%s>\n", var);
        return -1;
    }
    else if (errno == ERANGE && *number == LONG_MIN)
    {
        fprintf(stdout, "[ERROR] Underflow at <%s>\n", var);
        return -1;
    }

    return 0;
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
            if ((filev_fd = open(filev[STDIN_FILENO], O_RDONLY)) < 0) {
                fprintf(stderr, "Error opening input file: %s\n", strerror(errno));
                restore_stdfd(stdfd_backup);
                return filev_fd;
            }
            break;
        case STDOUT_FILENO:
            if ((filev_fd = open(filev[STDOUT_FILENO], O_CREAT | O_WRONLY | O_TRUNC, 0664)) < 0) {
                fprintf(stderr, "Error opening output file: %s\n", strerror(errno));
                restore_stdfd(stdfd_backup);
                return filev_fd;
            }
            break;
        case STDERR_FILENO:
            if ((filev_fd = open(filev[STDERR_FILENO],  O_CREAT | O_WRONLY | O_TRUNC , 0664)) < 0) {
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



// A lo mejor hay q cambiar todos los fprintf(stderr) por perror()
// Each command must execute as an immediate child of the minishell, spawned by fork command (man 2 fork).
// Si te apetece ponerle error control a los resultados de las operaciones. Good luck.



// Incluido sigkill handler