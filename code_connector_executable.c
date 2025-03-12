#include "code_connector_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

int main(int argc, char *argv[]) {
  if(argc != 4) {
    fprintf(stderr, "Usage: %s <filename> <line> <column>\n", argv[0]);
    return 1;
  }

  const char *filename = argv[1];
  int line = atoi(argv[2]);
  int column = atoi(argv[3]);
  // Combine the input into a single string in the format "/path/to/file.extension line column"
  const int bufferSize = strlen(filename) + 50; // Assuming line and column will not exceed 10 characters each
  char *combinedInput = malloc(bufferSize);

  if(combinedInput == NULL) {
    fprintf(stderr, "Memory allocation failed.");
    return 1;
  }

  sprintf(combinedInput, "%s %d %d", filename, line, column);
  // Getting the result of the variable combinedInput
  // printf("Input string for the filename: %s\n", combinedInput); // For Testing.
  // Getting the result of the variable argv[1]
  // printf("Input filename: %s\n", argv[1]); // For Testing.
  char *result = processCompletionDataFromString(combinedInput);

  if(!result) {
    fprintf(stderr, "fn processCompletionDataFromString: Failed to process input string.\n");
    free(combinedInput);
    return 1;
  }

  // Print the result stored in the global buffer
  if(strlen(result) > 0) {
    // printf("%s", result);
  }
  else {
    printf("Failed to process input string.\n");
    free(combinedInput);
    return 1;
  }

  // Filtration part's varibales
  FILE *file = fopen(filename, "r");
  char *source = NULL; // Content of the line number of the file submitted.
  size_t len = 0;
  int currentLine = 0;

  if(file) {
    while(getline(&source, &len, file) != -1) {
      currentLine++;

      if(currentLine == line) {
        break;
      }
    }

    fclose(file);
  }

  // source now contains the line content
  //  printf("Line %d: %s", line, source);
  const char *pattern = result;
  char *substituted_result = substitute_function_pattern(source, pattern);

  if(!result) {
    fprintf(stderr, "Error: Failed to substitute pattern\n");
    free(source);
    free(substituted_result);  // Free the allocated memory
    free(combinedInput);
    return 1;
  }

  printf("%s", substituted_result);
  free(source);
  free(substituted_result);  // Free the allocated memory
  free(combinedInput);
  return 0;
}
