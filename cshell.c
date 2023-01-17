#include <stdio.h>
#include <dirent.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

// Defining constant variables to make the code easier to read
#define ANSI_RED_CODE     "\x1b[31m"
#define ANSI_GREEN_CODE   "\x1b[32m"
#define ANSI_BLUE_CODE    "\x1b[34m"
#define ANSI_RESET_CODE   "\x1b[0m"
#define RED 1
#define GREEN 2
#define BLUE 3
#define UNSUPPORTED -1
#define MAX_ENV_VARS 5000
#define MAX_COMMANDS 3000

typedef struct {
	char name[1000];
	char value[1000];
} EnvVar;
	
typedef struct {
	char name[5000];
	char time[64];
	int value;
} Command;

EnvVar envVars[MAX_ENV_VARS];
Command commands[MAX_COMMANDS];
int color = 0;


// This function prints a string in 3 different colors:
// Red, Green, and Blue and can also reset color to default. 
void print(char *str, int color) {
	if (color == RED) {
		printf(ANSI_RED_CODE);
	} else if (color == GREEN) {
		printf(ANSI_GREEN_CODE);
	} else if (color == BLUE) {
		printf(ANSI_BLUE_CODE);
	}
	
	printf("%s", str);
	printf(ANSI_RESET_CODE);
}

// This function is used to ignore the casing of the words for the theme command.
// I had to provide this function manually as it is not built into every C distribution.
// Reference: http://computer-programming-forum.com/47-c-language/baa7ac4c62a6a164.htm
char *str_case_str(char *sz_src, char *sz_find) {
	int i, k, n_find, n_src;
	if (!sz_src || !sz_src[0] || !sz_find || !sz_find[0]) {
		return 0;
	}
	
	n_src = strlen(sz_src);
	n_find = strlen(sz_find);
	for (i = k = 0; k < n_find && i < n_src; i++, k++) {
		if (toupper(sz_src[i]) != toupper(sz_find[k])) {
			i -= k;
			k = -1;
		}
	}
	if (k == n_find) {
		return sz_src + i - n_find;
	}
	return 0;
}

// Reference: http://www.cse.yorku.ca/~oz/hash.html
// My function is based off of the well known djb2 hash function and Java's hash function
unsigned long hash(char *str){
    unsigned long hash = 5003;
    int c;
    while (((c) = *str++))
        hash = ((hash << 5) - hash) + c; // hash * 31 + c
    return hash;
}

void parsecommand(char *command, char *args[], int *argc) {
	*argc = 0; 
	args[*argc] = strtok(command, " ");
	while (args[*argc] != NULL) {
		args[++(*argc)] = strtok(NULL, " ");
	}
}

// This function executes any non built in commands. 
// The command is executed in the child process and printed by the parent process.
void execute_non_built_in_command(char *non_built_in_command, int color, int number_of_commands) {
	int fd[2];
	
	if (pipe(fd) == -1) {
		perror("Something went wrong making the pipe.");
		exit(1);
	}
	pid_t id = fork();
	if (id == -1) {
		perror("Something went wrong creating a child process.");
		exit(1);
	} else if (id == 0) {
		close(fd[0]);
		//dup2(fd[1], STDOUT_FILENO); // Send stdout to the pipe
		dup2(fd[1], 2); // STDERR_FILENO //Send stderr ro the pipe
		close(fd[1]);
		
		char *args[10];
		int count = 0;
		parsecommand(non_built_in_command, args, &count);
		
		execvp(args[0], args);
		print("Incorrect keyword or command.\n", color);
		exit(1);		
	} else {
		char buffer[5000];
		close(fd[1]);
		ssize_t c = read(fd[0], buffer, sizeof(buffer));
		while (c != 0) {
			if (c == -1) {
				if (errno == EINTR) {
					continue;
				} else {
					perror("read");
					exit(1);
				}
			}
			print(buffer, color);
		}
		close(fd[0]);
		
		int stat;
		wait(&stat);
		commands[number_of_commands].value = WEXITSTATUS(stat);
	}
	
}

