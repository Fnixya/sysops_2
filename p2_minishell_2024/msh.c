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

#include <errno.h>          // errno
#include <limits.h>         // LONG_MIN and LONG_MAX for overflow and underflow

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




/* Our custom functions _________________________________________ */

int myshell(char*** argvv, int command_counter, int in_background);
int mycalc(char *argv[]);
int run_command(char ***argvv, int command_counter, int in_background);

int myhistory(char ***argvv);

void sigchldhandler(int param);

int redirect_to_file(int fileno);
void print_history();
int my_strtol(char *string, long *number, const char* var, int mode);

/* Our custom global variables __________________________________ */

char acc_str[19];           // MAX 9,223,372,036,854,775,807

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
            myhistory(argvv);
        }
        else {
            // Store command in history
            if (n_elem == 20) {
                /* If history is full then free the oldest command, 
                store the new one in the same memory space and move the head and tail */
                free_command(&history[head]);
                store_command(argvv, filev, in_background, &history[tail]);
                head = ++head % 20;
                tail = head;
            }
            else {
                /* If history is not full then just 
                store it in the next available memory space */
                store_command(argvv, filev, in_background, &history[tail]);
                tail = ++tail % 20;
                n_elem++;
            }
            
            run_command(argvv, command_counter, in_background);
        }

        // print_command(argvv, filev, in_background);
	}
	
	return 0;
}




/***
 * Shell code
 * @param ***argvv: array of all arguments of the command sequence
 * @param command_counter: number of commands of ***argvv
 * @param in_background: 1 if the command is executed in background, 0 if not
 * @param from_history: 1 if the command is extracted from history, 0 if it is read from the terminal
 * @return -1 if error, 0 if successful
 */
int myshell(char*** argvv, int command_counter, int in_background) {         
    // Forks and pipes for each pair of consecutive commands in the sequence
    int pid, pipe_fd[2], pipe_in, pipe_out, i = 0, status = 0;
    while (i < command_counter) {
        // Creates two pid for an unnamed pipe. 
        if (i < command_counter - 1) {
            if (pipe(pipe_fd) < 0) {
                fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
                exit(errno);
            }

            // Save output of the pipe to be used in the current child process
            pipe_out = pipe_fd[STDOUT_FILENO];
        }
    
        // Child process
        if ((pid = fork()) == 0) {   
            // Redirect error output if there is file in filev
            if (strcmp(filev[STDERR_FILENO], "0") != 0) {     
                if (redirect_to_file(STDERR_FILENO) < 0) return -1;
            }

            // Input redirection
            if (i == 0) {
                // Redirect input if there is file in filev
                if (strcmp(filev[STDIN_FILENO], "0") != 0) {     
                    if (redirect_to_file(STDIN_FILENO) < 0) return -1;
                }
            }
            else {       
                // Input redirection (parent process)
                if (close(STDIN_FILENO) < 0) {
                    fprintf(stderr, "Error closing file: %s\n", strerror(errno));
                    exit(errno);
                }
                if (dup(pipe_in) < 0) {
                    fprintf(stderr, "Error duplicating file pointer: %s\n", strerror(errno));
                    exit(errno);
                }
                if (close(pipe_in) < 0) {
                    fprintf(stderr, "Error closing file: %s\n", strerror(errno));
                    exit(errno);
                }
            }

            // Output redirection
            if (i == command_counter - 1) {
                // Redirect output if there is file in filev
                if (strcmp(filev[STDOUT_FILENO], "0") != 0) {
                    if (redirect_to_file(STDOUT_FILENO) < 0) return -1;
                }
            }
            else {
                // Redirect output to unnamed pipe
                if (close(STDOUT_FILENO) < 0) {
                    fprintf(stderr, "Error closing file: %s\n", strerror(errno));
                    exit(errno);
                }
                if (dup(pipe_out) < 0) {
                    fprintf(stderr, "Error duplicating file pointer: %s\n", strerror(errno));
                    exit(errno);
                }
                if (close(pipe_out) < 0) {
                    fprintf(stderr, "Error closing file: %s\n", strerror(errno));
                    exit(errno);
                }
            }

            // EXECUTE COMMAND
            if (execvp(argvv[i][0], argvv[i]) == -1) {
                exit(errno);
            }
        }

        // Close pipes in parent process
        if (i != command_counter - 1) {
            if (close(pipe_out) < 0) {
                fprintf(stderr, "Error closing file: %s\n", strerror(errno));
                exit(errno);
            }
        }
        if (i != 0) {
            if (close(pipe_in) < 0) {
                fprintf(stderr, "Error closing file: %s\n", strerror(errno));
                exit(errno);
            }
        }

        // Save input pipe for next child process
        pipe_in = pipe_fd[STDIN_FILENO];

        i++;
    }
    
    if (in_background)
        // Prints the pid of the last command in the sequence if in background 
        fprintf(stderr, "[%d]\n", pid);
    else
        /* If it is in foreground then
            block the msh and wait for the last child process to finish */
        waitpid(pid, &status, 0);
     
    return 0;
}


