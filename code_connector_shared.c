// Last Change: 2025-03-03  Monday: 01:39:32 PM
#include "code_connector_shared.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <regex.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

// Add at the top of the file, after includes
static CodeCompletionCache completion_cache;

// OS detection macro

#if defined(_WIN32)
  #define OS_WINDOWS
#elif defined(__linux__)
  #define OS_LINUX
#elif defined(__APPLE__)
  #define OS_MACOS
#endif

#if !defined(OS_WINDOWS) && (defined(OS_LINUX) || !defined(OS_MACOS))
  #define  _POSIX_C_SOURCE 200809L
  #define  _XOPEN_SOURCE 500L
#endif

// Global buffer to store the result
char global_result_buffer[MAX_OUTPUT];

// Global buffer to monitor project directory changes
char global_buffer_project_dir_monitor[PATH_MAX];
// If the project directory changes, and integer value will hold 1 insteadof the default value 0
int global_project_dir_monitor_changed = 0;
// Global buffer to store the current file path
char global_buffer_current_file_dir[PATH_MAX];
// Global buffer to store the project directory
char global_buffer_project_dir[PATH_MAX];
// Global buffer to store the header file paths for the current project obtained from .ccls and compile_flags.txt files
char global_buffer_header_paths[MAX_LINES][MAX_PATH_LENGTH];
// Global buffer to store the CPU architecture
char global_buffer_cpu_arc[MAX_OUTPUT];

// Global buffer to store the header file paths and CPU architecture (temporary)
// char global_buffer_header_paths_cpu_arc[MAX_OUTPUT];

/*
  To avoid recalculation, we will be caching ceratin information, such as include paths, project directory, of the CPU architecture detected by the LLVM (namely, Clang here) etc.
  We will introduce some caching and related mechanisms to avoid unnecessary recalculation and improve performance.
  This includes some functions and global variables.
*/

// Initialize the cache
void init_cache(void) {
  memset(&completion_cache, 0, sizeof(CodeCompletionCache));
  completion_cache.is_valid = 0;
  completion_cache.include_path_count = 0;
}

// Clear the cache
void clear_cache(void) {
  // Free any allocated include paths
  for(int i = 0; i < completion_cache.include_path_count; i++) {
    if(completion_cache.include_paths[i]) {
      free(completion_cache.include_paths[i]);
      completion_cache.include_paths[i] = NULL;
    }
  }

  completion_cache.include_path_count = 0;
  completion_cache.is_valid = 0;
  memset(completion_cache.project_dir, 0, PATH_MAX);
  memset(completion_cache.cpu_arch, 0, MAX_LINE_LENGTH);
}

// Check if cache is valid for current project
int is_cache_valid(const char *current_project_dir) {
  if(!completion_cache.is_valid || !current_project_dir) {
    return 0;
  }

  // Compare current project directory with cached project directory
  char resolved_current[PATH_MAX];

  if(realpath(current_project_dir, resolved_current) == NULL) {
    return 0;
  }

  return strcmp(resolved_current, completion_cache.project_dir) == 0;
}

// Update the cache with new values
void update_cache(const char *project_dir, char **include_paths, int path_count, const char *cpu_arch) {
  clear_cache();  // Clear existing cache

  // Store project directory
  if(realpath(project_dir, completion_cache.project_dir) == NULL) {
    completion_cache.is_valid = 0;
    return;
  }

  // Store include paths
  completion_cache.include_path_count = (path_count > MAX_CACHED_PATHS) ? MAX_CACHED_PATHS : path_count;

  for(int i = 0; i < completion_cache.include_path_count; i++) {
    completion_cache.include_paths[i] = strdup(include_paths[i]);

    if(!completion_cache.include_paths[i]) {
      clear_cache();
      return;
    }
  }

  // Store CPU architecture
  strncpy(completion_cache.cpu_arch, cpu_arch, MAX_LINE_LENGTH - 1);
  completion_cache.cpu_arch[MAX_LINE_LENGTH - 1] = '\0';
  completion_cache.is_valid = 1;
}

// Function to get cached include paths
char **get_cached_include_paths(int *count) {
  if(!completion_cache.is_valid) {
    *count = 0;
    return NULL;
  }

  *count = completion_cache.include_path_count;
  return completion_cache.include_paths;
}

// Function to create default .ccls and compile_flags.txt files
// Returns 0 on success, 1 on failure
int create_default_config_files(const char *directory) {
  // Calculate required buffer sizes (including null terminator)
  size_t dir_len = strlen(directory);
  // PATH_MAX is typically defined, but we'll add extra space for safety
  size_t max_path_len = dir_len + 20;  // Extra space for "/.ccls" or "/compile_flags.txt" and null terminator
  // Dynamically allocate memory for paths
  char *ccls_path = (char *)malloc(max_path_len * sizeof(char));
  char *compile_flags_path = (char *)malloc(max_path_len * sizeof(char));

  // Check if memory allocation failed
  if(!ccls_path || !compile_flags_path) {
    // Free any successfully allocated memory
    free(ccls_path);
    free(compile_flags_path);
    return 1;  // Return error code
  }

  // Initialize ccls_path and compile_flags_path
  memset(ccls_path, 0, max_path_len);
  memset(compile_flags_path, 0, max_path_len);
  // Create path strings
  snprintf(ccls_path, max_path_len, "%s/.ccls", directory);
  snprintf(compile_flags_path, max_path_len, "%s/compile_flags.txt", directory);
  // Create .ccls with basic configuration
  FILE *ccls = fopen(ccls_path, "w");
  int return_value = 0;  // Store return value

  if(ccls) {
    fprintf(ccls, "clang\n%%c -std=c11\n%%cpp -std=c++17\n");
    fclose(ccls);
  }

  else {
    return_value = 1;
  }

  // Only proceed with second file if first file creation succeeded
  if(return_value == 0) {
    // Create compile_flags.txt with basic flags
    FILE *compile_flags = fopen(compile_flags_path, "w");

    if(compile_flags) {
      fprintf(compile_flags, "-I.\n-I..\n-I/usr/include\n-I/usr/local/include\n");
      fclose(compile_flags);
    }

    else {
      return_value = 1;
    }
  }

  // Free allocated memory
  free(ccls_path);
  free(compile_flags_path);
  return return_value;
}