// This function executes the built in commands such as Exit, Theme, Log, and Print.
// We can also execute environment variables in this function. 
void execute(char *input, int number_of_commands) { 
	if (!input) {
		return;
	}
	
	char copy_of_input[256];
	strcpy(copy_of_input, input);
	
	// Check if the input is an Environment variable
	if (strlen(input) > 0 && input[0] == '$') {
		char *assignment = strtok(input, "$");
		char *var_name = strtok(assignment, "=");
		bool valid = false;
		if (var_name) {
			char *var_value = strtok(NULL, "");
			if (var_value) {
				int index = hash(var_name) % MAX_ENV_VARS;
				strcpy(envVars[index].name, var_name);
				strcpy(envVars[index].value, var_value);
				valid = true;
			}
		}
		
		if (!valid) {
			print("Invalid variable assignment", color);
			return;
		}
		
		commands[number_of_commands].value = 0;
		return;
	}
	
	char *word_token = strtok(input, " ");
	
	if (!word_token) {
		return;
	}
	
	if (word_token) {
		strcpy(commands[number_of_commands].name, word_token);
	}
	
	// Get the time required for the Log command
	time_t t = time(NULL);
	char command_date[100];
	
	struct tm *tm = localtime(&t);
	
	strftime(command_date, sizeof(command_date), "%c", tm);
	stpcpy(commands[number_of_commands].time, command_date);
	
	// Check if the input is an Exit command
	if (strstr(word_token, "exit")) {
		printf("Bye!\n", color);
		exit(0);
	}
	
	
	// Check if the input is a Print command
	if (strstr(word_token, "print")) {
		char *rest = strtok(NULL, "");
		if (rest == NULL) {
			return;
		}
		while (rest != NULL) {
			char *token = strtok_r(rest, " ", &rest);
			if (token == NULL) break;

			if (strstr(token, "$")) {
				char *var_name = strtok(token, " $");
				if (!var_name) {
					return;
				}
				
				int index = hash(var_name) % MAX_ENV_VARS;
				if (strcmp(envVars[index].name, var_name) == 0) {
					print(envVars[index].value, color);
				} else {
					print(token, color);
				}
			} else {
				print(token, color);
			}
			print(" ", color);
		}
		print("\n", color);
	
		commands[number_of_commands].value = 0;
		return;
	}
	
	
	// Check if the input is a Log command
	if (strstr(word_token, "log")) {
		for (int i = 0; i < number_of_commands; i++) { 
			char name[5000];
			char date[5000];
			char value[5000];
			
			// Printing the Date of the command
			strcpy(date, commands[i].time);
			strcat(date, "\n");
			print(date, color);

			// Printing the name of the commmand
			strcpy(name, commands[i].name);
			strcat(name, " ");
			print(name, color);
			
			// Printing the value of the command
			sprintf(value, "%d", commands[i].value); // This is required because we need our
													 // int value to be displayed as a string
			print(value, color);
			print("\n", color);
		}
		
		commands[number_of_commands].value = 0;
		return;
	}
	
	// Check if the input is a Theme command
	if (strstr(word_token, "theme")) {
		while (word_token != NULL) {
			if (str_case_str(word_token, "red")) {
				color = RED;
			} else if (str_case_str(word_token, "green")) {
				color = GREEN;
			} else if (str_case_str(word_token, "blue")) {
				color = BLUE;
			} else {
				color = UNSUPPORTED;
			}
			word_token = strtok(NULL, " ");
		}
		
		if (color == UNSUPPORTED) {
			print("This theme is not supported! \n", color);
			return;
			
		}
		
		commands[number_of_commands].value = 0;
		return;
	}
	
	execute_non_built_in_command(copy_of_input, color, number_of_commands);
}

// Script mode for the cshell
void script_mode(char *fileName) {
	int number_of_commands = 0;
	
	FILE *scriptFile = fopen(fileName, "r");
	if (scriptFile == NULL) {
		perror("Error: File does not exist");
		return;
	}
	
	char line[5000];
	while (fgets(line, sizeof(line), scriptFile) != NULL) {
		line[strcspn(line, "\r\n")] = 0;

		commands[number_of_commands].value = 1;
		execute(line, number_of_commands);
		number_of_commands++;
	}
	
	fclose(scriptFile);
	return;
}
	
// Interactive mode for the cshell
void interactive_mode() {
	int number_of_commands = 0;
	while(1) {
		char input[5000];
		print("cshell$ ", color);
		fgets(input, sizeof(input), stdin);
		input[strcspn(input, "\r\n")] = 0;

		commands[number_of_commands].value = 1;
		execute(input, number_of_commands);
		number_of_commands++;
	}
}


int main (int argc, char **argv) {
	if (argc > 1) {
		script_mode(argv[1]);
	} else {
		interactive_mode();
	}
	
	return 0;
}