/***
 * Interal command that acts as a basic calculator
 * @param *argv[]: array of arguments of the mycalc command
 * @return -1 if error, 0 if successful
 */
int mycalc(char *argv[]) {
    // Check if the command has no less than 4 arguments
    for (int i = 0; i < 4; i++) {
        if (argv[i] == NULL) {
            fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
            return -1;
        }
    }

    // Check if the command has no more than 4 arguments
    if (argv[4] != NULL) {
        fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
        return -1;
    }

    long operand1, operand2, acc_l;

    // Conversion of <openrand_1> from str to long int using strtol (more secure)
    if (my_strtol(argv[1], &operand1, "operand_1", 0) < 0) return -1;

    // Conversion of <openrand_2> from str to long int using strtol (more secure)
    if (my_strtol(argv[3], &operand2, "operand_2", 0) < 0) return -1;

    // Addition
    if (strcmp(argv[2], "add") == 0) {
        // Conversion of Acc from str to long int using strtol (more secure)
        if (my_strtol(getenv("Acc"), &acc_l, "acc", 0) < 0) return -1;

        // sprintf() more secure than itoa() for conversion of: int -> str
        sprintf(acc_str, "%ld", acc_l += operand1 + operand2);
        setenv("Acc", acc_str, 1);
        fprintf(stderr, "[OK] %ld + %ld = %ld; Acc %ld\n", operand1, operand2, operand1 + operand2, acc_l);
    } 
    // Multiplication
    else if (strcmp(argv[2], "mul") == 0) {
        long res = operand1 * operand2;

        // Check for math errors in multiplication (overflow or underflow)
        if (operand1 != 0) {
            if (res / operand1 != operand2) {
                if (res < 0)
                    fprintf(stdout, "[ERROR] Underflow at multiplication\n");
                else
                    fprintf(stdout, "[ERROR] Overflow at multiplication\n");

                return -1;
            }
        }
        fprintf(stderr, "[OK] %ld * %ld = %ld\n", operand1, operand2, res);
    } 
    // Division
    else if (strcmp(argv[2], "div") == 0) {
        // Check for division errors
        if (operand2 == 0) {
            fprintf(stdout, "[ERROR] Division by zero is not allowed\n");
            return -1;
        }
        fprintf(stderr, "[OK] %ld / %ld = %ld; Remainder %ld\n", operand1, operand2, operand1 / operand2, operand1 % operand2);
    } 
    // Error
    else {
        fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
    }

    return 0;
}


/***
 * It runs the POSIX services or the internal comand myhistory
 * @param ***argvv: array of all arguments of the command sequence
 * @param command_counter: number of commands of ***argvv
 * @param in_background: 1 if the command is executed in background, 0 if not
 * @param from_history: 1 if the command is extracted from history, 0 if it is read from the terminal
 * @return -1 if error, 0 if successful
 */
int run_command(char ***argvv, int command_counter, int in_background) {
    // Run mycalc if command is mycalc
    if (strcmp(argvv[0][0], "mycalc") == 0) {      
        // If mycalc is part of a sequence of commands then don't execute it: format is wrong
        if (command_counter != 1) {
            fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
            return -1;
        } else
            return mycalc(argvv[0]);
    }
    // If command is not mycalc -> then it is executed by execvp() by a child process
    else {      
        return myshell(argvv, command_counter, in_background);
    }
}


/***
 * Internal command myhistory
 *  @param *argv[]: array of arguments of a command
 *  @return -1 if error, 0 if it prints history of commands, 1 if it runs a command from history
***/
int myhistory(char ***argvv) {
    char **argv = argvv[0];
    // If <myhistory>
    if (argv[1] == NULL) {
        print_history();
    } 
    // If <myhistory> <index>
    else if (argv[2] == NULL) {
        // Conversion of command index from char* to int, aborts command if conversion fails
        long index;
        if (my_strtol(argv[1], &index, "", 1) < 0) {
            fprintf(stdout, "ERROR: Command not found\n");
            return -1;
        }

        if (index < 0 || index >= 20 || n_elem < index) {
            fprintf(stdout, "ERROR: Command not found\n");
            return -1;
        }
        else {
            fprintf(stderr, "Running command %ld\n", index);

            index = (index + head) % 20;
            
            char ***argvv_execvp = history[index].argvv; // Get all commands of the piped sequence from history
            memcpy(filev, history[index].filev, sizeof(filev));

            if (run_command(argvv_execvp, history[index].num_commands, history[index].in_background) < 0) {
                fprintf(stdout, "ERROR: Failed to execute command\n");
                return -1;
            }

            return 1;
        }
    }
    else {
        fprintf(stdout, "ERROR: Command not found\n");
        return -1;
    }

    return 0;
}


