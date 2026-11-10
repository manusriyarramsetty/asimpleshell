#define _POSIX_C_SOURCE 200809L//turn on posix features for read() and isatty()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

/*hello manu. i implemented main, aka im reading the command line input and making sure that
the user inputted the correct number of arguments (0 or 1), and then i also implemented
the check on whether mysh is running interactive mode or batch mode. I also made the loop
that reads the command line input. This basically calls read() to read each individual char
in the input and stores it to a C string and ends it/records it based on the characters read.
i added comments so it makes more snese in the code idk. basically ur next step shld be
to break that C string that i had the loop build for us into each separate command/arg
and start goin into the actual pipes and processes or whatever. 
*/

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

        // temporarily print line (testing)
        if (!error && (length > 0 || !eof)) {
            printf("Received command: %s\n", line);
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
