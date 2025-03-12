// Last Change: 2025-03-13  Thursday: 12:56:17 AM
#include "code_connector_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#define VERSION "1.0" /* defines a constant string called "VERSION" with the value 1.0 */
#define NO_OF_ARGS 2 /* the exact no. of command-line arguments the program takes */
int main(int argc, char *argv[]) { /* The Main function. argc means the number of arguments. argv[0] is the program name. argv[1] is the first argument. argv[2] is the second argument and so on. */
  if(argc == 2 && strcmp(argv[1], "--version") == 0) { /* checks if the program was called with the argument "--version". If it was, it prints out the value of the "VERSION" constant and returns 0. Otherwise, it slides down to the next `else if()` block. */
    printf("%s\n", VERSION);
    return 0;
  }

  if(argc != NO_OF_ARGS) {  /* Checks if the program was called with the exact number of arguments. If it wasn't, it prints out an error message and returns 1. */
    fprintf(stderr, "Usage: %s <dir> or, %s --version\n", argv[0], argv[0]);
    log_message("Invalid arguments provided");
    return 1;
  }

  /* After performing all checks, your code starts here */
  const char *filename = argv[1];
  char input[1024];
  snprintf(input, sizeof(input), "%s", filename);
  int result = execute_ccls_index(filename);

  if(result) {
    return 0;
  }

  else {
    log_message("execute_ccls_index returned NULL");
    fprintf(stderr, "Error: No completion data generated\n");
    return 1;
  }

  return 0;
}