/**
   Finds the .ccls and compile_flags.txt files in the directory tree starting from the given path (UNIX-specific).

   This function recursively searches upward through the directory hierarchy starting from the specified
   path until it finds both .ccls and compile_flags.txt in the same directory, or reaches the root (/).
   If found, the directory path is stored in found_at. If not found by the root, it returns an error.

   @param path      The directory path from which to start the search (e.g., source code directory).
   @param found_at  A buffer to store the path where the files were found (must be PATH_MAX size).
   @return          0 if the files are found; non-zero otherwise.
*/
int findFiles(const char *path, char *found_at) {
  DIR *dir;                            // Directory stream pointer
  struct dirent *entry;                // Directory entry structure
  char currentPath[PATH_MAX] = {0};    // Buffer for current directory path
  int cclsFound = 0;                   // Flag for .ccls
  int compileFlagsFound = 0;           // Flag for compile_flags.txt

  // Validate inputs
  if(path == NULL) {
    fprintf(stderr, "fn findFiles: Path is NULL\n");
    return 1; // Return error code
  }

  // Ensure the found_at buffer is not NULL to store the result
  if(found_at == NULL) {
    fprintf(stderr, "fn findFiles: Found_at buffer is NULL\n");
    return 1; // Return error code
  }

  // Resolve absolute path of the starting directory to handle relative paths and symbolic links
  char *abs_path = realpath(path, NULL);

  if(abs_path == NULL) {  // realpath fails, possibly due to invalid path or permissions
    perror("realpath"); // Print detailed error message
    return 1; // Return error code
  }

  snprintf(currentPath, PATH_MAX, "%s", abs_path);
  free(abs_path);
  // Open the current directory
  dir = opendir(currentPath);

  if(!dir) {  // opendir fails, could be due to permission issues or non-existent directory
    perror("opendir"); // Print detailed error message
    return 1; // Return error code
  }

  // Read each entry in the directory to search for .ccls and compile_flags.txt
  while((entry = readdir(dir)) != NULL) {
    if(strcmp(entry->d_name, ".ccls") == 0) {  // Check if the entry is .ccls
      cclsFound = 1;
    }

    else if(strcmp(entry->d_name, "compile_flags.txt") == 0) {    // Check if the entry is compile_flags.txt
      compileFlagsFound = 1;
    }
  }

  // Close the directory stream after reading all entries
  closedir(dir);

  // Check if both .ccls and compile_flags.txt are found in the current directory,  store the path and return success
  if(cclsFound && compileFlagsFound) {
    snprintf(found_at, PATH_MAX, "%s", currentPath);
    return 0; // Success: files found
  }

  // If at root and files not found, fail
  if(strcmp(currentPath, "/") == 0) {
    return 1;  // Return error code
  }

  // Move up to parent directory and recurse
  char *currentPathCopy = strdup(currentPath);

  if(!currentPathCopy) {
    perror("strdup");
    return 1; // Return error code: reached root without finding files
  }

  char *parentDir = dirname(currentPathCopy);

  if(!parentDir) {
    perror("dirname");
    free(currentPathCopy);
    return 1;
  }

  char parentPath[PATH_MAX] = {0};
  snprintf(parentPath, PATH_MAX, "%s", parentDir);
  free(currentPathCopy);
  // Recursively search the parent
  return findFiles(parentPath, found_at);
}

// Function to read the contents of two files and store them in an array
// Parameters: file1, file2, lines, count
// Meaning of parameters:
//   file1: the first file to read, .ccls
//   file2: the second file to read, compile_flags.txt
//   lines: the array to store the lines in
//   count: the number of lines read
// Return value: none
void read_files(const char *file1, const char *file2, char **lines, int *count) {
  FILE *f1 = fopen(file1, "r");
  FILE *f2 = fopen(file2, "r");

  if(f1 == NULL || f2 == NULL) {
    printf("Error opening files.\n");
    exit(EXIT_FAILURE);
  }

  char line[MAX_LINE_LENGTH];

  while(fgets(line, sizeof(line), f1) != NULL) {
    // Skip lines that contain "-Iinc"
    if(strstr(line, "-Iinc")) {
      continue;
    }

    if(strstr(line, "-isystem") || strstr(line, "-I")) {
      if(*count < MAX_LINES - 1) {
        // Strip newline character
        line[strcspn(line, "\n")] = '\0';
        lines[*count] = strdup(line);
        (*count)++;
      }
    }
  }

  while(fgets(line, sizeof(line), f2) != NULL) {
    // Skip lines that contain "-Iinc"
    if(strstr(line, "-Iinc")) {
      continue;
    }

    if(strstr(line, "-isystem") || strstr(line, "-I")) {
      if(*count < MAX_LINES - 1) {
        // Strip newline character
        line[strcspn(line, "\n")] = '\0';
        lines[*count] = strdup(line);
        (*count)++;
      }
    }
  }

  fclose(f1);
  fclose(f2);
}

// Function to remove duplicate lines
void remove_duplicates(char **lines, int *count) {
  for(int i = 0; i < *count; i++) {
    for(int j = i + 1; j < *count; j++) {
      if(strcmp(lines[i], lines[j]) == 0) {
        free(lines[j]);

        for(int k = j; k < *count - 1; k++) {
          lines[k] = lines[k + 1];
        }

        (*count)--;
        j--;
      }
    }
  }
}

// Function to store lines in the array
// Parameters:
//   file1: path to the first file, .ccls file
//   file2: path to the second file, compile_flags.txt file
//   lines: array to store the lines
//   sorted_lines: array to store the sorted lines
//   count: pointer to the number of lines
void store_lines(const char *file1, const char *file2, char **lines, char **sorted_lines, int *count) {
  // Read files and store lines in the array
  read_files(file1, file2, lines, count);
  remove_duplicates(lines, count);

  // Copy the lines to sorted_lines
  for(int i = 0; i < *count; i++) {
    sorted_lines[i] = lines[i];
  }

  // Sort the lines if count is valid
  if(*count > 0) {
    qsort(sorted_lines, (size_t)*count, sizeof(char *), compare_strings);
  }
}

