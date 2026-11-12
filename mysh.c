#define _POSIX_C_SOURCE 200809L//turn on posix features for read() and isatty()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>

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

//wildcard handling -> check whether a filename matches the prefix*suffix pattern

static int wildcard_match(const char *name, const char *prefix, const char *suffix) {
    size_t name_len = strlen(name);
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);

    // filename must be long enough for both parts
    if (name_len < prefix_len + suffix_len) {
        return 0;
    }

    if (strncmp(name, prefix, prefix_len) != 0) {
        return 0;
    }

    if (strcmp(name + name_len - suffix_len, suffix) != 0) {
        return 0;
    }

    return 1;
}

// expand the wildcard * into matching filenames
static void expand_wildcard(const char *token, strlist *expanded) {
    char *star = strchr(token, '*');

    //if no wildcard, keep the original token
    if (star == NULL) {
        stringlist_push(expanded, strdup(token));
        return;
    }

    if (strchr(star + 1, '*') != NULL) {
        stringlist_push(expanded, strdup(token));
        return;
    }

    const char *slash = strrchr(token, '/');

    if (slash != NULL && star < slash) {
        stringlist_push(expanded, strdup(token));
        return;
    }

    const char *filename = (slash == NULL) ? token : slash + 1;

    char *prefix = strndup(filename, star - filename);
    char *suffix = strdup(star + 1);

    char *directory;

    if (slash == NULL) {
        directory = strdup(".");
    } else if (slash == token) {
        directory = strdup("/");
    } else {
        directory = strndup(token, slash - token);
    }

    DIR *dir = opendir(directory);

    // if directory cant be opened, keep the original token
    if (dir == NULL) {
        stringlist_push(expanded, strdup(token));
        free(prefix);
        free(suffix);
        free(directory);
        return;
    }

    struct dirent *entry;
    int matches = 0;

    while ((entry = readdir(dir)) != NULL) {
        char *name = entry->d_name;

        // patterns beginning with * must not match hidden files
        if (filename[0] == '*' && name[0] == '.') {
            continue;
        }

        if (wildcard_match(name, prefix, suffix)) {
            char *match;

            if (slash == NULL) {
                match = strdup(name);
            } else {
                size_t path_len = slash - token + 1;
                match = malloc(path_len + strlen(name) + 1);

                memcpy(match, token, path_len);
                strcpy(match + path_len, name);
            }

            stringlist_push(expanded, match);
            matches++;
        }
    }

    closedir(dir);

    // if nothing matched, pass the original token unchanged
    if (matches == 0) {
        stringlist_push(expanded, strdup(token));
    }

    free(prefix);
    free(suffix);
    free(directory);
}

//executes all the cd pwd commands etc
static int execute_builtin(strlist *tokens) {
    char *command = tokens->items[0];

    // cd implementation aka change the shell's working directory
    if (strcmp(command, "cd") == 0) {
        if (tokens->length > 2) {
            fprintf(stderr, "cd: too many arguments\n");
            return 0;
        }

        char *directory;

        if (tokens->length == 1) {
            directory = getenv("HOME");
            if (directory == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 0;
            }
        } else {
            directory = tokens->items[1];
        }

        if (chdir(directory) == -1) {
            perror("cd");
        }

        return 0;
    }

    // pwd implementation aka print the current workin directory
    if (strcmp(command, "pwd") == 0) {
        char cwd[PATH_MAX];

        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("pwd");
        } else {
            printf("%s\n", cwd);
        }

        return 0;
    }

    // which implementaiton aka find the executable path of a command
    if (strcmp(command, "which") == 0) {
        for (int i = 1; i < tokens->length; i++) {
            char *path = search_path(tokens->items[i]);

            if (path == NULL) {
                fprintf(stderr, "%s: not found\n", tokens->items[i]);
            } else {
                printf("%s\n", path);
                free(path);
            }
        }

        return 0;
    }

    // exit aka tell the shell to terminate
    if (strcmp(command, "exit") == 0) {
        return 1;
    }

    return -1;
}

