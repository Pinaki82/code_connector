// code_connector_shared.h
#ifndef __CODE_CONNECTOR_SHARED__
#define __CODE_CONNECTOR_SHARED__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>

#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
  #include <direct.h>
#else
  #include <sys/wait.h>
  #include <fcntl.h>
  #include <regex.h>
  #include <dirent.h>
  #include <unistd.h> // F_OK, realpath
  #include <libgen.h>  // dirname()
#endif

#define MAX_CACHED_PATHS 128 // For caching header file paths
#ifndef PATH_MAX
  #define PATH_MAX 4096
#endif

#define MAX_LINES 10000
#define MAX_PATH_LENGTH 1024
#define MAX_LINE_LENGTH 2048
#define MAX_OUTPUT 2097152  // 2 MB buffer for output
#define MAX_OUTPUT_LENGTH 2097152  // 2 MB buffer for output length
#define MAX_REGX_MATCHES 100 // Increased to 100 to capture all groups
#define EXTRA_BUFFER 100

// Cache structure to store include paths and CPU architecture
typedef struct {
  char project_dir[PATH_MAX];          // Directory where .ccls and compile_flags.txt were found
  char *include_paths[MAX_CACHED_PATHS]; // Cached include paths
  int include_path_count;              // Number of cached include paths
  char cpu_arch[MAX_LINE_LENGTH];      // Cached CPU architecture
  int is_valid;                        // Cache validity flag
  // int *path_allocated;  // Track which paths are allocated
} CodeCompletionCache;

#ifdef __cplusplus
extern "C" {
#endif


// Function prototypes (common to both platforms)
int create_default_config_files(const char *directory);

// Function to find .ccls and compile_flags.txt files
int findFiles(const char *path, char *found_at);

// Function to read and process include paths from files
void read_files(const char *file1, const char *file2, char **lines, int *count);

// Function to remove duplicate lines
void remove_duplicates(char **lines, int *count);

// Function to store lines
void store_lines(const char *file1, const char *file2, char **lines, char **sorted_lines, int *count);

int compare_strings(const void *a, const void *b);

// Function to get the clang target
int get_clang_target(char *output);

// Function to collect filename, line number, and column number for code completion
char *collect_code_completion_args(const char *filename, int line, int column);

char *execute_code_completion_command(const char *filename, int line, int column);

#if !defined(_WIN32)
char *filter_clang_output(const char *input);
#endif

#if defined(_WIN32)
char *filter_clang_output_mswin(const char *input, const char *pre_paren_text, const char *post_paren_text);
#endif

int execute_ccls_index(const char *directory);

// Function to split the input string into file path, line, and column
void split_input_string(const char *input, char *file_path, int *line, int *column);

char *processCompletionDataFromString(const char *vimInputString);

// Function to write the result to a temporary file and return the file path
char *writeResultToTempFile(const char *result);

// Wrapper function for Vim
void processCompletionDataForVim(const char *vimInputString);

// Global buffer to store the result
extern char global_result_buffer[MAX_OUTPUT];

// Cache related global variables -------------------------
// Global buffer to store the header file paths for the current project obtained from .ccls and compile_flags.txt files
// extern char global_buffer_header_paths[MAX_OUTPUT];
char global_buffer_header_paths[MAX_LINES][MAX_PATH_LENGTH];
// Global buffer to store the CPU architecture
extern char global_buffer_cpu_arc[MAX_OUTPUT];

// Global buffer to store the project directory, where the .ccls and the compile_flags.txt files were found
extern char global_buffer_project_dir[PATH_MAX];

// Global buffer to store the header file paths and the CPU architecture (temporary)
// extern char global_buffer_header_paths_cpu_arc[MAX_OUTPUT]; // Not needed anymore.
// ---------------------------- Cache related global variables

// Returns the global buffer for data exchange
char *transfer_global_buffer(void);

// Function to log messages to a file
void log_message(const char *message);

char *vim_parser(const char *combined_input);
char *substitute_function_pattern(const char *source, const char *pattern);

// Cache-related functions
void init_cache(void);
void clear_cache(void);
int is_cache_valid(const char *current_project_dir);
void update_cache(const char *project_dir, char **include_paths, int path_count, const char *cpu_arch);
char **get_cached_include_paths(int *count);

// Global buffers
extern char global_result_buffer[MAX_OUTPUT];
extern char global_buffer_project_dir[PATH_MAX];
extern char global_buffer_cpu_arc[MAX_OUTPUT];
extern char global_buffer_project_dir_monitor[PATH_MAX];
extern int global_project_dir_monitor_changed;
extern char global_buffer_current_file_dir[PATH_MAX];
extern char global_buffer_header_paths[MAX_LINES][MAX_PATH_LENGTH];

// Function to find function name in the string // Static functions (defined in each platform-specific file)
static int extract_function_name(const char *str, const char **func_start, size_t *func_name_len);
static int find_function_call(const char *str, const char *func_name, size_t func_name_len,
                              const char **call_start, const char **call_end,
                              const char **args_start, const char **args_end);
static int match_function_signature(const char *source_args, const char *pattern_args);
// char *substitute_function_pattern(const char *source, const char *pattern);

#ifdef __cplusplus
}

#endif
#endif // __CODE_CONNECTOR_SHARED__