// Function to compare strings
int compare_strings(const void *a, const void *b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

// Function to get clang target. Filters output from the command `clang --version`. Takes the CPU architecture from the line `Target:`.
int get_clang_target(char *output) {
  // Check cache first
  if(completion_cache.is_valid && completion_cache.cpu_arch[0] != '\0') {
    strncpy(output, completion_cache.cpu_arch, MAX_LINE_LENGTH - 1);
    output[MAX_LINE_LENGTH - 1] = '\0';
    return 0;
  }

  char *target_str = "Target: ";
  char *output_buffer = (char *)malloc(MAX_OUTPUT * sizeof(char));
  // Execute the command and capture the output
  int pipe_fd[2];
  pipe(pipe_fd);
  pid_t pid = fork();

  if(pid == 0) {
    dup2(pipe_fd[1], STDOUT_FILENO);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    execlp("clang", "clang", "--version", (char *)NULL);
    exit(0);
  }

  else if(pid > 0) {
    close(pipe_fd[1]);
    // Read the output from the child process
    int bytes_read = (int)read(pipe_fd[0], output_buffer, MAX_OUTPUT - 1);
    output_buffer[bytes_read] = '\0';
    close(pipe_fd[0]);
    wait(NULL);
    // Find the line containing "Target: "
    char *target_line = strstr(output_buffer, target_str);

    if(target_line) {
      char *target_value = target_line + strlen(target_str);
      int target_length = (int)strcspn(target_value, "\n");
      // Copy the target value to the output parameter
      strncpy(output, target_value, (size_t)target_length);
      output[target_length] = '\0';
      // Cache the CPU architecture
      strncpy(completion_cache.cpu_arch, output, MAX_LINE_LENGTH - 1);
      completion_cache.cpu_arch[MAX_LINE_LENGTH - 1] = '\0';
      // Free the buffer
      free(output_buffer);
      return 0;
    }

    else {
      free(output_buffer);
      return 1;
    }
  }

  else {
    perror("fork");
    free(output_buffer);
    return 1;
  }
}

// Function to collect filename, line number, and column number
char *collect_code_completion_args(const char *filename, int line, int column) {
  // Calculate initial sizes based on filename length
  size_t filename_len = strlen(filename);
  size_t max_path_len = filename_len + 256;  // Extra space for paths and null terminator
  // Dynamically allocate memory for all fixed-size arrays
  char *found_at = (char *)malloc(max_path_len * sizeof(char));
  char *target_output = (char *)malloc(max_path_len * sizeof(char));
  char **lines = (char **)malloc(MAX_LINES * sizeof(char *));
  char **sorted_lines = (char **)malloc(MAX_LINES * sizeof(char *));
  char *abs_filename = (char *)malloc(max_path_len * sizeof(char));
  char *ccls_path = (char *)malloc(max_path_len * sizeof(char));
  char *compile_flags_path = (char *)malloc(max_path_len * sizeof(char));
  char cpu_arch[MAX_LINE_LENGTH];

  // Check for allocation failures
  if(!found_at || !target_output || !lines || !sorted_lines || !abs_filename || !ccls_path || !compile_flags_path) {
    // Handle allocation failure
    free(found_at);
    free(target_output);
    free(lines);
    free(sorted_lines);
    free(abs_filename);
    free(ccls_path);
    free(compile_flags_path);
    return NULL;
  }

  int count = 0;

  // Check if the file exists
  if(access(filename, F_OK) != 0) {
    perror("File does not exist");
    log_message("fn collect_code_completion_args: File does not exist\n");
    // Free all allocated memory
    free(found_at);
    free(target_output);
    free(lines);
    free(sorted_lines);
    free(abs_filename);
    free(ccls_path);
    free(compile_flags_path);
    return NULL;
  }

  // Get the absolute path of the filename and its directory
  if(realpath(filename, abs_filename) == NULL) {
    perror("realpath");
    log_message("fn collect_code_completion_args: Error getting absolute path\n");
    // Free all allocated memory
    free(found_at);
    free(target_output);
    free(lines);
    free(sorted_lines);
    free(abs_filename);
    free(ccls_path);
    free(compile_flags_path);
    return NULL;
  }

  char *dir_path = dirname(abs_filename);
  // Initialize cache if not already done
  static int cache_initialized = 0;

  if(!cache_initialized) {
    init_cache();
    cache_initialized = 1;
  }

  // Find the directory where .ccls and compile_flags.txt are located
  // Check if we can use cached values
  if(is_cache_valid(global_buffer_project_dir) && global_buffer_project_dir[0] != '\0') {
    char **cached_paths = get_cached_include_paths(&count);

    if(cached_paths && count > 0 && global_buffer_cpu_arc[0] != '\0') {
      size_t command_length = 512 + strlen(global_buffer_cpu_arc) + strlen(filename) * 2 + count * MAX_PATH_LENGTH;
      char *command = (char *)malloc(command_length);

      if(!command) {
        free(found_at);
        free(target_output);
        free(lines);
        free(sorted_lines);
        free(abs_filename);
        free(ccls_path);
        free(compile_flags_path);
        return NULL;
      }

      int offset = snprintf(command, command_length,
                            "clang -target %s -fsyntax-only -Xclang -code-completion-macros",
                            global_buffer_cpu_arc);

      for(int i = 0; i < count; i++) {
        offset += snprintf(command + offset, command_length - offset, " %s", cached_paths[i]);
      }

      snprintf(command + offset, command_length - offset,
               " -Xclang -code-completion-at=%s:%d:%d %s", filename, line, column, filename);
      free(found_at);
      free(target_output);
      free(lines);
      free(sorted_lines);
      free(abs_filename);
      free(ccls_path);
      free(compile_flags_path);
      return command;
    }
  }

  // Cache miss - recalculate
  if(findFiles(dir_path, found_at) != 0) {
    printf("Error finding .ccls and compile_flags.txt\n");
    log_message("fn collect_code_completion_args: Error finding .ccls and compile_flags.txt\n");
    // Free all allocated memory
    free(found_at);
    free(target_output);
    free(lines);
    free(sorted_lines);
    free(abs_filename);
    free(ccls_path);
    free(compile_flags_path);
    return NULL;
  }

  // copy the value of found_at using strndup to store it into a global variable global_buffer_project_dir
  strncpy(global_buffer_project_dir, found_at, PATH_MAX - 1);
  global_buffer_project_dir[PATH_MAX - 1] = '\0'; // Ensure null termination
  // print global_buffer_project_dir
  // printf("DEBUG: fn collect_code_completion_args: global_buffer_project_dir: %s\n", global_buffer_project_dir);
  /*
    GLOBAL BUFFER PART HERE
    Remember to use the global_buffer_project_dir
    as a substitute for found_at the next time this function is called
  */

  // Get the clang target
  if(get_clang_target(target_output) != 0) {
    printf("Error getting clang target\n");
    log_message("fn collect_code_completion_args: Error getting clang target\n");
    // Free all allocated memory
    free(found_at);
    free(target_output);
    free(lines);
    free(sorted_lines);
    free(abs_filename);
    free(ccls_path);
    free(compile_flags_path);
    return NULL;
  }

  // copy the value of target_output using strndup to store it into a global variable global_buffer_cpu_arc
  strncpy(global_buffer_cpu_arc, target_output, MAX_OUTPUT - 1);
  global_buffer_cpu_arc[MAX_OUTPUT - 1] = '\0'; // Ensure null termination
  // print global_buffer_cpu_arc
  // printf("DEBUG: fn collect_code_completion_args: global_buffer_cpu_arc: %s\n", global_buffer_cpu_arc);
  /*
    GLOBAL BUFFER PART HERE
    Remember to use the global_buffer_cpu_arc
    as a substitute for target_output the next time this function is called
  */
  // Construct full paths based on global_buffer_project_dir
  snprintf(ccls_path, max_path_len, "%s/.ccls", global_buffer_project_dir);
  snprintf(compile_flags_path, max_path_len, "%s/compile_flags.txt", global_buffer_project_dir);

  // Initialize lines array
  for(int i = 0; i < MAX_LINES; i++) {
    lines[i] = NULL;
    sorted_lines[i] = NULL;
  }

  // Read and process the include paths
  store_lines(compile_flags_path, ccls_path, lines, sorted_lines, &count);
  //
  // Copy the value of sorted_lines into global_buffer_header_paths through a for loop
  static size_t num_lines = 0; // Initialize the counter. It counts the number of lines copied.
  num_lines = 0;

  for(size_t i = 0; i < (size_t)count && num_lines < MAX_LINES; i++) {
    strncpy(global_buffer_header_paths[num_lines], sorted_lines[i], MAX_PATH_LENGTH - 1);
    global_buffer_header_paths[num_lines][MAX_PATH_LENGTH - 1] = '\0'; // Null-terminate
    num_lines++;
  }

  // print global_buffer_header_paths
  //for(int i = 0; i < (int)num_lines; i++) {
  //printf("DEBUG: fn collect_code_completion_args: global_buffer_header_paths: %s\n", global_buffer_header_paths[i]);
  //}
  /*
    GLOBAL BUFFER PART HERE

    Remember to use the global_buffer_header_paths
    as a substitute for sorted_lines the next time this function is called.
    If the function is designed to work with char **,
    you may need to modify how you store and pass the data.
    You can create a temporary array of pointers and populate it
    with the addresses of the strings in your two-dimensional array.

    Use it as follows:

    char *temp[MAX_LINES];

    for(size_t i = 0; i < num_lines; i++) {
        temp[i] = global_buffer_header_paths[i];
    }

    // Then, pass temp to the desired function.
  */
  // Update cache
  char *temp_paths[MAX_LINES];

  for(size_t i = 0; i < num_lines; i++) {
    temp_paths[i] = global_buffer_header_paths[i];
  }

  update_cache(global_buffer_project_dir, temp_paths, (int)num_lines, global_buffer_cpu_arc);
  // Build command string
  size_t command_length = 512 + strlen(global_buffer_cpu_arc) + strlen(filename) * 2 + num_lines * MAX_PATH_LENGTH;
  char *command = (char *)malloc(command_length);

  if(!command) {
    log_message("In fn collect_code_completion_args: Failed to allocate memory for the result.\n");
    // Free all allocated memory
    free(found_at);
    free(target_output);
    free(lines);
    free(sorted_lines);
    free(abs_filename);
    free(ccls_path);
    free(compile_flags_path);
    return NULL;
  }

  // Construct the clang command
  int offset = snprintf(command, command_length,
                        "clang -target %s -fsyntax-only -Xclang -code-completion-macros",
                        global_buffer_cpu_arc);

  for(size_t i = 0; i < num_lines; i++) {
    offset += snprintf(command + offset, command_length - offset, " %s", global_buffer_header_paths[i]);
  }

  snprintf(command + offset, command_length - offset,
           " -Xclang -code-completion-at=%s:%d:%d %s", filename, line, column, filename);

  // Free allocated memory
  for(int i = 0; i < count; i++) {
    free(lines[i]);
  }

  // Free all temporary allocated memory (except command, which is returned)
  free(found_at);
  free(target_output);
  free(lines);
  free(sorted_lines);
  free(abs_filename);
  free(ccls_path);
  free(compile_flags_path);
  return command;
}

// Function to execute the code completion command: `clang -fsyntax-only -Xclang -code-completion-macros -Xclang -code-completion-at=file.c:line:column file.c`
char *execute_code_completion_command(const char *filename, int line, int column) {
  /* printf("DEBUG: Starting code completion for file %s at line %d, column %d\n",
         filename, line, column); */
  char *command = collect_code_completion_args(filename, line, column);

  if(!command) {
    /* printf("DEBUG: Failed to collect code completion arguments\n"); */
    return NULL;
  }

  /* printf("DEBUG: Command to execute: %s\n", command); */
  char *output = (char *)malloc(MAX_OUTPUT * sizeof(char));

  if(!output) {
    /* printf("DEBUG: Failed to allocate output buffer\n"); */
    free(command);
    return NULL;
  }

  output[0] = '\0';
  /* printf("DEBUG: Output buffer allocated, size: %d\n", MAX_OUTPUT); */
  FILE *fp = popen(command, "r");

  if(fp == NULL) {
    perror("DEBUG: popen failed");
    free(command);
    free(output);
    return NULL;
  }

  /* printf("DEBUG: Command executed successfully\n"); */
  size_t total_length = 0;
  char buffer[1024];

  while(fgets(buffer, sizeof(buffer), fp) != NULL) {
    size_t chunk_length = strlen(buffer);
    /* printf("DEBUG: Read chunk of %zu bytes: %s", chunk_length, buffer); */

    /* printf("DEBUG: Read chunk of %zu bytes: %s", chunk_length, buffer); */
    // More precise buffer overflow check
    if(total_length + chunk_length >= MAX_OUTPUT - 1) {
      /* printf("DEBUG: Output buffer overflow, current length: %zu, chunk length: %zu\n",
             total_length, chunk_length); */
      pclose(fp);
      free(command);
      free(output);
      return NULL;
    }

    strcat(output, buffer);
    total_length += chunk_length;
    /* printf("DEBUG: Current total output length: %zu\n", total_length); */
  }

  if(ferror(fp)) {
    perror("DEBUG: fgets error");
    pclose(fp);
    free(command);
    free(output);
    return NULL;
  }

  int exit_status = pclose(fp);

  if(exit_status == -1) {
    perror("DEBUG: pclose failed");
    free(command);
    free(output);
    return NULL;
  }

  // Check for non-zero exit status (indicating clang error)
  if(exit_status != 0) {
    printf("DEBUG: Command failed with exit status: %d\n", exit_status);
    // You might want to return NULL or handle this differently
  }

  else {
    /* printf("DEBUG: Command completed successfully with exit status: %d\n", exit_status); */
  }

  if(total_length == 0) {
    /* printf("DEBUG: No output received from command\n"); */
    free(command);
    free(output);
    return strdup("");
  }

  /* printf("DEBUG: Final command output:\n%s\n", output); */
  char *result = strdup(output);
  free(command);
  free(output);

  if(!result) {
    /* printf("DEBUG: Failed to allocate result string\n"); */
    return NULL;
  }

  /* printf("DEBUG: Returning result, length: %zu\n", strlen(result)); */
  return result;
}

/* Substitute the output from the command
  clang -fsyntax-only -Xclang -code-completion-macros -Xclang -code-completion-at=file.c:line:column file.c
  such as: PREFERRED-TYPE: double
  COMPLETION: remainderf : [#float#]remainderf(<#float x#>, <#float y#>)
  with    double result = remainderf(`<float x>`, `<float y>`) by matching certain patterns */
char *filter_clang_output(const char *input) {
  /* printf("DEBUG: Input string:\n%s\n", input); */
  // Dynamically allocate memory for output
  char *output = (char *)malloc(MAX_OUTPUT * sizeof(char));

  if(!output) {
    /* printf("DEBUG: Failed to allocate output buffer\n"); */
    return NULL;
  }

  output[0] = '\0';
  /* printf("DEBUG: Output buffer allocated, initial length: %zu\n", strlen(output)); */
  regex_t regex;
  regmatch_t *matches = (regmatch_t *)malloc(MAX_REGX_MATCHES * sizeof(regmatch_t));

  if(!matches) {
    /* printf("DEBUG: Failed to allocate matches array\n"); */
    free(output);
    return NULL;
  }

  /* printf("DEBUG: Matches array allocated\n"); */
  // Pattern to match "COMPLETION: function : [#type#]function(<#type#>, <#type#>)"
  const char *pattern = "^COMPLETION: ([^ ]+) : \\[#([^#]+)#\\]\\1\\(<#([^#]+)#>(, <#([^#]+)#>)*\\)";
  /* printf("DEBUG: Using regex pattern: %s\n", pattern); */
  int regex_result = regcomp(&regex, pattern, REG_EXTENDED | REG_NEWLINE);

  if(regex_result != 0) {
    char errbuf[256];
    regerror(regex_result, &regex, errbuf, sizeof(errbuf));
    /* printf("DEBUG: Regex compilation failed: %s\n", errbuf); */
    free(output);
    free(matches);
    return NULL;
  }

  /* printf("DEBUG: Regex compiled successfully\n"); */
  // Check if input contains COMPLETION lines
  const char *cursor = input;
  int has_completion = 0;

  while(*cursor != '\0') {
    if(strncmp(cursor, "COMPLETION:", 11) == 0) {
      has_completion = 1;
      break;
    }

    while(*cursor != '\0' && *cursor != '\n') {
      cursor++;
    }

    if(*cursor == '\n') {
      cursor++;
    }
  }

  /* printf("DEBUG: Input contains COMPLETION lines: %s\n",
         has_completion ? "yes" : "no"); */
  cursor = input;
  size_t matches_size = MAX_REGX_MATCHES;
  int match_count = 0;

  while(regexec(&regex, cursor, matches_size, matches, 0) == 0) {
    match_count++;
    /* printf("DEBUG: Found match #%d\n", match_count); */
    /*printf("DEBUG: Match[0] (full): %.*s\n",
           (int)(matches[0].rm_eo - matches[0].rm_so),
           cursor + matches[0].rm_so);
      printf("DEBUG: Match[1] (function): %.*s\n",
           (int)(matches[1].rm_eo - matches[1].rm_so),
           cursor + matches[1].rm_so);
      printf("DEBUG: Match[2] (return type): %.*s\n",
           (int)(matches[2].rm_eo - matches[2].rm_so),
           cursor + matches[2].rm_so);
      printf("DEBUG: Match[3] (first param): %.*s\n",
           (int)(matches[3].rm_eo - matches[3].rm_so),
           cursor + matches[3].rm_so); */
    // Calculate remaining space in output buffer
    size_t output_len = strlen(output);
    size_t remaining_space = MAX_OUTPUT - output_len - 1;
    /* printf("DEBUG: Output length: %zu, Remaining space: %zu\n",
           output_len, remaining_space); */
    // Extract and append function name (matches[1])
    size_t match_len = (size_t)(matches[1].rm_eo - matches[1].rm_so);
    /* printf("DEBUG: Function name length: %zu\n", match_len); */

    if(match_len >= remaining_space) {
      /* printf("D  EBUG: Buffer overflow for function name\n"); */
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, cursor + matches[1].rm_so, match_len);
    remaining_space -= match_len;
    /* printf("DEBUG: After function name, output: %s\n", output); */

    // Append opening parenthesis
    if(remaining_space < 1) {
      /* printf("D  EBUG: Buffer overflow for opening parenthesis\n"); */
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, "(", 2); // 1+1, increased to prevent buffer overflow
    remaining_space -= 1;
    /* printf("DEBUG: After opening parenthesis, output: %s\n", output); */

    // Process first parameter (matches[3])
    if(remaining_space < 2) {
      /* printf("DEBUG: Buffer overflow for first param start\n"); */
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, "`", 2);
    remaining_space -= 2;
    /* printf("DEBUG: After first param start, output: %s\n", output); */
    match_len = (size_t)(matches[3].rm_eo - matches[3].rm_so);
    /* printf("DEBUG: First param length: %zu\n", match_len); */

    if(match_len >= remaining_space) {
      /* printf("D  EBUG: Buffer overflow for first param\n"); */
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    if(remaining_space < 2) {
      /* printf("D  EBUG: Buffer overflow for first backtick\n"); */
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, "<", 2);
    remaining_space -= 2;
    /* printf("DEBUG: After first backtick, output: %s\n", output); */
    strncat(output, cursor + matches[3].rm_so, match_len);
    remaining_space -= match_len;
    /* printf("DEBUG: After first param value, output: %s\n", output); */

    if(remaining_space < 2) {
      /* printf("DEBUG: Buffer overflow for first param end\n"); */
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, ">`", 3); // 2+1, increased to prevent buffer overflow
    remaining_space -= 2;
    /* printf("DEBUG: After first param end, output: %s\n", output); */

    // Handle additional parameters (matches[4] and matches[5])
    if(matches[4].rm_so != -1) {
      /* printf("DEBUG: Second parameter found\n"); */
      if(remaining_space < 4) {
        /* printf("DEBUG: Buffer overflow for second param start\n"); */
        regfree(&regex);
        free(output);
        free(matches);
        return NULL;
      }

      strncat(output, ", `<", 5); // 4+1, increased to prevent buffer overflow
      remaining_space -= 4;
      /* printf("DEBUG: After second param start, output: %s\n", output); */
      match_len = (size_t)(matches[5].rm_eo - matches[5].rm_so);
      /* printf("DEBUG: Second param length: %zu\n", match_len); */

      if(match_len >= remaining_space) {
        /* printf("D  EBUG: Buffer overflow for second param\n"); */
        regfree(&regex);
        free(output);
        free(matches);
        return NULL;
      }

      strncat(output, cursor + matches[5].rm_so, match_len);
      remaining_space -= match_len;
      /* printf("DEBUG: After second param value, output: %s\n", output); */

      if(remaining_space < 2) {
        /* printf("DEBUG: Buffer overflow for second param end\n"); */
        regfree(&regex);
        free(output);
        free(matches);
        return NULL;
      }

      strncat(output, ">`", 3); // 2+1, increased to prevent buffer overflow
      remaining_space -= 2;
      /* printf("DEBUG: After second param end, output: %s\n", outpu  t); */
    }

    // Append closing parenthesis and newline
    if(remaining_space < 2) {
      /* printf("DEBUG: Buffer overflow for closing parenthesis\n"); */
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, ")\n", 3); // 2+1, increased to prevent buffer overflow
    /* printf("DEBUG: After closing parenthesis, output: %s\n", output); */
    // Move cursor to next line
    cursor += matches[0].rm_eo;
    /* printf("DEBUG: Cursor moved to position: %zu\n",
           (size_t)(cursor - input)); */

    while(*cursor != '\0' && *cursor != '\n') {
      cursor++;
    }

    if(*cursor == '\n') {
      cursor++;
    }

    /* printf("DEBUG: Cursor after line skip: %zu\n",
           (size_t)(cursor - input)); */
  }

  /* printf("DEBUG: Total matches found: %d\n", match_count);
    printf("DEBUG: Final output: %s\n", output); */

  // If no matches were found, return empty string
  if(match_count == 0) {
    /* printf("DEBUG: No completions found, returning empty string\n"); */
    regfree(&regex);
    char *result = strdup("");
    free(output);
    free(matches);
    /* printf("DEBUG: Function complete, returning result\n "); */
    return result;
  }

  regfree(&regex);
  char *result = strdup(output);
  free(output);
  free(matches);
  /* printf("DEBUG: Function complete, returning result\n"); */
  return result;
}

/* Intended to be called from Vim with :%p. When called, it should generate .ccls-cache in the directory passed as an argument. */
int execute_ccls_index(const char *directory) {
  // Calculate required buffer size for found_at and command
  size_t dir_len = strlen(directory);
  size_t max_path_len = dir_len + 256;  // Extra space for paths and null terminator
  // Dynamically allocate memory for found_at and command
  char *found_at = (char *)malloc(max_path_len * sizeof(char));
  char *command = (char *)malloc(max_path_len * sizeof(char));

  // Check for allocation failures
  if(!found_at || !command) {
    free(found_at);
    free(command);
    return -1;
  }

  if(findFiles(directory, found_at) != 0) {
    printf("Error finding .ccls and compile_flags.txt\n");
    free(found_at);
    free(command);
    return -1;
  }

  // Construct the command string
  snprintf(command, max_path_len, "ccls --index %s", found_at);
  // Execute the command
  int result = system(command);
  // Free allocated memory
  free(found_at);
  free(command);

  // Check if the command was successful
  if(result != 0) {
    fprintf(stderr, "Failed to execute ccls --index command\n");
    return -1;
  }

  return 0;
}

// Function to split the input string into file path, line, and column
void split_input_string(const char *input, char *file_path, int *line, int *column) {
  // Dynamically allocate memory for input_copy and log_file_path
  size_t input_len = strlen(input) + 1;  // +1 for null terminator
  char *input_copy = (char *)malloc(input_len * sizeof(char));

  // Check for allocation failures
  if(!input_copy) {
    fprintf(stderr, "Memory allocation failed\n");
    log_message("In fn split_input_string: Memory allocation failed\n");
    free(input_copy);
    return;
  }

  // Make a copy of the input string to use with strtok
  strcpy(input_copy, input);
  // Divide input string into three parts
  char *str = strtok(input_copy, " ");

  if(str == NULL) {
    fprintf(stderr, "Invalid input format\n");
    log_message("In fn split_input_string: Invalid input format\n");
    free(input_copy);
    return;
  }

  // Copy the file path to file_path (caller-provided buffer)
  // Assuming file_path has sufficient space (1024 bytes as per original)
  strncpy(file_path, str, 1023);
  file_path[1023] = '\0'; // Ensure null termination
  // Parse the line and column numbers
  char *line_str = strtok(NULL, " ");
  char *column_str = strtok(NULL, " ");

  if(line_str == NULL || column_str == NULL) {
    fprintf(stderr, "Invalid input format\n");
    log_message("In fn split_input_string: Invalid input format\n");
    free(input_copy);
    return;
  }

  *line = atoi(line_str);
  *column = atoi(column_str);
  // Free allocated memory
  free(input_copy);
}

// Function to process the completion data from the string
// It is the driver function for the executable file
// Returns a string containing the completion data, ready to be used by the Vim plugin, code_connector.vim
// The string is dynamically allocated and should be freed by the caller
// Returns NULL on failure
char *processCompletionDataFromString(const char *vimInputString) {
  if(vimInputString == NULL) {
    fprintf(stderr, "Input string is NULL.\n");
    log_message("fn processCompletionDataFromString: Input string is NULL.\n");
    return NULL;
  }

  // Allocate memory for the result (return value)
  size_t output_size = strlen(vimInputString) + 50;
  char *output_to_return = (char *)malloc(output_size * sizeof(char));

  if(output_to_return == NULL) {
    log_message("fn processCompletionDataFromString: Failed to allocate memory for the result.\n");
    return NULL;
  }

  // Dynamically allocate memory for file_path and log_filtered_result
  char *file_path = (char *)malloc(1024 * sizeof(char));  // Maintain original size

  // Check for allocation failures

  if(!file_path) {
    log_message("fn processCompletionDataFromString: Failed to allocate memory for internal buffers.\n");
    free(output_to_return);
    free(file_path);
    return NULL;
  }

  int extracted_line = 0;
  int extracted_column = 0;
  // Call split_input_string to split the input string
  split_input_string(vimInputString, file_path, &extracted_line, &extracted_column);
  // Call the function to execute the code completion command
  char *result = execute_code_completion_command(file_path, extracted_line, extracted_column);

  //printf("result: %s\n", result);
  if(result) {
    // Filter and transform the output
    char *filtered_result = filter_clang_output(result);
    // printf(" DEBUG: filtered_result: %s\n", filtered_result);
    // Only accept the first line of the result
    char *first_line = strtok(filtered_result, "\n");
    // printf("DEBUG: fn processCompletionDataFromString: first_line: %s\n", first_line);
    filtered_result = first_line;

    if(filtered_result) {
      // Copy the result to output_to_return buffer
      size_t filtered_len = strlen(filtered_result);

      if(filtered_len < output_size) {
        strncpy(output_to_return, filtered_result, filtered_len);
        output_to_return[filtered_len] = '\0';  // Ensure null termination
      }

      else {
        printf("fn processCompletionDataFromString: Filtered result too large for output buffer.\n");
        free(result);
        free(filtered_result);
        free(output_to_return);
        free(file_path);
        return NULL;
      }

      // Free temporary memory
      free(result);
      free(filtered_result);
      free(file_path);
      return output_to_return;  // output_to_return to be freed by Vim
    }

    else {
      printf("fn processCompletionDataFromString: Failed to filter code completion output.\n");
      free(result);
      free(output_to_return);
      free(file_path);
      return NULL;
    }
  }

  else {
    printf("fn processCompletionDataFromString: Failed to execute code completion command.\n");
    free(output_to_return);
    free(file_path);
    return NULL;
  }
}

// Function to write the result to a temporary file and return the file path
// As of now, it is a redundant function, but it is kept for testing, etc., in the future
// Returns the path of the temporary file
// The result is freed by the caller
// The file path is freed by the caller
char *writeResultToTempFile(const char *result) {
  // Calculate required buffer size for tempFilePath
  // Base path + pid + extra space for formatting
  size_t max_path_len = MAX_PATH_LENGTH;  // Reasonable size for temp path
  char *tempFilePath = (char *)malloc(max_path_len * sizeof(char));

  if(!tempFilePath) {
    perror("fn writeResultToTempFile: Failed to allocate memory for temporary file path");
    return NULL;
  }

  // Delete the previously created temporary directory along with all its files
  system("rm -rf /tmp/code_connector_vim_return/*");
  // Create a directory in the temp for the temporary file
  mkdir("/tmp/code_connector_vim_return", 0700);
  // Construct the temporary file path
  snprintf(tempFilePath, max_path_len,
           "/tmp/code_connector_vim_return/code_connector_output_%d.txt",
           getpid());
  FILE *tempFile = fopen(tempFilePath, "w");

  if(tempFile == NULL) {
    perror("Failed to create temporary file");
    free(tempFilePath);
    return NULL;
  }

  fprintf(tempFile, "%s", result);
  fclose(tempFile);
  // Create a copy of the path to return
  char *result_path = strdup(tempFilePath);
  // Free temporary allocated memory
  free(tempFilePath);
  return result_path; // The caller is still responsible for freeing the returned string (created by strdup()).
}

// Function to process the completion data from the vim plugin, similar to processCompletionDataFromString
// This function was originally intended to be used from Vim by calling the shared library directly
// This function is presenly a redundant function
// It is kept for testing, etc., in the future
void processCompletionDataForVim(const char *vimInputString) {
  // Process the completion data from the vim plugin
  char *result = processCompletionDataFromString(vimInputString);

  // Dynamically allocate memory for log_tempFilePath and log_global_result_buffer
  // Check for allocation failures
  if(result) {
    free(result);
  }

  if(result == NULL) {
    log_message("Warning: processCompletionDataFromString returned NULL");
    global_result_buffer[0] = '\0'; // Clear the buffer if result is NULL
  }

  else {
    // Copy result to global_result_buffer
    strncpy(global_result_buffer, result, MAX_OUTPUT - 1);
    global_result_buffer[MAX_OUTPUT - 1] = '\0'; // Ensure null termination
    // Write global_result_buffer to a temporary file
    char *tempFilePath = writeResultToTempFile(global_result_buffer);

    if(tempFilePath) {
      free(tempFilePath); // Free the allocated memory
    }

    else {
      printf("Failed to write output to temporary file.\n");
      log_message("Failed to write output to temporary file.");
    }

    free(result); // Free the allocated memory
  }
}

// Returns the global buffer for data exchange
char *transfer_global_buffer(void) {
  // Safety check: ensure global_result_buffer is not NULL
  if(global_result_buffer[0] == '\0') {
    log_message("Warning: global_result_buffer is empty in transfer_global_buffer\n");
    // Return pointer to empty buffer rather than NULL to maintain original behavior
    return global_result_buffer;
  }

  // Note: Returning pointer to global buffer
  // - global_result_buffer is assumed to be a global array defined elsewhere
  // - The caller must not free this pointer
  // - The buffer's lifetime is managed globally, not by this function
  // - No dynamic allocation is needed as we're returning the existing global buffer
  // Debug logging (commented out as in original)
  // printf("Global extern to transfer output data: %s\n", global_result_buffer);
  //
  return global_result_buffer;
  //
  // Call it from other functions as: char *data = transfer_global_buffer();
  // Note: The returned pointer should not be freed by the caller
}

// Function to log messages to a file
void log_message(const char *message) {
  // Note: This function does not require dynamic memory allocation
  // - No local buffers or arrays are used
  // - The input message is provided by the caller
  // - No return value to manage
  // - The caller is not responsible for freeing any memory
  if(message == NULL) {
    // Handle NULL message case
    FILE *log_file = fopen("/tmp/vim_parser_log.txt", "a");

    if(log_file) {
      fprintf(log_file, "Warning: NULL message passed to log_message\n");
      fclose(log_file);
    }

    return;
  }

  FILE *log_file = fopen("/tmp/vim_parser_log.txt", "a");

  if(log_file) {
    fprintf(log_file, "%s\n", message);
    fclose(log_file);
  }

  else {
    // Log file couldn't be opened; error message could be sent to stderr
    fprintf(stderr, "Failed to open log file /tmp/vim_parser_log.txt\n");
  }
}

/*
  Function to read and parse output from the created temp file by writeResultToTempFile for data exchange.
  It was meant to return what was read from the temp file, but it is now a redundant function.
*/
char *vim_parser(const char *combined_input) {
  if(combined_input == NULL) {
    log_message("Error: fn vim_parser: Received NULL string");
    // Return value must be freed by Vim
    return strdup("Error: fn vim_parser: Received NULL string");
  }

  // Process the input string and store the result in the global buffer
  processCompletionDataForVim(combined_input);
  // Dynamically allocate memory for tempFilePath
  size_t max_path_len = MAX_PATH_LENGTH;  // Reasonable size for temp path
  char *tempFilePath = (char *)malloc(max_path_len * sizeof(char));

  // Check for allocation failures
  if(!tempFilePath) {
    log_message("Error: fn vim_parser: Memory allocation failed for internal buffers");
    free(tempFilePath);
    // Return value must be freed by Vim
    return strdup("Error: Memory allocation failed for internal buffers");
  }

  // Construct the temporary file path
  snprintf(tempFilePath, max_path_len,
           "/tmp/code_connector_vim_return/code_connector_output_%d.txt",
           getpid());
  FILE *tempFile = fopen(tempFilePath, "r");

  if(tempFile == NULL) {
    log_message("Error: Temporary file not found");
    free(tempFilePath);
    // Return value must be freed by Vim
    return strdup("Error: Temporary file not found");
  }

  // Allocate memory to store the file content
  fseek(tempFile, 0, SEEK_END);
  long fileSize = ftell(tempFile);
  fseek(tempFile, 0, SEEK_SET);

  // Check for negative file size
  if(fileSize < 0) {
    log_message("Error: Invalid file size");
    fclose(tempFile);
    free(tempFilePath);
    // Return value must be freed by Vim
    return strdup("Error: Invalid file size");
  }

  // Allocate memory for file content (to be freed by Vim)
  char *fileContent = (char *)malloc((size_t)fileSize + 1);

  if(fileContent == NULL) {
    log_message("Error: Memory allocation failed for file content");
    fclose(tempFile);
    free(tempFilePath);
    // Return value must be freed by Vim
    return strdup("Error: Memory allocation failed for file content");
  }

  // Read the file content
  size_t items_read = fread(fileContent, (size_t)fileSize, 1, tempFile);

  if(items_read != 1) {
    log_message("Error: Failed to read file content");
    fclose(tempFile);
    free(tempFilePath);
    free(fileContent);
    // Return value must be freed by Vim
    return strdup("Error: Failed to read file content");
  }

  fileContent[fileSize] = '\0'; // Null-terminate the string
  fclose(tempFile);
  // Free internal buffers
  free(tempFilePath);
  // Return the file content (to be freed by Vim)
  /*printf( "DEBUG: fn vim_parser: File content: %s\n", fileContent);*/
  return fileContent;
}

// Pattern substitution and filtration part for the output derived from the LLVM

// Function to find function name in the string
static int extract_function_name(const char *str, const char **func_start, size_t *func_name_len) {
  if(!str) {
    fprintf(stderr, "Error: Null string in extract_function_name\n");
    return 0;
  }

  const char *start = str;

  while(*start && isspace(*start)) {
    start++;
  }

  const char *end = start;

  while(*end && !isspace(*end) && *end != '(') {
    end++;
  }

  *func_name_len = end - start;

  if(*func_name_len == 0) {
    fprintf(stderr, "Error: Empty function name\n");
    return 0;
  }

  *func_start = start;
  return 1;
}

// Modified function to find function call boundaries
// Currently, a redundant function
static int find_function_call(const char *str, const char *func_name, size_t func_name_len,
                              const char **call_start, const char **call_end,
                              const char **args_start, const char **args_end) {
  if(!str || !func_name) {
    fprintf(stderr, "Error: Null input in find_function_call\n");
    return 0;
  }

  const char *pos = str;
  const char *str_end = str + strlen(str);

  while((pos = strstr(pos, func_name)) != NULL) {
    // Verify this is actually the function name (not part of another word)
    if(pos > str && !isspace(*(pos - 1)) && *(pos - 1) != '(' && *(pos - 1) != ',') {
      pos++;
      continue;
    }

    *call_start = pos;
    pos += func_name_len;

    // Skip whitespace before parenthesis
    while(pos < str_end && isspace(*pos)) {
      pos++;
    }

    // If we have an opening parenthesis, find the matching closing one
    if(pos < str_end && *pos == '(') {
      int paren_count = 1;
      *args_start = pos + 1;
      pos++;

      while(pos < str_end && paren_count > 0) {
        if(*pos == '(') {
          paren_count++;
        }

        else if(*pos == ')') {
          paren_count--;
        }

        pos++;
      }

      if(paren_count != 0) {
        // Incomplete call - set args_end to end of string
        *args_end = str_end;
        *call_end = str_end;
        return 1;
      }

      *args_end = pos - 1;
      *call_end = pos;
      return 1;
    }

    // If no opening parenthesis or end of string, treat as incomplete call
    else {
      // Set args_start and args_end to the current position
      *args_start = pos;
      *args_end = pos;
      // Set call_end to the end of string
      *call_end = str_end;
      return 1;
    }
  }

  fprintf(stderr, "Error: Function name '%s' not found in source\n", func_name);
  return 0;
}

// Function to match function signatures. Pattern matching of functions from the Clang output.
static int match_function_signature(const char *source_args, const char *pattern_args) {
  // For pattern matching purposes, we'll accept any source args
  // since we're focusing on the pattern rather than strict signature matching
  return 1;
}

// Function to perform pattern substitution.
// Currently, a redundant function
char *substitute_function_pattern(const char *source, const char *pattern) {
  if(!source || !pattern) {
    fprintf(stderr, "Error: Null source or pattern\n");
    return NULL;
  }

  // Extract function name from pattern
  const char *pattern_func_start;
  size_t pattern_func_name_len;

  if(!extract_function_name(pattern, &pattern_func_start, &pattern_func_name_len)) {
    return NULL;
  }

  // Create function name string for searching
  char *pattern_func_name = (char *)malloc(pattern_func_name_len + 1);

  if(!pattern_func_name) {
    fprintf(stderr, "Error: Memory allocation failed for function name\n");
    return NULL;
  }

  strncpy(pattern_func_name, pattern_func_start, pattern_func_name_len);
  pattern_func_name[pattern_func_name_len] = '\0';
  // Find function call in source
  const char *source_call_start;
  const char *source_call_end;
  const char *source_args_start;
  const char *source_args_end;

  if(!find_function_call(source, pattern_func_name, pattern_func_name_len,
                         &source_call_start, &source_call_end,
                         &source_args_start, &source_args_end)) {
    free(pattern_func_name);
    return NULL;
  }

  // Find function call in pattern
  const char *pattern_call_start;
  const char *pattern_call_end;
  const char *pattern_args_start;
  const char *pattern_args_end;

  if(!find_function_call(pattern, pattern_func_name, pattern_func_name_len,
                         &pattern_call_start, &pattern_call_end,
                         &pattern_args_start, &pattern_args_end)) {
    free(pattern_func_name);
    return NULL;
  }

  // Calculate lengths with bounds checking
  size_t source_args_len = (source_args_end >= source_args_start) ?
                           (size_t)(source_args_end - source_args_start) : 0;
  size_t pattern_args_len = (pattern_args_end >= pattern_args_start) ?
                            (size_t)(pattern_args_end - pattern_args_start) : 0;
  char *source_args = (char *)malloc(source_args_len + 1);
  char *pattern_args = (char *)malloc(pattern_args_len + 1);

  if(!source_args || !pattern_args) {
    fprintf(stderr, "Error: Memory allocation failed for args\n");
    free(pattern_func_name);
    free(source_args);
    free(pattern_args);
    return NULL;
  }

  // Safe string copying
  if(source_args_len > 0 && source_args_start < source + strlen(source)) {
    strncpy(source_args, source_args_start, source_args_len);
  }

  source_args[source_args_len] = '\0';

  if(pattern_args_len > 0 && pattern_args_start < pattern + strlen(pattern)) {
    strncpy(pattern_args, pattern_args_start, pattern_args_len);
  }

  pattern_args[pattern_args_len] = '\0';
  // Since match_function_signature always returns 1, we don't need to check it
  free(source_args);
  free(pattern_args);
  // Calculate lengths with bounds checking
  size_t prefix_len = (source_call_start >= source) ?
                      (size_t)(source_call_start - source) : 0;
  size_t pattern_call_len = (pattern_call_end >= pattern_call_start) ?
                            (size_t)(pattern_call_end - pattern_call_start) : 0;
  size_t suffix_len = (source_call_end <= source + strlen(source)) ?
                      strlen(source_call_end) : 0;
  // Allocate result buffer
  char *result = (char *)malloc(prefix_len + pattern_call_len + suffix_len + 1);

  if(!result) {
    fprintf(stderr, "Error: Memory allocation failed for result\n");
    free(pattern_func_name);
    return NULL;
  }

  // Build result with safe copying
  size_t offset = 0;

  if(prefix_len > 0) {
    strncpy(result, source, prefix_len);
    offset += prefix_len;
  }

  if(pattern_call_len > 0) {
    strncpy(result + offset, pattern_call_start, pattern_call_len);
    offset += pattern_call_len;
  }

  if(suffix_len > 0) {
    strncpy(result + offset, source_call_end, suffix_len);
  }

  result[prefix_len + pattern_call_len + suffix_len] = '\0';
  free(pattern_func_name);
  return result;
}
