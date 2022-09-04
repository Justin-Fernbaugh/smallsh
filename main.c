#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
int allowBackground = 1;

/*-------------------------------------------------*/
//List to hold background processes and 1 foreground process for shellStatus()
struct list {
	pid_t pid;
	int status;
	int foreground;					//1 if foreground proc, 0 if background proc
	struct list *next;
};
/*-------------------------------------------------*/
/*
Parameters: head pointer, pid of new node, status of new node, foreground status of new node
Description: adds a new node to the linked list declared dynamically on the heap.
Return Value: void
*/
void push(struct list** head_ref, pid_t new_pid, int new_status, int new_foreground)
{
    struct list* new_node
        = (struct list*)malloc(sizeof(struct list));
    new_node->pid = new_pid;
	new_node->status = new_status;
	new_node->foreground = new_foreground;
    new_node->next = (*head_ref);
    (*head_ref) = new_node;
}
/*-------------------------------------------------*/
/*
Parameters: head pointer, PID of node to be deleted.
Description: Deletes a node with specific process ID
Return Value: void
*/
void deleteNode(struct list **head_ref, pid_t key) {
	struct list *curr = *head_ref, *prev;
 
    // if head is the key
    if (curr != NULL && curr->pid == key) {
        *head_ref = curr->next; // swap head
        free(curr);
        return;
    }
 
	//search for key
    while (curr != NULL && curr->pid != key) {
        prev = curr;
        curr = curr->next;
    }
 
    // if not in list
    if (curr == NULL)
        return;
 
    prev->next = curr->next;
    free(curr); // free node
}
/*-------------------------------------------------*/
/*
Parameters: head pointer, bool foreground
Description: Deletes node with int foreground equal to 1.
Return Value: void
*/
void deleteNodeForeground(struct list **head_ref, int foregroundData) {
	struct list *curr = *head_ref, *prev;
 
    // if head is the key
    if (curr != NULL && curr->foreground == foregroundData) {
        *head_ref = curr->next;
        free(curr);
        return;
    }
 
    //search for key
    while (curr != NULL && curr->foreground != foregroundData) {
        prev = curr;
        curr = curr->next;
    }
 
    //search for key
    if (curr == NULL)
        return;
 
    // Unlink the node from linked list
    prev->next = curr->next;
    free(curr); // free node
}
//https://www.geeksforgeeks.org/linked-list-set-3-deleting-node/
/*-------------------------------------------------*/
/*
Parameters: int signo
Description: Function called when SIGINT event occurs.
Return Value: void
*/
/* Our signal handler for SIGINT */
void handle_SIGINT(int signo){
	char* message = "\nCaught SIGINT\n: ";
  // We are using write rather than printf
	write(STDOUT_FILENO, message, 18);
	fflush(stdout);
}
/*-------------------------------------------------*/
/*
Parameters: int signo
Description: Function called when SIGTSTP event occurs.
Return Value: void
*/
void handle_SIGTSTP(int signo){
	char* backgroundTrue = "\nExiting foreground-only mode.\n: ";
	char* backgroundFalse = "\nEntering foreground only-mode.\n: ";
	allowBackground = !allowBackground;

	if(allowBackground)
		write(STDOUT_FILENO, backgroundTrue, 35);
	else
		write(STDOUT_FILENO, backgroundFalse, 35);
	fflush(stdout);
}
/*-------------------------------------------------*/
/*
Parameters: none
Description: Function that sets flags and handles for SIGINT and SIGTSTP calls.
Return Value: void
*/
void catchInterrupt() {
	// Initialize SIGINT_action struct to be empty
	struct sigaction SIGINT_action = {0};
	struct sigaction SIGTSTP_action = {0};

	// Fill out the SIGINT_action struct
	// Register handle_SIGINT as the signal handler
		SIGINT_action.sa_handler = handle_SIGINT;
		SIGTSTP_action.sa_handler = handle_SIGTSTP;
	// Block all catchable signals while handle_SIGINT is running
		sigfillset(&SIGINT_action.sa_mask);
		sigfillset(&SIGTSTP_action.sa_mask);
		
		// sigaddset(&SIGINT_action.sa_mask, SIGINT);
		// sigaddset(&SIGTSTP_action.sa_mask, SIGTSTP);
	// No flags set
		SIGINT_action.sa_flags = SA_RESTART;
		SIGTSTP_action.sa_flags = SA_RESTART;

	// Install our signal handler
		sigaction(SIGINT, &SIGINT_action, NULL);
		sigaction(SIGTSTP, &SIGTSTP_action, NULL);
		//SIGTSTP not being handled
		

}
/*-------------------------------------------------*/
/*
Parameters: void
Description: Waits for user to input then returns c-string
Return Value: char*(c-string)
*/
char *shellReadline(void) {
	char *line = NULL;
	ssize_t bufsize = 2048; // have getline allocate a buffer for us

	fflush(stdout);
	if (getline(&line, &bufsize, stdin) == -1){
		if (feof(stdin)) {
			exit(EXIT_SUCCESS);  // We recieved an EOF
		} else  {
			clearerr(stdin);
			perror("readline");
		}
	}
	
	return line;
}
/*-------------------------------------------------*/
/*
Parameters: char*(c-string)
Description: Takes c-string and separates strings based on spaces
Return Value: array of char*(c-string)
*/
#define LSH_TOK_BUFSIZE 512
#define LSH_TOK_DELIM " \t\r\n\a"
char **shellGetArgs(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
	fprintf(stderr, "shell: allocation error\n");
	exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
	tokens[position] = token;
	position++;

	if (position >= bufsize) {
	  bufsize += LSH_TOK_BUFSIZE;
	  tokens = realloc(tokens, bufsize * sizeof(char*));
	  if (!tokens) {
		fprintf(stderr, "shell: allocation error\n");
		exit(EXIT_FAILURE);
	  }
	}

	token = strtok(NULL, LSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}
/*-------------------------------------------------*/
/*
Parameters: char**(array of c-string), int outputFIle, int inputFile
Description: Processes arguements and finds if < and > characters are present. If they are perform redirection.
Return Value: array of char*(file for input, file opened for output)
*/
char** getRedirectArgs(char **args, int *outputFile, int *inputFile) {
	char** redirArgs = malloc(2 * sizeof(char*));
	*outputFile = NULL;
	*inputFile = NULL;
	redirArgs[0] = NULL;
	redirArgs[1] = NULL;
	struct stat dirStat;

	int curr = 0;
	while(args[curr] != NULL) {
		if(args[curr] == NULL) {
			curr++;
			continue;
		}
		
		if(strcmp(args[curr], "<") == 0) {
			redirArgs[1] = args[curr+1];
			// printf("inputFile: %s\n", redirArgs[0]);
			*inputFile = open(redirArgs[1], O_RDONLY);
			if (*inputFile == -1) {
				perror("open()");
				redirArgs[1] = NULL;
				*inputFile = NULL;
				args[curr] = "";
				curr++;
				continue;
			}
			stat(redirArgs[1], &dirStat);
        	if(dirStat.st_size <= 0) {
				printf("Error: empty file.\n");
				redirArgs[1] = NULL;
				*inputFile = NULL;
				args[curr] = "";
				args[curr+1] = "";
				curr += 2;
				continue;
			}

			args[curr] = NULL;
			args[curr+1] = NULL;
			curr += 2;
			continue;
		}
		if(strcmp(args[curr], ">") == 0) {
			redirArgs[0] = args[curr+1];
			*outputFile = open(redirArgs[0], O_WRONLY | O_CREAT | O_TRUNC, 0640);
			if (*outputFile == -1)
				perror("open()");
			// printf("outputFile: %s\n", redirArgs[0]);
			args[curr] = NULL;
			args[curr+1] = NULL;
		}
		curr++;
		
	}
	return redirArgs;
}
/*-------------------------------------------------*/
/*
Parameters: char**(arguements), list of background processes, int foreground(create proc in fg or bg)
Description: Forks the current process uses exec to run new command(either in foreground or background)
Return Value: int 1(run) or 0(exit)
*/
int shellCreateProcess(char **args, struct list **processList, int foreground)
{
	pid_t pid, wpid;
	int status;
	int outputFile, inputFile;
	char **redirArgs = getRedirectArgs(args, &outputFile, &inputFile);
	int mySTDOUT = dup(1);
	int mySTDIN = dup(0);
	int result, resultTwo;
	int fd;

	// printf("redirArg0: %s fd: %d\n", redirArgs[0], outputFile);
	// printf("redirArg1: %s fd: %d\n", redirArgs[1], inputFile);
	fflush(stdout);
	if(redirArgs[0] != NULL) {
		//do /dev/null redir for bground proc
		//implement input redir
		result = dup2(outputFile, 1);
		if (result == -1) {
			perror("output dup2");
			fflush(stdout);
		}
		close(outputFile);
	}
	if(redirArgs[1] != NULL) {
		resultTwo = dup2(inputFile, 0);
		if (resultTwo == -1) {
			perror("input dup2");
			fflush(stdin);
		}
		close(inputFile);
	}

	pid = fork();
	if (pid == 0) {
		// Child process
		if(resultTwo > 0) {
			  execlp(args[0], args[0], NULL);
		} else {
			if (execvp(args[0], args) == -1) {
				perror("shell");
			}
			exit(EXIT_FAILURE);
		}

	} else if (pid < 0) {
		// Error forking
		perror("shell");
	} else {
		// Parent process
		if(!foreground) {
			push(processList, pid, status, 0);
			wpid = waitpid(pid, status, WNOHANG);
			printf("Child background pid: %d \n", pid);
			fflush(stdin);
			fflush(stdout);
			return 1;
		} else {
			do {
				deleteNodeForeground(processList, 1);
				push(processList, pid, status, 1);
				wpid = waitpid(pid, &status, WUNTRACED);
				fflush(stdout);
				fflush(stdin);
			} while (!WIFEXITED(status) && !WIFSIGNALED(status));
		}
	}

	if(result > 0) close(result);
	if(resultTwo > 0) close(resultTwo);
	dup2(mySTDOUT, 1);
	dup2(mySTDIN, 0);
	printf("\n");
	fflush(stdout);
	return 1;
}
/*-------------------------------------------------*/
/*
Parameters: list**(fg and bg processes)
Description: Checks whether any background process has finished running.
Return Value: void
*/
void shellCheckBackground(struct list **processList) {
	if(processList == NULL)
		return;

	pid_t wpid;
	int childStatus;
	struct list **head_ref = processList;
	struct list *curr = *processList;

	while(curr != NULL) {
		wpid = waitpid(curr->pid, &childStatus, WNOHANG);
		//if proc running or was foreground(status) proc = skip
		if(wpid == 0 || curr->foreground) {
			curr = curr->next;
			continue;
		}
		
		if(WIFSIGNALED(childStatus)) {
			printf("Child %d exited abnormally due to signal %d\n", 
			wpid, WTERMSIG(childStatus));
			deleteNode(head_ref, curr->pid);
		} else if(WIFEXITED(childStatus)){
			printf("Child %d exited normally with status %d\n", 
			wpid, WEXITSTATUS(childStatus));
			deleteNode(head_ref, curr->pid);
		}
		curr = curr->next;
	}
	return;
}
/*-------------------------------------------------*/
/*
Parameters: input string, target subString, string to replace target
Description: Replaces all occurences of a string within a substring
Return Value: new string(char*)
*/
char* replaceWord(const char* s, const char* oldW, const char* newW) {
    char* result;
    int i, cnt = 0;
    int newWlen = strlen(newW);
    int oldWlen = strlen(oldW);
    // count occurences of word in string
    for (i = 0; s[i] != '\0'; i++) {
        if (strstr(&s[i], oldW) == &s[i]) {
            cnt++;
            i += oldWlen - 1;
        }
    }
    // re-size new string
    result = (char*)malloc(i + cnt * (newWlen - oldWlen) + 1);
    i = 0;
    while (*s) {
        // compare the substring with the result
        if (strstr(s, oldW) == s) {
            strcpy(&result[i], newW);
            i += newWlen;
            s += oldWlen;
        }
        else
            result[i++] = *s++;
    }
    result[i] = '\0';
    return result;
}
//https://www.geeksforgeeks.org/c-program-replace-word-text-another-given-word/
/*-------------------------------------------------*/
/*
Parameters: char**(arguements)
Description: Checks whether the bg modifier(&) or process PID($$) modifier is present
Return Value: run background or foreground process(int)
*/
//returns 1 if process is foreground, 0 if background
int shellCheckModifiers(char** args) {
	int size = 0;
	while(args[size] != NULL) size++;
	int lastIndex = size-1;
	// printf("size: %d\n", size);

	for(int i = 0; i < size; i++) {
		if(args[i] == NULL)
			continue;

		if(strstr(args[i], "$$") != NULL) {
			pid_t myPid = getpid();
			char pidString[10];
			sprintf(pidString, "%d", getpid());
			args[i] = replaceWord(args[i], "$$", pidString);
		}
	}
	if(args[lastIndex] == NULL) return 1;
	if((strcmp(args[lastIndex], "&") == 0)) {
		if(!allowBackground) {
			args[lastIndex] = NULL;
			return 1;
		}
		args[lastIndex] = NULL;
		return 0;
	}
  return 1;
}
/*-------------------------------------------------*/
int shellCD(char** args);
int shellStatus(char** args, struct list **processList);
int shellExit(char** args);
/*-------------------------------------------------*/
/*
Parameters: char**(arguement)
Description: Chooses a function to run based on an index passed(either shellCD, shellStatus or shellExit)
Return Value: index(int)
*/
int (*builtin_func[]) (char **) = {
  &shellCD,
  &shellStatus,
  &shellExit
};
/*-------------------------------------------------*/
int lsh_num_builtins() {
	char *builtin_str[] = {
		"cd",
		"status",
		"exit"
	};
  return sizeof(builtin_str) / sizeof(char *);
}
/*-------------------------------------------------*/
/*
Parameters: char**(arguements)
Description: 
Return Value: 
*/
int cmdHandler(char** args, struct list **processList) {
	if(args[0] == NULL)
		return 1;
	//midline comments not supported
	//check first char not first string
	if(strstr(args[0], "#") != NULL)
		return 1;

	char *builtin_str[] = {
		"cd",
		"status",
		"exit"
	};

	int i;
	for (i = 0; i < lsh_num_builtins(builtin_str); i++) {
		if (strcmp(args[0], builtin_str[i]) == 0) {
			if(i == 1)
				return(shellStatus(args, processList));
			else
		  		return (*builtin_func[i])(args);
		}
	}

	// int *targetFD = NULL;
	int foreground = shellCheckModifiers(args);
	return shellCreateProcess(args, processList, foreground);
}
/*-------------------------------------------------*/
/*
LOCAL COMMANDS
*/
/*-------------------------------------------------*/
/*
Parameters: char**(arguements)
Description: Changes directory to arguement passed, or $HOME if no args passed
Return Value: int(1)
*/
int shellCD(char** args) {
	//
	char *homedir = getenv("HOME");
	if (args[1] == NULL) {
		// fprintf(stderr, "shell: expected argument to \"cd\"\n");
		args[1] = homedir;
	}
	if (chdir(args[1]) != 0) {
		perror("shell");
	}

	return 1;
}
/*-------------------------------------------------*/
/*
Parameters: char**(arguements), list**(process list)
Description: Checks node with foreground==1 and checks exit status of process
Return Value: int(1)
*/
int shellStatus(char** args, struct list **processList) {
	struct list *curr = *processList;
	if(curr == NULL) {
		printf("exit status 0\n");
		return 1;
	}

	//loop list 
	//check if curr is fground
	//	print
	//
	//remove prev fground on createProcess then add new fground
	int foregroundStatus;
	while(curr != NULL) {
		if(!curr->foreground) {
			curr = curr->next;
			continue;
		}
		foregroundStatus = curr->status;

		if(WIFEXITED(foregroundStatus)){
			printf("Child %d exited normally with status %d\n", 
			curr->pid, WEXITSTATUS(foregroundStatus));
			return 1;
		} else {
			printf("Child %d exited abnormally due to signal %d\n", 
			curr->pid, WTERMSIG(foregroundStatus));
			return 1;
		}

		curr = curr->next;
	}
	return 1;
}
/*-------------------------------------------------*/
/*
Parameters: char**(args)
Description: Exits the process
Return Value: int(0)
*/
int shellExit(char** args) {
	return 0;
}
/*-------------------------------------------------*/
int main(int argc, char *argv[]) {

	char* line;
	char** args;
	int status = 1;
	struct list* head = NULL;

	catchInterrupt();
	do {
		shellCheckBackground(&head);
		printf(": ");
		line = shellReadline();
		args = shellGetArgs(line);
		status = cmdHandler(args, &head);

		free(line);
		free(args);
		//free the proc list here 
	} while(status);

	return 0;
}