//process organization, args allocation, redirection stuf
static int execute_command(strlist *tokens) {

    if (tokens->length == 0) {
        return 0;
    }

    int result = execute_builtin(tokens);

    if (result != -1) {
        return result;
    }

    char *input_file = NULL;
    char *output_file = NULL;
    int append = 0;

    char **args = malloc((tokens->length + 1) * sizeof(char *));
    if (args == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int arg_count = 0;

    for (int i = 0; i < tokens->length; i++) {

        if (strcmp(tokens->items[i], "<") == 0) {
            if (i + 1 >= tokens->length) {
                fprintf(stderr, "Missing input filename\n");
                free(args);
                return 0;
            }

            input_file = tokens->items[++i];
        }

        else if (strcmp(tokens->items[i], ">") == 0) {
            if (i + 1 >= tokens->length) {
                fprintf(stderr, "Missing output filename\n");
                free(args);
                return 0;
            }

            output_file = tokens->items[++i];
            append = 0;
        }

        else if (strcmp(tokens->items[i], ">>") == 0) {
            if (i + 1 >= tokens->length) {
                fprintf(stderr, "Missing output filename\n");
                free(args);
                return 0;
            }

            output_file = tokens->items[++i];
            append = 1;
        }

        else {
            args[arg_count] = tokens->items[i];
            arg_count++;
        }
    }

    args[arg_count] = NULL;

    //cld be null if someone only enters a redirection
    if (arg_count == 0) {
        fprintf(stderr, "Missing command\n");
        free(args);
        return 0;
    }

    // Expand wildcards in all command arguments
    strlist expanded;
    stringlist_initialize(&expanded);

    for (int i = 0; i < arg_count; i++) {
        expand_wildcard(args[i], &expanded);
    }

    // replace og args array with the expanded args
    free(args);

    arg_count = expanded.length;
    args = malloc((arg_count + 1) * sizeof(char *));

    if (args == NULL) {
        perror("malloc");
        stringlist_free(&expanded);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < arg_count; i++) {
        args[i] = expanded.items[i];
    }

    args[arg_count] = NULL;

    //command is first item of tokens array
    char *command = args[0];
    char *path;

    //if command contains '/', use it as a direct path or search specified directories
    if (strchr(command, '/') != NULL) {
        path = strdup(command);
    } else {
        path = search_path(command);
    }

    if (path == NULL) {
        fprintf(stderr, "%s: command not found\n", command);
        free(args);
        stringlist_free(&expanded);
        return 0;
    }

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
        // Handle input redirection
        if (input_file != NULL) {
            int fd = open(input_file, O_RDONLY);

            if (fd == -1) {
                perror("open input");
                _exit(EXIT_FAILURE);
            }

            if (dup2(fd, STDIN_FILENO) == -1) {
                perror("dup2 input");
                _exit(EXIT_FAILURE);
            }

            close(fd);
        }

        // handle output redirection
        if (output_file != NULL) {
            int flags = O_WRONLY | O_CREAT;

            if (append) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }

            int fd = open(output_file, flags, S_IRUSR | S_IWUSR | S_IRGRP);

            if (fd == -1) {
                perror("open output");
                _exit(EXIT_FAILURE);
            }

            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2 output");
                _exit(EXIT_FAILURE);
            }

            close(fd);
        }

        execv(path, args);
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
    stringlist_free(&expanded);

    return 0;
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

        int should_exit = 0;

        if (!error && (length > 0 || !eof)) {
            strlist tokens;
            stringlist_initialize(&tokens);
            tokenize(line, &tokens);

            should_exit = execute_command(&tokens);

            stringlist_free(&tokens);
        }

        free(line);

        //error hnadling
        if (eof || error || should_exit) {
            break;
        }

    }

    if (interactive) {
        printf("Exiting my shell.\n");
    }

    if (argc == 2) {
        close(input_fd);
    }

    return EXIT_SUCCESS;
}
