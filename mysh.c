#define _POSIX_C_SOURCE 200809L//turn on posix features for read() and isatty()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef struct{
    char **items;
    int length;
    int capacity;
}strlist; //hello anagha ajesh, this is a struct w/ name strlist kinda like an arraylist for strings

static void stringlist_initialize(strlist *list) {
    list->length = 0;
    list->capacity = 8; 
    list->items = malloc(list->capacity * sizeof(char *));
    if (list->items == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
}

//adding new item to the "arraylist" and reaccolacting if capacity <-
static void stringlist_push(strlist *list, char *item) {
    if (list->length >= list->capacity) {
        list->capacity *= 2;
        char **new_items = realloc(list->items, list->capacity * sizeof(char *));
        if (new_items == NULL) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        list->items = new_items;
    }
    list->items[list->length] = item;
    list->length++;
}

static void stringlist_free(strlist *list) {
    for (int i = 0; i < list->length; i++) {
        free(list->items[i]);
    }
    free(list->items);
}

//tokenizer implementation yaya
static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n';
}

//split the tlines into tokens based on whitespace and pushes a malloc'd copy of each one onto tokens.
static void tokenize(const char *line, strlist *tokens) {
    int i = 0;
    while(1){
        while (is_space(line[i])) {
            i++;
        }
        if(line[i] == '\0' || line[i] == '\n'){
            break;
        }
        int start = i;
        while (line[i] != '\0' && line[i] != '#' && !is_space(line[i])) {
            i++;
        }
        char *word = strndup(line + start, i - start);

        if(word == NULL){
            perror("strndup");
            exit(EXIT_FAILURE);
        }

        if (line[i] == '#') {
            break;
        }
        
        stringlist_push(tokens, word);

    }
}

//finding and running the programs yippee
static const char *search_dirs[] = {"/usr/local/bin", "/usr/bin", "/bin"};
static const int num_search_dirs = 3; //I know I shouldnt be hardcoding values but oh well
static char *search_path(const char *command) {
    for (int i = 0; i < num_search_dirs; i++) {
        size_t size = (strlen(search_dirs[i]) + 1 + strlen(command) + 1);
        char *path = malloc(size);
        if (path == NULL){
            perror("malloc");
            exit(EXIT_FAILURE); 
        }
        snprintf(path, size, "%s/%s", search_dirs[i], command);
        if (access(path, X_OK) == 0) {
            return path;
        }
        free(path);
    }
    return NULL;
}

static void execute_command(strlist *tokens) {

    // don't execute anything if the command line input was empty
    if (tokens->length == 0) {
        return;
    }

    //command is first item of tokens array
    char *command = tokens->items[0];
    char *path;

    //if command contains '/', use it as a direct path or search specified directories
    if (strchr(command, '/') != NULL) {
        path = strdup(command);
    } else {
        path = search_path(command);
    }

    if (path == NULL) {
        fprintf(stderr, "%s: command not found\n", command);
        return;
    }

    //create a null-terminated argument array for execv()
    char **args = malloc((tokens->length + 1) * sizeof(char *));

    if (args == NULL) {
        perror("malloc");
        free(path);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < tokens->length; i++) {
        args[i] = tokens->items[i];
    }

    args[tokens->length] = NULL;

    // create a child process
    /*negative: gulp somethins wrong
    0:child process
    positive: parent process*/ 
    pid_t pid = fork();

    //error w fork
    if (pid < 0) {
        perror("fork");
    }
    //child process
    else if (pid == 0) {
        execv(path, args);

        // Only reached if execv fails
        perror("execv");
        _exit(EXIT_FAILURE);
    }
    //parent process aka pid > 0 positive
    else {
        // PARENT PROCESS: wait for child to finish
        waitpid(pid, NULL, 0);
    }

    free(args);
    free(path);
}

int main(int argc, char *argv[]) {

    int input_fd = STDIN_FILENO;

    //check argument nums -> assignment only allows 0 or 1 arguments
    if (argc > 2) {
        fprintf(stderr, "Usage: ./mysh [file]\n");
        return EXIT_FAILURE;
    }

    // If a file is provided, open it.
    if (argc == 2) {
        input_fd = open(argv[1], O_RDONLY);

        if (input_fd == -1) {
            perror("mysh");
            return EXIT_FAILURE;
        }
    }

    // determines interactively or batch mode -> default batch mode
    //interactive mode is if no file was specified or our input is comin from the terminal
    int interactive = (argc == 1 && isatty(input_fd));

    if (interactive) {
        printf("Welcome to my shell!\n");
    }

    while (1) {

        // Print prompt if interactive, no if batch
        if (interactive) {
            printf("$ ");
            fflush(stdout);
        }

        
        // Allocate memory for reading the command line! can be reallocated later
        size_t capacity = 128;
        size_t length = 0;
        int eof = 0;
        int error = 0;

        char *line = malloc(capacity);

        //if malloc fails we break!! maybe store more or less idk
        if (line == NULL) {
            perror("malloc");
            break;
        }

        // read one character of the command line input at a time with the loop
        //this is where we use read()

        while (1) {

            char c;
            ssize_t bytes = read(input_fd, &c, 1);

            //return values of read(): negative one is an error, zero means reached eof
            //aka end of file so break, positive means it was successfull. when /n is reached,
            //we have to end it immediately to execute command -> execution obv implemented later

            if (bytes == -1) {
                perror("read");
                error = 1;
                break;
            }

            /*break rn but check length of line later to make sure we didnt miss any last 
            commands withotu new line*/
            if (bytes == 0) {
                eof = 1;
                break;
            }

            if (c == '\n') {
                break;
            }

            // increase capacity for line when necessary -> reallocation if C string 
            //memory too small
            if (length + 1 >= capacity) {
                capacity *= 2;

                char *temp = realloc(line, capacity);

                if (temp == NULL) {
                    perror("realloc");
                    error = 1;
                    break;
                }

                line = temp;
            }

            line[length] = c;
            length++;
        }

        line[length] = '\0';

        /* temporarily print line (testing)
        if (!error && (length > 0 || !eof)) {
            printf("Received command: %s\n", line);
        }

        //testing tokenizer
        if (!error && (length > 0 || !eof)) {
            strlist tokens;
            stringlist_initialize(&tokens);

            tokenize(line, &tokens);

            for (int i = 0; i < tokens.length; i++) {
                printf("Token %d: %s\n", i, tokens.items[i]);
            }

            stringlist_free(&tokens);
        } */

        if (!error && (length > 0 || !eof)) {
            strlist tokens;
            stringlist_initialize(&tokens);
            tokenize(line, &tokens);

            execute_command(&tokens);

            stringlist_free(&tokens);
        }

        free(line);

        //error hnadling
        if (eof || error) {
            break;
        }

    }

    if (interactive) {
        printf("Exiting my shell.\n");
    }

    //close file if opened
    if (argc == 2) {
        close(input_fd);
    }

    return EXIT_SUCCESS;
}
