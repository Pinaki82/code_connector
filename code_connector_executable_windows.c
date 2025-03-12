// code_connector_executable_windows.c (or .cpp if renamed)
#include "code_connector_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int main(int argc, char *argv[]) {
  //log_message("Starting code_connector_executable (Windows)");
  if(argc < 4) {
    fprintf(stderr, "Usage: %s <file> <line> <column>\n", argv[0]);
    log_message("Invalid arguments provided");
    return 1;
  }

  const char *filename = argv[1];
  int line = atoi(argv[2]);
  int column = atoi(argv[3]);
  char input[1024];
  snprintf(input, sizeof(input), "%s %d %d", filename, line, column);
  //log_message("Input constructed: "); // DEBUG
  //log_message(input); // DEBUG
  char *result = processCompletionDataFromString(input);

  if(result) {
    //log_message("Completion result obtained: ");
    //log_message(result);
    printf("%s", result);
    free(result);
    //log_message("Program completed successfully");
    return 0;
  }

  else {
    //log_message("processCompletionDataFromString returned NULL");
    fprintf(stderr, "Error: No completion data generated\n");
    return 1;
  }
}
