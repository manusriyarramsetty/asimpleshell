
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

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

    // determines interactively or batch mode
    int interactive = (argc == 1 && isatty(input_fd));

    if (interactive) {
        printf("Welcome to my shell!\n");
    }

    while (1) {

        // Print prompt if interactive.
        if (interactive) {
            printf("$ ");
            fflush(stdout);
        }

        break;
    }

    //exit message
    if (interactive) {
        printf("Exiting my shell.\n");
    }

    //close file if opened
    if (argc == 2) {
        close(input_fd);
    }

    return EXIT_SUCCESS;
}
