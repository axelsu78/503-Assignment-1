#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <fcntl.h>  // For open(), O_RDONLY, O_WRONLY, O_CREAT, etc.
#include <sys/types.h>  // For mode_t
#include <sys/stat.h>   // For file permissions

#define MAX_LINE 80 /* The maximum length command */

int main(void) {
    char *args[MAX_LINE/2 + 1]; /* command line arguments */
    int should_run = 1; /* flag to determine when to exit program */
    
    // Add any other variables you need here
    char input[MAX_LINE]; // Buffer to store user input

    char last_input[MAX_LINE] = {0}; // Previous command buffer
    int has_history = 0; // Flag to indicate if there's a command history
    
    while (should_run) {
        printf("osh>");
        fflush(stdout);
        
        // Reading input
        if (fgets(input, MAX_LINE, stdin) == NULL){
            perror("fgets failed");
            continue;
        }        
        
        // Removing trailing newline
        size_t length = strlen(input);
        if (length > 0 && input[length - 1] == '\n'){
            input[length - 1] = '\0';
        }
        
        // Check for !! (history command)
        if (strcmp(input, "!!") == 0) {
            if (!has_history) {
                printf("No commands in history.\n");
                continue;
            }
            printf("%s\n", last_input);
            strcpy(input, last_input);
        } else {
            // Save current command as history
            strcpy(last_input, input);
            has_history = 1;
        }

        // Parsing input into tokens
        int i = 0;
        char* token;
        token = strtok(input, " \t");
        while (token != NULL && i < MAX_LINE/2) {
            args[i] = token;
            i++;
            token = strtok(NULL, " \t");
        }

        args[i] = NULL;

        // Empty Command
        if (i == 0){
            continue;
        }

        // Exit Command
        if (strcmp(args[0], "exit") == 0){
            should_run = 0;
            continue;
        }
        
        int background = 0;
        if (i > 0 && strcmp(args[i - 1], "&") == 0){
            background = 1;
            args[i-1] = NULL;
            i--;
        }

        // I/O Redirection
        int input_redirection = 0;
        int output_redirection = 0;
        char *input_file = NULL;
        char *output_file = NULL;

        for (int j = 0; j < i; j++){
            if (args[j] != NULL){
                if (strcmp(args[j], "<") == 0){
                    if (j + 1 < i && args[j + 1] != NULL){
                        input_redirection = 1;
                        input_file = args[j + 1];
                        args[j] = NULL;
                        j++;
                    }
                } else if (strcmp(args[j], ">") == 0){
                    if (j + 1 < i && args[j + 1] != NULL){
                        output_redirection = 1;
                        output_file = args[j + 1];
                        args[j] = NULL;
                        j++;
                    }
                }
            }
        }

        // Pipe
        int pipe_present = 0;
        char* args_pipe[MAX_LINE / 2 + 1];
        int pipe_index = -1;

        for (int j = 0; j < i; j++){
            if (args[j] != NULL && strcmp (args[j], "|") == 0){
                pipe_present = 1;
                pipe_index = j;
                args[pipe_index] = NULL;
                int k = 0;
                for (int m = pipe_index + 1; m < i; m++){
                    args_pipe[k++] = args[m];
                }
                args_pipe[k] = NULL;
                break;
            }
        }

        if (pipe_present){
            int pipefd[2];
            if (pipe(pipefd) < 0){
                perror("Pipe failed");
                continue;
            }

            // First child
            pid_t pid1 = fork();
            if (pid1 < 0){
                perror("Fork failed for first command");
                continue;
            } else if (pid1 == 0){
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                if (execvp(args[0], args) < 0){
                    printf("Command Not Found: %s\n", args[0]);
                    exit(1);
                }
            }

            pid_t pid2 = fork();
            if (pid2 < 0){
                perror("Fork failed for second command");
                continue;
            } else if (pid2 == 0){
                close(pipefd[1]);
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[0]);

                if (execvp(args_pipe[0], args_pipe) < 0){
                    printf("Command Not Found: %s\n", args_pipe[0]);
                    exit(1);
                }
            }

            close(pipefd[0]);
            close(pipefd[1]);

            if (!background){
                waitpid(pid1, NULL, 0);
                waitpid(pid2, NULL, 0);
            } else {
                printf("Background Process Started: %d, %d\n", pid1, pid2);
            }

        } else {
            pid_t pid = fork();

            if (pid < 0){
                perror("Fork failed");
                continue;
            } else if (pid == 0){

                if (input_redirection){
                    int fd = open(input_file, O_RDONLY);
                    if (fd < 0){
                        printf("Error: Cannot open input file %s\n", input_file);
                        exit(1);
                    }
                    if (dup2(fd, STDIN_FILENO) < 0){
                        perror("dup2 failed for input redirection");
                        exit(1);
                    }
                    close(fd);
                }

                if (output_redirection){
                    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0){
                        printf("Error: Cannot open output file %s\n", output_file);
                        exit(1);
                    }

                    if (dup2(fd, STDOUT_FILENO) < 0){
                        perror("dup2 failed for output direction");
                        exit(1);
                    }
                    close(fd);
                }

                if (execvp(args[0], args) < 0){
                    printf("Command Not Found: %s\n", args[0]);
                    exit(1);
                }
            } else {
                if (!background){
                    waitpid(pid, NULL, 0);
                } else {
                    printf("Background Process Started: %d\n", pid);
                }
            }
        }
       
    }

    return 0;
}