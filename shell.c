#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

#define COMMAND_LENGTH 1024
#define NUM_TOKENS (COMMAND_LENGTH / 2 + 1)
#define DELIMITERS " \t\n"
#define HISTORY_DEPTH 10

char history[HISTORY_DEPTH][COMMAND_LENGTH];
int history_number[1024];
int history_count;
int child_retval;
int child_status;

/**
* Read a command from the keyboard into the buffer 'buff' and tokenize it
* such that 'tokens[i]' points into 'buff' to the i'th token in the command.
* buff: Buffer allocated by the calling code. Must be at least
* COMMAND_LENGTH bytes long.
* tokens[]: Array of character pointers which point into 'buff'. Must be at
* least NUM_TOKENS long. Will strip out up to one final '&' token.
* 'tokens' will be NULL terminated.
* in_background: pointer to a boolean variable. Set to true if user entered
* an & as their last token; otherwise set to false.
*/

void shell_print_history()
{
    int i = 0;
    char num[4];

    if (history_count <= HISTORY_DEPTH) {
        while (i < history_count - 1) {
            sprintf(num, "%4d", history_number[i]);
            write(STDOUT_FILENO, num, strlen(num));
            write(STDOUT_FILENO, ". ", strlen(". "));
            write(STDOUT_FILENO, history[i], strlen(history[i]));
            write(STDOUT_FILENO, "\n", strlen("\n"));
            i++;
        }
    }
    else {
        while (i < HISTORY_DEPTH) {
            sprintf(num, "%4d", history_number[history_count - (HISTORY_DEPTH - i)]);
            write(STDOUT_FILENO, num, strlen(num));
            write(STDOUT_FILENO, ". ", strlen(". "));
            write(STDOUT_FILENO, history[i], strlen(history[i]));
            write(STDOUT_FILENO, "\n", strlen("\n"));
            i++;
        }
    }
    return;
}

int shell_get_num(char *buff)
{
    char index_char[5];
    _Bool validnum = true;
    int num, i;

    for (i = 0; buff[i] != '\0'; i++) {
        if (i > 0 && !isdigit(buff[i])) {
            validnum = false;
            break;
        }
        buff[i] = buff[i + 1];
    }
    if (!validnum)
        num = -1;
    else {
        strcpy(index_char, buff);
        num = atoi(index_char); 
    }
    if (num > history_count)
        num = -1;

    return num;
}

char* shell_retrieve_history(char *buff)
{
    char his_command[1024], out[40];;
    char *his_ptr = NULL;
    int index_num;

    if (buff[1] == '!' && buff[2] == '\0') {
        if (history_count <= HISTORY_DEPTH) {
            strcpy(his_command, history[history_count - 1]);
            his_ptr = &(his_command[0]);
        }
        else {
            strcpy(his_command, history[9]);
            his_ptr = &(his_command[0]);
        }
    }
    else {
        index_num = shell_get_num(buff);
        if (index_num != -1) {
            if (history_count <= HISTORY_DEPTH) {
                strcpy(his_command, history[index_num - 1]);
                his_ptr = &(his_command[0]);
            }
            else {
                strcpy(his_command, history[9 + index_num - history_count]);
                his_ptr = &(his_command[0]);
            }
        }
    }
    if (his_ptr == NULL) {
        strcpy(out, "shell: Invalid history command.\n");
        write(STDOUT_FILENO, out, strlen(out));
    }

    return his_ptr;
}

void shell_add_to_history(char *command)
{
    int i = 0;

    if (history_count < HISTORY_DEPTH)
        strcpy(history[history_count], command);
    else {
        while (i < HISTORY_DEPTH - 1) {
            strcpy(history[i], history[i + 1]);
            i++;
        }
        strcpy(history[i], command);
    }
    history_number[history_count] = history_count + 1;
    history_count++;

    return;
}