/*** 
 * Prints the history of commands up to history_size 
*/
void print_history() {
    int i, ii, iii;             // Iterators    
    struct command curr_cmd;    // Command

    for (i = 0; i < history_size; i++) {
        curr_cmd = history[(i+head)%20];

        // IF THERE ARE NO MORE COMMANDS IN HISTORY THEN STOP PRINTING
        if (curr_cmd.argvv == NULL) break;

        // PRINT COMMAND INDEX
        fprintf(stderr, "%d ", i);

        // PRINT COMMANDS
        for (ii = 0; ii < curr_cmd.num_commands - 1; ii++) {
            for (int iii = 0; iii < curr_cmd.args[ii]; iii++) {
                fprintf(stderr, "%s ", curr_cmd.argvv[ii][iii]);
            }
            fprintf(stderr, "| ");
        }
        for (iii = 0; iii < curr_cmd.args[ii]; iii++) {
            fprintf(stderr, "%s ", curr_cmd.argvv[ii][iii]);
        }

        // PRINT INPUT REDIRECTION
        if (strcmp(curr_cmd.filev[STDIN_FILENO], "0") != 0) {
            fprintf(stderr, "< %s ", curr_cmd.filev[STDIN_FILENO]);
        }
        
        // PRINT OUTPUT REDIRECTION
        if (strcmp(curr_cmd.filev[STDOUT_FILENO], "0") != 0) {
            fprintf(stderr, "> %s ", curr_cmd.filev[STDOUT_FILENO]);
        }
        
        // PRINT ERROR OUTPUT REDIRECTION
        if (strcmp(curr_cmd.filev[STDERR_FILENO], "0") != 0) {
            fprintf(stderr, "!> %s ", curr_cmd.filev[STDERR_FILENO]);
        }

        // PRINT BACKGROUND
        if (curr_cmd.in_background) fprintf(stderr, " &");

        fprintf(stderr, "\n");
    }

    return;
}


/*** 
 * Signal handler for SIGCHLD, which waits for all child processes that finished
*/
void sigchldhandler(int param)
{
	// printf("****  CHILD DEAD! **** \n");
    int status = 0;
    wait(&status);

    signal(SIGCHLD, sigchldhandler);
    return;
}


/***
 * Conversion from string to long integer using strtol with some error handling
 * @param string: string to convert to long
 * @param number: pointer to store the result
 * @param var: variable name (used to print error message)
 * @param mode: 0 to print error message (made for mycalc), otherwise it does not print error message 
 * @return Error -1 otherwise 0 */
int my_strtol(char *string, long *number, const char* var, int mode) {
    // https://stackoverflow.com/questions/8871711/atoi-how-to-identify-the-difference-between-zero-and-error
    char *nptr, *endptr = NULL;                            /* pointer to additional chars  */
    
    nptr = string;
    endptr = NULL;
    errno = 0;
    *number = strtol(nptr, &endptr, 10);

    // Error extracting number (it is not an integer)
    if (nptr && *endptr != 0) {
        if (mode == 0)
            fprintf(stdout, "[ERROR] The structure of the command is mycalc <operand_1> <add/mul/div> <operand_2>\n");
        return -1;
    }
    // Overflow
    else if (errno == ERANGE && *number == LONG_MAX)
    {
        if (mode == 0)
            fprintf(stdout, "[ERROR] Overflow at <%s>\n", var);
        return -1;
    }
    // Underflow
    else if (errno == ERANGE && *number == LONG_MIN)
    {
        if (mode == 0)
            fprintf(stdout, "[ERROR] Underflow at <%s>\n", var);
        return -1;
    }

    return 0;
}

/***
 * It redirects the input/output to filev[fileno]
 * @param fileno: file number to redirect: STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO
 * @return fd of the opened file (-1 if error)
*/
int redirect_to_file(int fileno) {
    // Opens the file in filev[fileno]
    int filev_fd;
    switch (fileno) {
        case STDIN_FILENO:
            if ((filev_fd = open(filev[STDIN_FILENO], O_RDONLY)) < 0) {
                fprintf(stderr, "Error opening input file: %s\n", strerror(errno));
                return filev_fd;
            }
            break;
        case STDOUT_FILENO:
            if ((filev_fd = open(filev[STDOUT_FILENO], O_CREAT | O_WRONLY | O_TRUNC, 0664)) < 0) {
                fprintf(stderr, "Error opening output file: %s\n", strerror(errno));
                return filev_fd;
            }
            break;
        case STDERR_FILENO:
            if ((filev_fd = open(filev[STDERR_FILENO],  O_CREAT | O_WRONLY | O_TRUNC , 0664)) < 0) {
                fprintf(stderr, "Error opening error output file: %s\n", strerror(errno));
                return filev_fd;
            } 
            break;
        default:
            return 0;
            break;
    }

    // Redirecting the opened file to stdin/stdout/stderr
    if (close(fileno) < 0) {
        fprintf(stderr, "Error closing file: %s\n", strerror(errno));
        exit(errno);
    }
    if (dup(filev_fd) < 0) {
        fprintf(stderr, "Error duplicating file pointer: %s\n", strerror(errno));
        exit(errno);
    }      
    if (close(filev_fd) < 0) {
        fprintf(stderr, "Error closing file: %s\n", strerror(errno));
        exit(errno);
    }      

    return fileno;
}


// zip p2_minishell.zip msh.c authors.txt
// ./checker_os_p2.sh p2_minishell.zip