int shell_execute(char *tokens[], _Bool in_background)
{
    pid_t childpid;

    childpid = fork();

    if (childpid >= 0) {
        if (childpid == 0) {
            child_retval = 0;
            if (execvp(tokens[0], tokens) == -1) {
                child_retval = -1;
            }
            exit(child_retval);
        }
        else {
            if (!in_background) {
                waitpid(childpid, &child_status, 0);
                if (WEXITSTATUS(child_status) != 0) {
                    char output[1024];
                    strcpy(output, "shell: \"");
                    strcat(output, tokens[0]);
                    strcat(output, "\" unknown command\n");
                    write(STDOUT_FILENO, output, strlen(output)); 
                }
            }
            while (waitpid(-1, NULL, WNOHANG) > 0); 
        }
    }
    else {
        perror("fork");
        exit(0);
    }

    return 1;
}

void  shell_sig_handler(int sig)
{
     if (sig == SIGINT) {
        printf("\n");
        shell_print_history();
     }    
}

int tokenize_command(char *command, char *tokens[])
{
    int i = 0;
    char *token;

    token = strtok(command, DELIMITERS);
    while (token != NULL) {
        tokens[i] = token;
        token = strtok(NULL, DELIMITERS);
        i++;
    }
    tokens[i] = NULL;

    return i;
}

void read_command(char *buff, char *tokens[], _Bool *in_background)
{
    *in_background = false;

    // Read input
    int length = read(STDIN_FILENO, buff, COMMAND_LENGTH-1);
    signal(SIGINT, shell_sig_handler);
    if (length < 0 && errno == EINTR) {
        buff = NULL;
        tokens[0] = NULL;
        return;
    }
    if (length < 0) {
        perror("Unable to read command. Terminating.\n");
        exit(-1); /* terminate with error */
    }
    if ( (length < 0) && (errno != EINTR) ) {
        perror("Unable to read command. Terminating.\n");
        exit(-1); /* terminate with error */
    }   

    // Null terminate and strip \n.
    buff[length] = '\0';
    if (buff[strlen(buff) - 1] == '\n')
        buff[strlen(buff) - 1] = '\0';
    if (buff[0] == '!')
        buff = shell_retrieve_history(buff);
    if (buff == NULL) {
        tokens[0] = NULL;
        return;
    }  
    
    // Tokenize (saving original command string)
    char command[1024];
    strcpy(command, buff);
    int token_count = tokenize_command(command, tokens);
    if (token_count == 0)
        return;
    shell_add_to_history(buff);

    // Extract if running in background:
    if (token_count > 0 && strcmp(tokens[token_count - 1], "&") == 0) {
        *in_background = true;
        tokens[token_count - 1] = 0;
    }
}

/**
* Main and Execute Commands
*/
int main(int argc, char* argv[])
{
    history_count = 0;
    char input_buffer[COMMAND_LENGTH];
    char *tokens[NUM_TOKENS];
    char wd[1024], prompt[1024];

    strcpy(prompt, "> ");

    while (true) {

        // Get command
        // Use write because we need to use read()/write() to work with
        // signals, and they are incompatible with printf().
        write(STDOUT_FILENO, prompt, strlen(prompt));
        _Bool in_background = false;
        read_command(input_buffer, tokens, &in_background);

        if (tokens[0] == NULL)
            continue;

        if (strcmp(tokens[0], "exit") == 0)
            break;
        else if (strcmp(tokens[0], "cd") == 0) {
            if (tokens[1] == NULL) {
                char out[60];
                strcpy(out, "shell: Expected argument for \"cd\", no action taken.\n");
                write(STDOUT_FILENO, out, strlen(out));
            }   
            else if (chdir(tokens[1]) != 0) {
                perror("shell");
            }
            else {
                getcwd(prompt, sizeof(prompt));
                strcat(prompt, "> ");
            }         
        }
        else if (strcmp(tokens[0], "pwd") == 0) {
            char out[30];
            getcwd(wd, sizeof(wd));
            strcpy(out, "shell: Working directory: ");
            write(STDOUT_FILENO, out, strlen(out));
            write(STDOUT_FILENO, strcat(wd, "\n"), strlen(strcat(wd, "\n")));
        }
        else if (strcmp(tokens[0], "history") == 0) {
            shell_print_history();
        }  
        else {
            shell_execute(tokens, in_background);
        }    
    }

    return 0;
} 