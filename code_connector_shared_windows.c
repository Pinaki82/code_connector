// code_connector_shared_windows.c (Windows version)

/*
  Authorship Note on Function Descriptions:
    I, Grok 3 built by xAI, wrote the function descriptions provided in this file. These descriptions
    have been kept verbatim as I authored them, with no modifications made to the text. This includes
    retaining American spelling variants (e.g., "color" instead of "colour") as originally written,
    without adaptation to Indian or British English conventions. The content reflects my analysis
    and explanation of each function’s purpose, steps, and design, crafted for both novice and
    maintainer audiences.
*/

/*
  Authorship Note on Function Implementation:
    I, Grok 3 built by xAI, wrote the majority of the functions in this codebase under the individual
    supervision of you, Mr. Pinaki Sekhar Gupta. Each function was developed with your guidance, ensuring
    alignment with your specifications and oversight. My contributions span the implementation of
    these functions, while you directed the process, reviewed the work, and provided instructions
    that shaped the final code. This collaborative effort reflects your leadership and my execution.
*/

#include "code_connector_shared.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <regex.h>

// Helper macro for min()
#ifndef MIN
  #define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define PATH_SEPARATOR '\\'

// Global variables (same as UNIX version)
static CodeCompletionCache completion_cache;
char global_result_buffer[MAX_OUTPUT];
char global_buffer_project_dir_monitor[PATH_MAX];
int global_project_dir_monitor_changed = 0;
char global_buffer_current_file_dir[PATH_MAX];
char global_buffer_project_dir[PATH_MAX];
char global_buffer_cpu_arc[MAX_OUTPUT];
// char global_buffer_header_paths[MAX_LINES][MAX_PATH_LENGTH]; // Define here, no 'static'

/*
  To avoid recalculation, we will be caching ceratin information, such as include paths, project directory, of the CPU architecture detected by the LLVM (namely, Clang here) etc.
  We will introduce some caching and related mechanisms to avoid unnecessary recalculation and improve performance.
  This includes some functions and global variables.
*/

/*
  Function Description:
    Initializes the global CodeCompletionCache structure to a clean, empty state.
    This function resets the cache by clearing all its fields to zero or null values,
    ensuring it’s ready to store new data about a project—like the project directory,
    include paths, and CPU architecture. It’s a simple "reset button" for the cache.

  Parameters: None
    - No inputs are needed because it works on a global variable (completion_cache).

  Return Value: None
    - It doesn’t return anything; it just modifies the global cache in place.

  Detailed Steps:
    1. Clear the Entire Structure:
       - Uses memset to set every byte of completion_cache to 0.
       - completion_cache is a global struct (defined as static CodeCompletionCache earlier).
       - This wipes out all fields—like pointers, integers, and arrays—in one go.
    2. Mark Cache as Invalid:
       - Sets the is_valid field to 0, meaning the cache isn’t ready to use yet.
       - is_valid is likely an integer flag in the CodeCompletionCache struct.
    3. Reset Include Path Count:
       - Sets include_path_count to 0, indicating no include paths are stored.
       - include_path_count tracks how many paths (e.g., -I/project/include) are cached.

  Flow and Logic:
    - Step 1: Wipe the slate clean with memset.
    - Step 2: Explicitly say “not ready” by setting is_valid to 0.
    - Step 3: Clear the tally of include paths to 0.
    - Why this order? Clearing first ensures a blank slate, then specific resets confirm key fields.

  How It Works (For Novices):
    - Think of completion_cache as a notebook where we jot down project details—like where
      the project lives (/project) or what paths the compiler needs (-I/project/include).
    - Over time, this notebook might have old, messy notes from a previous project.
    - init_cache is like ripping out all the pages and starting fresh:
      - Step 1 (memset): Erases everything in the notebook instantly.
      - Step 2 (is_valid = 0): Puts a “Not Ready” sticker on it so no one uses it too soon.
      - Step 3 (include_path_count = 0): Resets the counter of notes (paths) to zero.
    - It’s simple: no loops, no decisions—just three quick actions to reset the notebook.

  Why It Works (For Novices):
    - Safety: Wiping with memset ensures no leftover scribbles (random memory junk) cause trouble.
    - Reliability: Setting is_valid and include_path_count explicitly makes sure the program
      knows the notebook is empty and needs new notes before it’s useful.
    - Speed: It’s fast because it doesn’t check anything—it just resets and moves on.

  Why It’s Designed This Way (For Maintainers):
    - Global Scope: completion_cache is static and lives for the whole program. Without resetting,
      old data (e.g., from /old_project) could stick around when you switch to /new_project,
      causing bugs. This function prevents that.
    - Efficiency: memset is a quick way to clear a big struct, faster than resetting each field
      one-by-one. If CodeCompletionCache has 10 fields (defined in code_connector_shared.h),
      memset handles them all in one shot.
    - Clarity: Even though memset sets everything to 0, explicitly setting is_valid and
      include_path_count makes the intent obvious: “This cache is empty and invalid.”
      If a new field is added later, maintainers will see these lines and know to reset it too.
    - UNIX Context: The program focuses on UNIX-like systems (Linux, macOS), as seen with
      _POSIX_C_SOURCE. Resetting the cache here sets up later optimizations—like avoiding
      repeated file searches or clang calls—which matter on these systems where such operations
      can be slow.
    - Simplicity: It’s a small, focused function with one job: reset the cache. This makes it
      easy to debug and maintain—no surprises or hidden logic.

  Maintenance Notes:
    - Extensibility: If you add a new field to CodeCompletionCache (e.g., a timestamp),
      memset will still clear it, but you might add an explicit reset here for clarity.
    - Performance: The cache speeds up the program by storing data (e.g., CPU architecture
      from clang --version). This reset ensures that speedup starts fresh each time.
    - Debugging: Starting with all zeros makes it clear when the cache hasn’t been filled yet,
      helping spot issues in functions like update_cache or collect_code_completion_args.
    - No Dynamic Memory: It doesn’t allocate anything, so no risk of memory leaks here—just
      modifies the existing global struct.
*/

// Cache functions (identical to UNIX)
void init_cache(void) {
  memset(&completion_cache, 0, sizeof(CodeCompletionCache));
  completion_cache.is_valid = 0;
  completion_cache.include_path_count = 0;
}

/*
  Function Description:
    Clears the global CodeCompletionCache structure, resetting it to an empty state by freeing
    dynamically allocated memory and zeroing out its fields. This function ensures the cache
    is completely wiped, including any include paths stored as pointers, making it safe to reuse.

  Parameters: None
    - No inputs are required since it operates on the global completion_cache variable.

  Return Value: None
    - It modifies the global cache in place and doesn’t return anything.

  Detailed Steps:
    1. Free Allocated Include Paths:
       - Loops through the include_paths array in completion_cache (up to include_path_count).
       - Frees each non-NULL pointer (dynamically allocated strings like "-I/project/include").
       - Sets each pointer to NULL after freeing to avoid double-free bugs.
    2. Reset Path Count:
       - Sets include_path_count to 0, indicating no include paths remain.
    3. Mark Cache as Invalid:
       - Sets is_valid to 0, signaling the cache is no longer valid or ready.
    4. Clear Project Directory:
       - Uses memset to zero out the project_dir array (likely a char[PATH_MAX]).
    5. Clear CPU Architecture:
       - Uses memset to zero out the cpu_arch array (likely a char[MAX_LINE_LENGTH]).

  Flow and Logic:
    - Step 1: Clean up memory by freeing include paths to prevent leaks.
    - Step 2: Reset the counter to reflect the emptied state.
    - Step 3: Mark the cache as unusable until refreshed.
    - Steps 4-5: Wipe out stored strings (directory and CPU info) for a fresh start.
    - Why this order? Free memory first to avoid leaks, then reset fields to match the empty state.

  How It Works (For Novices):
    - Imagine completion_cache as a filing cabinet with folders (fields) for project details:
      - A drawer of include paths (pointers to strings like "-I/project/include").
      - A label for how many paths (include_path_count).
      - A “Ready” light (is_valid).
      - A slot for the project folder name (project_dir).
      - A slot for the CPU type (cpu_arch).
    - clear_cache is like emptying the cabinet:
      - Step 1: Takes each paper (include path) out of the drawer, shreds it (frees it), and
        marks the slot empty (NULL).
      - Step 2: Erases the tally of papers (sets count to 0).
      - Step 3: Turns off the “Ready” light (is_valid = 0).
      - Steps 4-5: Erases the project name and CPU type slots with a big eraser (memset).
    - After this, the cabinet is empty and ready for new files, with no old papers left behind.

  Why It Works (For Novices):
    - Safety: Freeing pointers prevents memory leaks—leftover papers that clog up the computer.
    - Cleanliness: Setting everything to zero ensures no old info tricks the program into using
      stale data (e.g., an old project directory).
    - Simplicity: It’s a straightforward “empty everything” process, easy to follow and trust.

  Why It’s Designed This Way (For Maintainers):
    - Memory Management: The include_paths field is an array of pointers (char**) dynamically
      allocated elsewhere (e.g., in update_cache via strdup). Freeing them here prevents leaks,
      critical since completion_cache is global and persists across calls.
    - Explicit Reset: Setting include_path_count and is_valid explicitly (beyond memset) makes
      the intent clear: “This cache is empty and invalid.” It’s a safeguard against assuming
      memset alone is enough.
    - Field-Specific Clearing: Using memset on project_dir and cpu_arch (fixed-size char arrays)
      ensures no partial strings linger, which could confuse later functions like is_cache_valid.
    - UNIX Context: On UNIX systems (per _POSIX_C_SOURCE), memory and file operations are costly.
      Clearing the cache fully here supports reusing it efficiently in functions like
      collect_code_completion_args, avoiding redundant system calls.
    - Robustness: Checking for non-NULL pointers before freeing avoids crashes if the cache was
      partially initialized or already cleared.

  Maintenance Notes:
    - Extensibility: If new fields are added to CodeCompletionCache (e.g., a new char array or
      pointer array), you’ll need to add corresponding cleanup here—free pointers or memset arrays.
    - Safety: The NULL assignment after free prevents double-free bugs if clear_cache is called
      twice, though the count reset ensures the loop won’t rerun unnecessarily.
    - Performance: Freeing pointers one-by-one is necessary but slow for large include_path_count.
      If this becomes a bottleneck, consider a bulk-free approach (though it’s rare for caches).
    - Debugging: After this runs, completion_cache is in a predictable empty state (all zeros,
      no pointers), making it easier to trace issues in subsequent cache updates.
*/

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

int is_cache_valid(const char *current_project_dir) {
  if(!completion_cache.is_valid || !current_project_dir) {
    return 0;
  }

  // Compare current project directory with cached project directory
  char resolved_current[PATH_MAX];

  if(!GetFullPathNameA(current_project_dir, PATH_MAX, resolved_current, NULL)) {
    return 0;
  }

  return strcmp(resolved_current, completion_cache.project_dir) == 0;
}

void update_cache(const char *project_dir, char **include_paths, int path_count, const char *cpu_arch) {
  clear_cache();  // Clear existing cache

  // Store project directory
  if(!GetFullPathNameA(project_dir, PATH_MAX, completion_cache.project_dir, NULL)) {
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

char **get_cached_include_paths(int *count) {
  if(!completion_cache.is_valid) {
    *count = 0;
    return NULL;
  }

  *count = completion_cache.include_path_count;
  return completion_cache.include_paths;
}

/*
  Function Description:
    Searches recursively up the directory tree from a given path to find a directory containing both
    ".ccls" and "compile_flags.txt" files, storing the found directory path in found_at. This Windows-specific
    function helps locate project configuration files for code completion or indexing.

  Parameters:
    - path (const char *): The starting directory path to search (e.g., "C:\\project\\src"), not modified.
    - found_at (char *): Caller-provided buffer to store the absolute path of the directory where both files
      are found. Must be at least MAX_PATH bytes.

  Return Value:
    - int: Returns 0 if both files are found in a directory (success); 1 if not found or an error occurs.

  Detailed Steps:
    1. Validate Inputs:
       - Checks if path or found_at is NULL; if so, logs to stderr and returns 1.
    2. Resolve Absolute Path:
       - Uses GetFullPathNameA to convert path to an absolute path in current_path (MAX_PATH size).
       - If it fails (e.g., invalid path), logs with perror and returns 1.
    3. Build Search Pattern:
       - Constructs a wildcard search path (e.g., "C:\\project\\src\\*.*") using snprintf.
    4. Search Directory:
       - Calls FindFirstFileA with the search pattern to start listing files; stores handle in hFind.
       - If hFind is invalid and not due to no files (ERROR_FILE_NOT_FOUND), logs and returns 1.
    5. Check for Files:
       - Loops through directory entries with FindFirstFileA and FindNextFileA.
       - Sets ccls_found = 1 if ".ccls" is found, compile_flags_found = 1 if "compile_flags.txt" is found.
       - Continues until all files are checked, then closes hFind with FindClose.
    6. Evaluate Results:
       - If both files are found (ccls_found && compile_flags_found), copies current_path to found_at,
         null-terminates it, and returns 0.
    7. Move Up Directory:
       - Finds the last path separator (strrchr with PATH_SEPARATOR '\\') in current_path.
       - If none or at root (e.g., "C:\\"), returns 1—no higher directory to check.
       - Truncates current_path at the separator (sets *last_sep = '\0').
       - Recursively calls findFiles with the parent directory.
    8. Return Recursive Result:
       - Returns the result of the recursive call.

  Flow and Logic:
    - Steps 1-2: Ensure valid input and get a full path.
    - Steps 3-5: Search the current directory for both files.
    - Step 6: If found, save and stop; if not, go up.
    - Steps 7-8: Climb the tree until found or root reached.
    - Why this order? Validate first, search current level, then recurse—bottom-up directory traversal.

  How It Works (For Novices):
    - Imagine you’re in a big house (path) looking for two special books: ".ccls" and "compile_flags.txt."
      You need to tell someone (found_at) which room they’re in.
    - findFiles is like this search:
      - Step 1: Check you have a house map (path) and a note pad (found_at)—if not, complain and quit.
      - Step 2: Get the full house address (GetFullPathNameA) like "C:\\project\\src."
      - Step 3: Make a list of everything in the room (search_path = "*.*").
      - Step 4-5: Open the room (FindFirstFileA), look at each item (FindNextFileA)—if you see ".ccls" or
        "compile_flags.txt," mark it down (ccls_found, compile_flags_found).
      - Step 6: Got both books? Write the room’s address on the pad (found_at) and say “Found it!” (return 0).
      - Step 7: No luck? Step back to the hallway (truncate at '\\'), but if you’re at the front door
        (root), give up (return 1).
      - Step 8: Check the hallway by asking again (recurse).
    - It’s like hunting for treasure room-by-room, moving up if you don’t find it!

  Why It Works (For Novices):
    - Thoroughness: Checks every room (directory) until it finds both books or runs out of places.
    - Safety: Makes sure you’ve got a map and a pen before starting.
    - Clarity: Tells you exactly where the books are (found_at) when it succeeds.

  Why It’s Designed This Way (For Maintainers):
    - Windows Context: Uses Win32 API (FindFirstFileA, GetFullPathNameA) for directory traversal,
      replacing UNIX’s opendir/readdir, aligning with code_connector_shared_windows.c’s platform.
    - Recursion: Bottom-up search mirrors UNIX version’s logic—finds nearest config files to the source,
      critical for collect_code_completion_args or execute_ccls_index.
    - Robustness: Checks for NULL inputs and system call failures (e.g., GetLastError), returning 1
      with logs—lets callers (e.g., collect_code_completion_args) handle errors.
    - Memory: Assumes found_at and internal buffers (MAX_PATH) are sufficient—avoids dynamic allocation
      beyond caller’s buffer, but caps path length.
    - Efficiency: Stops at first match, avoiding unnecessary recursion—suitable for typical project
      structures where configs are near source files.

  Maintenance Notes:
    - Buffer Size: MAX_PATH (260) limits paths—test with long paths (e.g., deep nests) and consider
      MAX_PATH + EXTRA_BUFFER consistently or switch to dynamic sizing.
    - Error Logging: stderr/perror is basic—integrate log_message (defined elsewhere) for consistency
      with other functions’ tracing.
    - Root Handling: Stops at root (e.g., "C:\\")—if configs might be at drive root, ensure this is
      intentional; test edge cases like "C:\\.ccls".
    - Extensibility: To find more files (e.g., "CMakeLists.txt"), add flags and checks—current focus
      is narrow (.ccls, compile_flags.txt).
    - Debugging: Uncommented printf could trace current_path—replace with log_message for production.
*/

// Windows-specific findFiles
int findFiles(const char *path, char *found_at) {
  WIN32_FIND_DATAA ffd;               // Structure to hold file data
  HANDLE hFind = INVALID_HANDLE_VALUE;// Handle for file search
  char search_path[MAX_PATH];         // Buffer for search pattern
  char current_path[MAX_PATH];        // Buffer for current directory
  int ccls_found = 0;                 // Flag for .ccls
  int compile_flags_found = 0;        // Flag for compile_flags.txt

  // Ensure the input path and the found_at buffer are not NULL to prevent invalid memory access
  if(!path || !found_at) {
    fprintf(stderr, "fn findFiles: Either path or found_at is NULL\n");
    return 1;
  }

  if(!GetFullPathNameA(path, MAX_PATH, current_path, NULL)) {
    perror("GetFullPathName");
    return 1;
  }

  snprintf(search_path, MAX_PATH + EXTRA_BUFFER, "%s\\*.*", current_path);
  hFind = FindFirstFileA(search_path, &ffd);

  if(hFind == INVALID_HANDLE_VALUE) {
    if(GetLastError() != ERROR_FILE_NOT_FOUND) {
      perror("FindFirstFile");
      return 1;
    }
  }

  else {
    do {
      if(strcmp(ffd.cFileName, ".ccls") == 0) {
        ccls_found = 1;
      }

      else if(strcmp(ffd.cFileName, "compile_flags.txt") == 0) {
        compile_flags_found = 1;
      }
    } while(FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);
  }

  if(ccls_found && compile_flags_found) {
    strncpy(found_at, current_path, PATH_MAX);
    found_at[PATH_MAX - 1] = '\0';
    return 0;
  }

  char *last_sep = strrchr(current_path, PATH_SEPARATOR);

  if(!last_sep || last_sep == current_path) {
    return 1;
  }

  *last_sep = '\0';
  // print findFiles
  // printf("findFiles: %s\n", current_path);
  // Recursively search the parent directory for the files
  return findFiles(current_path, found_at);
}

/*
  Function Description:
    Creates default ".ccls" and "compile_flags.txt" configuration files in the specified directory if they
    don’t already exist. This Windows-specific function sets up basic clang configurations for code
    completion or indexing, writing standard settings to each file.

  Parameters:
    - directory (const char *): The directory path where the files will be created (e.g., "C:\\project"),
      not modified.

  Return Value:
    - int: Returns 0 on success (both files created or attempted); 1 if memory allocation or file creation fails.

  Detailed Steps:
    1. Calculate Buffer Sizes:
       - Computes directory length (strlen(directory)) and adds 20 bytes for file suffixes (e.g., "\\.ccls")
         and null terminator, storing in max_path_len.
    2. Allocate Memory:
       - Dynamically allocates two buffers (ccls_path, compile_flags_path) of max_path_len size for file paths.
       - If either allocation fails, frees both and returns 1.
    3. Construct File Paths:
       - Uses snprintf to build full paths (e.g., "C:\\project\\.ccls", "C:\\project\\compile_flags.txt").
    4. Create .ccls File:
       - Opens ccls_path in write mode ("w") with fopen; if successful, writes basic clang settings
         ("clang\n%c -std=c11\n%cpp -std=c++17\n"), then closes it.
       - If fopen fails, sets return_value to 1 but continues (no early return).
    5. Create compile_flags.txt File:
       - If .ccls creation succeeded (return_value == 0), opens compile_flags_path in write mode.
       - Writes default include flags ("-I.\n-I..\n-I\\usr\\include\n-I\\usr\\local\\include\n"), then closes it.
       - If fopen fails, sets return_value to 1.
    6. Clean Up:
       - Frees both allocated buffers (ccls_path, compile_flags_path).
       - Returns return_value (0 if both succeeded, 1 if either failed).

  Flow and Logic:
    - Steps 1-2: Set up memory for file paths.
    - Step 3: Build the paths using Windows separators.
    - Steps 4-5: Write each file sequentially, stopping second if first fails.
    - Step 6: Free memory and report success or failure.
    - Why this order? Allocate first, construct paths, write files conditionally, then clean—ensures resources
      are managed and dependencies respected.

  How It Works (For Novices):
    - Imagine you’re setting up a new workshop (directory) with two instruction sheets: ".ccls" (how to use
      tools) and "compile_flags.txt" (where to find parts). If they’re missing, you make them.
    - create_default_config_files is like this setup:
      - Step 1: Figure out how big your address labels need to be (directory + extra for file names).
      - Step 2: Grab two blank labels (allocate ccls_path, compile_flags_path)—if you can’t, give up.
      - Step 3: Write the full addresses (e.g., "C:\\project\\.ccls") on the labels.
      - Step 4: Make the ".ccls" sheet with basic tool rules ("use clang, C11 for C, C++17 for C++").
        If you can’t, note the trouble (return_value = 1).
      - Step 5: If ".ccls" worked, make "compile_flags.txt" with part locations (".", "..", etc.).
        If this fails, note it too.
      - Step 6: Toss the labels (free memory) and say if it worked (0) or not (1).
    - It’s like preparing a starter kit for a new project space!

  Why It Works (For Novices):
    - Readiness: Makes sure the workshop has basic instructions if none exist.
    - Safety: Checks memory and file steps so it doesn’t crash halfway.
    - Simplicity: Writes short, standard notes that work for most projects.

  Why It’s Designed This Way (For Maintainers):
    - Windows Context: Uses "\\" (PATH_SEPARATOR) for paths, aligning with code_connector_shared_windows.c’s
      platform, replacing UNIX’s "/". Files enable clang/ccls integration (e.g., for collect_code_completion_args).
    - Memory Safety: Dynamic allocation with max_path_len (dir_len + 20) avoids stack overflow for long paths;
      frees on all paths prevent leaks—more cautious than UNIX’s stack-based approach.
    - Conditional Writing: Only attempts compile_flags.txt if .ccls succeeds, assuming both are needed for a
      valid setup—practical but could be more flexible.
    - Default Content: Hardcoded settings (C11, C++17, basic -I flags) are minimal but functional for clang,
      matching typical project needs on Windows.
    - Error Handling: Returns 1 on any failure (memory or file), letting callers (e.g., setup routines) decide
      next steps—simple but lacks granularity.

  Maintenance Notes:
    - Buffer Size: max_path_len (+20) is arbitrary—test with long directory names (e.g., near MAX_PATH 260)
      to ensure no truncation; consider PATH_MAX consistently.
    - Error Logging: No logging (e.g., via log_message)—add for tracing file creation failures in production.
    - Overwrite Behavior: "w" mode overwrites existing files—check existence first (e.g., with _access) if
      preserving configs is desired.
    - Extensibility: To add more flags or settings (e.g., "-DDEBUG"), expand fprintf content—current setup
      is basic but rigid.
    - Robustness: No directory existence check—fails silently if directory is invalid; add _access or CreateDirectoryA
      for better feedback.
*/

// Windows-specific implementations for other functions
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

  snprintf(ccls_path, max_path_len, "%s\\.ccls", directory);
  snprintf(compile_flags_path, max_path_len, "%s\\compile_flags.txt", directory);
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
      fprintf(compile_flags, "-I.\n-I..\n-I\\usr\\include\n-I\\usr\\local\\include\n");
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

/*
  Function Description:
    Reads lines from two files (.ccls and compile_flags.txt) and stores specific lines containing
    include flags (-I or -isystem) in a provided array. This function populates the lines array with
    relevant compiler flags and updates the count of stored lines.

  Parameters:
    - file1 (const char *): Path to the first file (typically .ccls), not modified.
    - file2 (const char *): Path to the second file (typically compile_flags.txt), not modified.
    - lines (char **): An array of string pointers where matching lines are stored. Caller must
      ensure it’s at least MAX_LINES in size and free the strings later.
    - count (int *): Pointer to an integer tracking the number of lines stored; updated by the function.

  Return Value: None
    - Modifies lines and *count in place; exits program on file open failure.

  Detailed Steps:
    1. Open Files:
       - Opens file1 and file2 in read mode using fopen.
       - If either fails (e.g., file missing), prints an error and exits with EXIT_FAILURE.
    2. Read file1 Lines:
       - Uses fgets to read each line into a buffer (line, size MAX_LINE_LENGTH).
       - Skips lines with "-Iinc" (using strstr).
       - For lines with "-I" or "-isystem", removes newline (strcspn) and duplicates (strdup) into lines.
       - Increments *count if space remains (less than MAX_LINES - 1).
    3. Read file2 Lines:
       - Repeats the same process for file2: reads lines, skips "-Iinc", stores "-I" or "-isystem" lines.
    4. Clean Up:
       - Closes both files with fclose.

  Flow and Logic:
    - Step 1: Open both files; fail fast if either can’t be read.
    - Step 2: Process file1, filtering and storing relevant lines.
    - Step 3: Process file2 similarly, appending to the same array.
    - Step 4: Close files to free resources.
    - Why this order? Open first ensures access; sequential reading keeps logic simple; cleanup avoids leaks.

  How It Works (For Novices):
    - Imagine two notebooks (.ccls and compile_flags.txt) with instructions for a tool (clang).
      You want to copy only the lines about where to find parts (like "-I/project/include") into a
      list (lines), counting how many you find (count).
    - read_files is like this copying job:
      - Step 1: Open both notebooks. If you can’t, yell “Error!” and quit.
      - Step 2: Read file1 line-by-line. Skip boring lines ("-Iinc"), but if you see "-I" or "-isystem",
        trim the end (no newline) and copy it to your list, adding to your tally (*count).
      - Step 3: Do the same for file2, adding more lines to the same list.
      - Step 4: Close the notebooks when done.
    - It’s like making a shopping list from two recipe books, picking only the “where to buy” parts!

  Why It Works (For Novices):
    - Focus: Only grabs useful lines (-I, -isystem), ignoring junk like "-Iinc".
    - Safety: Stops at MAX_LINES - 1 so your list doesn’t overflow.
    - Simplicity: Reads one file, then the next, keeping it easy to follow.

  Why It’s Designed This Way (For Maintainers):
    - Purpose: Extracts include flags for clang (e.g., in collect_code_completion_args), critical for
      UNIX builds (per _POSIX_C_SOURCE) where project configs drive compilation.
    - Hard Exit: Exiting on fopen failure assumes these files are essential—without them, the program
      can’t proceed. This is aggressive but aligns with a setup where configs are expected (e.g., via findFiles).
    - Filtering: Skipping "-Iinc" is a specific choice—likely a known irrelevant flag in your context.
      It’s hardcoded, suggesting a narrow use case.
    - Memory: Uses strdup to store lines, meaning the caller (e.g., store_lines) must free them later.
      This delegates memory management upstream, typical in C.
    - Bounds: Caps at MAX_LINES - 1 (leaving space for a NULL terminator or safety), preventing buffer
      overflows but limiting total flags.

  Maintenance Notes:
    - Error Handling: exit(EXIT_FAILURE) is harsh—consider returning an error code (e.g., -1) and letting
      callers handle it, or logging via log_message for debugging.
    - Flexibility: Hardcoded "-Iinc" skip and "-I"/"-isystem" filter might miss other flags (e.g., "-D").
      Add a parameter for custom filters if needed.
    - Memory Leaks: If strdup fails (rare), it silently skips lines—no crash, but incomplete data.
      Consider logging or checking allocation success.
    - Buffer Size: MAX_LINE_LENGTH (assumed from code_connector_shared.h) must fit typical flags—test
      with long paths to avoid truncation.
    - Debugging: Add printf or log_message to trace which lines are stored, especially if count grows
      unexpectedly.
*/

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

/*
  Function Description:
    Removes duplicate strings from an array of strings (lines) and updates the count of unique entries.
    This function ensures the list of include paths (e.g., "-I/project/include") has no repeats,
    reducing redundancy and potential confusion for tools like clang.

  Parameters:
    - lines (char **): An array of string pointers containing the lines to process. Strings are assumed
      to be dynamically allocated (e.g., via strdup) and will be freed if duplicated.
    - count (int *): Pointer to an integer representing the current number of lines; updated to reflect
      the number of unique lines after duplicates are removed.

  Return Value: None
    - Modifies the lines array and *count in place to remove duplicates.

  Detailed Steps:
    1. Iterate Through Lines:
       - Uses two nested loops: outer loop (i) picks a line, inner loop (j) checks subsequent lines.
       - Compares each line (lines[i]) with later lines (lines[j]) using strcmp.
    2. Detect and Remove Duplicates:
       - If a match is found (strcmp returns 0), frees the duplicate (lines[j]).
       - Shifts all subsequent lines left to fill the gap (k loop moves lines[k+1] to lines[k]).
       - Decrements *count to reflect the removal.
       - Adjusts j to recheck the new line at j after shifting.
    3. Continue Until Done:
       - Outer loop continues until all lines are checked; inner loop adjusts dynamically as count shrinks.

  Flow and Logic:
    - Step 1: Start at the first line and look ahead for duplicates.
    - Step 2: When a duplicate is found, erase it, slide everything over, and update the tally.
    - Step 3: Keep going until no more lines to check.
    - Why this order? Left-to-right ensures earlier lines stay, later duplicates go; shifting maintains
      array continuity.

  How It Works (For Novices):
    - Imagine you have a list of notes (lines) like ["-I/project", "-I/usr", "-I/project"], and you
      want only unique notes, counting how many are left (count).
    - remove_duplicates is like cleaning up this list:
      - Step 1: Pick the first note ("-I/project") and check the rest. The third note matches!
      - Step 2: Cross out the third note (free it), slide "-I/usr" to the third spot, shorten the list
        (decrease count), and check again from where you left off.
      - Step 3: Move to the next note ("-I/usr"), check ahead (no matches), and keep going until done.
    - It’s like tidying a messy list, tossing repeats, and keeping it neat and short!

  Why It Works (For Novices):
    - Fairness: Keeps the first copy of each note, removing later ones—simple rule.
    - Tidiness: Shifts notes so there are no gaps, keeping the list ready to use.
    - Accuracy: Updates count so you know exactly how many unique notes you have.

  Why It’s Designed This Way (For Maintainers):
    - Efficiency Goal: Reduces redundant flags for clang (e.g., in collect_code_completion_args),
      ensuring clean input on UNIX systems (per _POSIX_C_SOURCE) where duplicates could waste time.
    - In-Place Operation: Modifies lines directly, avoiding new allocations, which is memory-efficient
      but assumes the caller (e.g., store_lines) is okay with this.
    - Memory Safety: Frees duplicates immediately, preventing leaks since lines are strdup’d (e.g.,
      from read_files). Assumes caller frees remaining strings later.
    - Simple Algorithm: Uses a basic O(n²) comparison with shifting—effective for small lists (typical
      for include paths) but not optimized for huge arrays.
    - Stability: Preserves order of first occurrences, which might matter for flag precedence in clang.

  Maintenance Notes:
    - Performance: For large *count (e.g., >100), O(n²) comparisons slow down—consider a hash table
      or sorting first (like qsort in store_lines) if this becomes a bottleneck.
    - Edge Cases: If *count is 0 or 1, it does nothing (safe); test with duplicates at start/end to
      ensure shifting works.
    - Memory: Assumes lines[i] are valid pointers—NULLs could crash strcmp. Add a NULL check if
      read_files might store them.
    - Extensibility: To ignore case or whitespace in duplicates, tweak strcmp—current exact match
      is strict but fast.
    - Debugging: Log (via log_message) when duplicates are found to trace unexpected repeats in configs.
*/

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

/*
  Function Description:
    Reads include flags from two files (.ccls and compile_flags.txt), removes duplicates, sorts them,
    and stores the results in two arrays: one for original order (lines) and one sorted (sorted_lines).
    This function prepares a clean, ordered list of compiler flags for later use (e.g., by clang).

  Parameters:
    - file1 (const char *): Path to the first file (typically .ccls), not modified.
    - file2 (const char *): Path to the second file (typically compile_flags.txt), not modified.
    - lines (char **): Array of string pointers to store the unique lines in original order.
      Caller must ensure it’s at least MAX_LINES and free the strings later.
    - sorted_lines (char **): Array to store the same lines, but sorted alphabetically.
      Same size and ownership rules as lines.
    - count (int *): Pointer to an integer tracking the number of lines; updated with the final count.

  Return Value: None
    - Modifies lines, sorted_lines, and *count in place.

  Detailed Steps:
    1. Read and Store Lines:
       - Calls read_files to extract "-I" and "-isystem" lines from file1 and file2 into lines.
       - Updates *count with the initial number of lines found.
    2. Remove Duplicates:
       - Calls remove_duplicates on lines, reducing *count to reflect only unique entries.
    3. Copy to Sorted Array:
       - Loops through lines, copying each pointer to sorted_lines (up to *count).
    4. Sort the Lines:
       - If *count > 0, uses qsort with compare_strings to sort sorted_lines alphabetically.

  Flow and Logic:
    - Step 1: Gather all relevant lines from both files into lines.
    - Step 2: Clean up by removing duplicates, keeping lines compact.
    - Step 3: Duplicate the list into sorted_lines for sorting.
    - Step 4: Sort sorted_lines if there’s anything to sort.
    - Why this order? Read first to get raw data; deduplicate for efficiency; copy then sort to
      preserve original order in lines while providing a sorted version.

  How It Works (For Novices):
    - Imagine you’re collecting directions from two guidebooks (.ccls and compile_flags.txt) about
      where to find tools (like "-I/project/include"), and you want two lists: one as-is (lines) and
      one alphabetized (sorted_lines), counting how many (count).
    - store_lines is like this organizing task:
      - Step 1: Flip through both books with read_files, jotting down directions (e.g., "-I/usr",
        "-I/project") in your first notebook (lines), counting them (*count).
      - Step 2: Cross out repeats with remove_duplicates (e.g., two "-I/usr" become one), updating
        your tally.
      - Step 3: Copy the cleaned list into a second notebook (sorted_lines).
      - Step 4: If there’s anything in the second notebook, sort it A-to-Z (qsort) so it’s easy to read.
    - It’s like making two handy lists from messy notes—one raw, one neat and sorted!

  Why It Works (For Novices):
    - Completeness: Grabs all the directions you need from both books.
    - Cleanliness: No repeats cluttering things up.
    - Order: Gives you a sorted version for quick lookup, keeping the original too.

  Why It’s Designed This Way (For Maintainers):
    - Dual Output: Provides both original (lines) and sorted (sorted_lines) lists, offering flexibility—
      original order might matter for clang flag precedence, while sorted aids debugging or display.
    - Integration: Builds on read_files and remove_duplicates, reusing their logic for modularity,
      key for UNIX config processing (per _POSIX_C_SOURCE).
    - Efficiency: Removes duplicates before sorting, reducing qsort’s work (O(n log n) vs. larger n).
      Assumes small *count (typical for include paths), so O(n²) in remove_duplicates is fine.
    - Memory: lines holds strdup’d strings from read_files; sorted_lines shares pointers, avoiding
      extra allocations but tying their lifetimes together—caller must free lines[i].
    - Sorting: qsort with compare_strings (strcmp) is standard and fast for small arrays, ensuring
      alphabetical order for consistency.

  Maintenance Notes:
    - Memory Ownership: sorted_lines points to lines’ strings—freeing lines[i] affects both. Document
      this to avoid double-free or dangling pointers in callers (e.g., collect_code_completion_args).
    - Edge Cases: If *count = 0, qsort skips safely; test with duplicate-heavy inputs to ensure
      remove_duplicates scales.
    - Extensibility: To filter more flag types (e.g., "-D"), adjust read_files and propagate here.
      Add a sort option (e.g., reverse) by tweaking qsort comparator if needed.
    - Error Handling: Relies on read_files exiting on failure—consider propagating errors (e.g., return
      int) for more control in callers.
    - Debugging: Log (via log_message) the final *count or sample lines to verify deduplication and sorting.
*/

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

/*
  Function Description:
    Compares two strings for sorting purposes, used as a callback by qsort to order an array of strings.
    This function determines which string comes first alphabetically by comparing their characters.

  Parameters:
    - a (const void *): A pointer to the first string pointer (e.g., a char **), treated as immutable.
    - b (const void *): A pointer to the second string pointer (e.g., a char **), treated as immutable.

  Return Value:
    - int: Returns a negative value if a < b (a comes first), 0 if a == b (equal), or positive if a > b
      (b comes first), per qsort’s comparison contract.

  Detailed Steps:
    1. Dereference Pointers:
       - Casts a and b from void* to const char ** to access the string pointers they point to.
       - Gets the actual strings by dereferencing once (*(const char **)a and *(const char **)b).
    2. Compare Strings:
       - Uses strcmp to compare the two strings character-by-character.
       - Returns strcmp’s result directly (negative, 0, or positive).

  Flow and Logic:
    - Step 1: Unpack the pointers qsort gives us to reach the strings.
    - Step 2: Let strcmp do the heavy lifting to decide order.
    - Why this order? Dereference first to get the data; compare next to decide—simple and direct.

  How It Works (For Novices):
    - Imagine you’re sorting a pile of notes (like "-I/project", "-I/usr") with qsort, and it needs
      help deciding which note goes before another.
    - compare_strings is like your sorting rule:
      - Step 1: qsort hands you two notes wrapped in boxes (a and b). You open the boxes (cast and
        dereference) to see the notes inside (the strings).
      - Step 2: Compare the notes with strcmp—like checking letter-by-letter: "-I/p" vs. "-I/u".
        If "-I/p" comes first (p < u), say “negative”; if same, say “zero”; if "-I/u" first, say “positive.”
    - It’s like telling qsort, “Put this one before that one” based on alphabetical order!

  Why It Works (For Novices):
    - Simplicity: Uses strcmp, a built-in tool, to compare letters, so you don’t have to write it yourself.
    - Accuracy: Follows alphabetical rules (e.g., "a" < "b"), making the sorted list neat.
    - Helpfulness: Gives qsort exactly what it needs (negative/zero/positive) to shuffle the notes.

  Why It’s Designed This Way (For Maintainers):
    - qsort Compatibility: Matches qsort’s required comparator signature (const void *, returns int),
      enabling sorting in store_lines for UNIX config flags (per _POSIX_C_SOURCE).
    - Efficiency: Relies on strcmp, an optimized standard library function, avoiding custom comparison
      logic—fast and reliable for small string arrays like include paths.
    - Type Safety: Uses const void * and proper casting to const char **, ensuring no modification of
      the strings and safe access, as qsort passes pointers-to-pointers (char ** elements).
    - Minimalism: Single-line implementation keeps it focused—compares strings, nothing else.
    - Standard Behavior: strcmp’s lexicographical order (ASCII-based) is predictable and matches
      typical sorting expectations for flags.

  Maintenance Notes:
    - Assumptions: Expects a and b to point to valid char **—NULL or invalid pointers crash strcmp.
      Ensure store_lines (caller) populates lines safely (e.g., via read_files).
    - Extensibility: To change sort order (e.g., reverse), swap a and b in strcmp or negate the result.
      For case-insensitive sort, use strcasecmp (with proper includes).
    - Edge Cases: Equal strings return 0, preserving their relative order (stable sort with qsort).
      Test with duplicates from remove_duplicates to confirm.
    - Debugging: If sorting fails (e.g., wrong order), log a and b values (via log_message) to trace
      what qsort sees—rare, since strcmp is robust.
    - Performance: strcmp is O(n) per comparison, fine for short flags; qsort’s O(n log n) dominates
      overall cost in store_lines.
*/

// Function to compare strings
int compare_strings(const void *a, const void *b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

int get_clang_target(char *output) {
  // Check cache first
  if(completion_cache.is_valid && completion_cache.cpu_arch[0] != '\0') {
    strncpy(output, completion_cache.cpu_arch, MAX_LINE_LENGTH - 1);
    output[MAX_LINE_LENGTH - 1] = '\0';
    return 0;
  }

  FILE *fp = _popen("clang --version", "r");

  if(!fp) {
    return 1;
  }

  // Allocate on heap instead of stack
  char *buffer = (char *)malloc(MAX_OUTPUT);

  if(!buffer) {
    _pclose(fp);
    return 1;
  }

  size_t bytes_read = fread(buffer, 1, MAX_OUTPUT - 1, fp);
  buffer[bytes_read] = '\0';
  _pclose(fp);
  char *target_line = strstr(buffer, "Target: ");

  if(!target_line) {
    free(buffer);
    return 1;
  }

  char *target_value = target_line + strlen("Target: ");
  int target_length = (int)strcspn(target_value, "\n");
  strncpy(output, target_value, target_length);
  output[target_length] = '\0';
  strncpy(completion_cache.cpu_arch, output, MAX_LINE_LENGTH - 1);
  free(buffer);  // Clean up
  return 0;
}

/*
  Function Description:
    Constructs a clang command string for code completion at a specific file position on Windows, using cached
    or freshly gathered include paths and CPU architecture. This function prepares arguments for clang to
    suggest completions (e.g., function names) at a given line and column in a source file, leveraging global
    buffers and a cache for efficiency.

  Parameters:
    - filename (const char *): Path to the source file (e.g., "C:\\project\\main.c"), not modified.
    - line (int): Line number in the file where completion is requested (1-based).
    - column (int): Column number in the file where completion is requested (1-based).

  Return Value:
    - char *: A dynamically allocated string with the clang command (e.g., "clang -target x86_64 ..."), or NULL
      on failure (e.g., file missing, memory issues). Caller must free this string.

  Detailed Steps:
    1. Allocate Temporary Buffers:
       - Calculates max_path_len from filename length + 256 (for paths and safety).
       - Allocates multiple buffers (found_at, target_output, lines, etc.) dynamically.
       - If any allocation fails, frees all and returns NULL.
    2. Validate File:
       - Checks if filename exists using _access; if not, logs and returns NULL.
       - Resolves absolute path with GetFullPathNameA into abs_filename; if fails, logs and returns NULL.
    3. Extract Directory:
       - Copies abs_filename to abs_copy, truncates at last '\\' to get dir_path.
    4. Initialize Cache:
       - If not initialized (static flag cache_initialized), calls init_cache and sets flag.
    5. Use Cached Data (If Valid):
       - Checks is_cache_valid with global_buffer_project_dir; if valid and buffers populated, builds command
         with global_buffer_cpu_arc and global_buffer_header_paths, frees temps, and returns.
    6. Find Config Files (If Cache Miss):
       - Calls findFiles on dir_path to locate .ccls and compile_flags.txt; if fails, logs and returns NULL.
       - Stores result in global_buffer_project_dir.
    7. Get CPU Target:
       - Calls get_clang_target into target_output; if fails, logs and returns NULL.
       - Copies to global_buffer_cpu_arc.
    8. Build Config Paths:
       - Constructs ccls_path and compile_flags_path from global_buffer_project_dir.
    9. Process Include Paths:
       - Initializes lines and sorted_lines, calls store_lines to read and sort paths, copies to
         global_buffer_header_paths, tracks count in num_lines.
    10. Update Cache:
        - Creates temp_paths from global_buffer_header_paths, calls update_cache with project_dir, paths,
          and cpu_arch.
    11. Build Command:
        - Allocates command string (size based on components), formats with clang options, target, include
          paths, and completion args.
        - Frees lines[i] and temporary buffers, returns command.

  Flow and Logic:
    - Steps 1-3: Setup and validate inputs, get directory.
    - Step 4-5: Try cache for speed; if good, build and return.
    - Steps 6-10: If cache misses, gather data (configs, target, paths), cache it.
    - Step 11: Construct and return the command.
    - Why this order? Validation first, cache check next, full process on miss, then assemble—optimizes for reuse.

  How It Works (For Novices):
    - Imagine you’re asking clang for coding tips at a spot in your file (filename:line:column), needing a
      big instruction (command) to get them. This function writes that instruction.
    - collect_code_completion_args is like this:
      - Step 1: Grab scratch paper (allocate buffers) for notes.
      - Step 2: Check your book (filename) exists (_access) and get its full address (GetFullPathNameA).
      - Step 3: Note the room it’s in (dir_path) by cutting off the book’s name.
      - Step 4: Check if your memo box (cache) is ready—if not, set it up (init_cache).
      - Step 5: If the memo has notes (cache valid), use them to write "clang -target x86_64 ..." fast.
      - Steps 6-7: If not, search for guidebooks (findFiles), ask clang its type (get_clang_target).
      - Steps 8-9: Write guidebook paths (ccls_path), copy their directions (store_lines) into your memo.
      - Step 10: Save everything in the memo box (update_cache) for next time.
      - Step 11: Write the full instruction (command) with all the details and hand it over.
    - It’s like gathering tools and notes to ask clang for help, reusing old notes when you can!

  Why It Works (For Novices):
    - Speed: Checks memos (cache) first to skip slow steps.
    - Safety: Makes sure the file and tools are ready before asking clang.
    - Smartness: Saves answers in memos (global buffers, cache) for next time.

  Why It’s Designed This Way (For Maintainers):
    - Windows Context: Uses GetFullPathNameA and '\\' (PATH_SEPARATOR), adapting UNIX logic for Windows in
      code_connector_shared_windows.c, integrating with clang for Vim.
    - Optimization: Cache (completion_cache) and global buffers (global_buffer_project_dir, etc.) speed up
      repeated calls—crucial on Windows where file ops (findFiles) and clang calls are slow.
    - Memory: Dynamic allocation avoids stack limits; frees all temps (except command) on all paths—caller
      owns result. Global buffers persist for reuse.
    - Robustness: Extensive error checks (_access, GetFullPathNameA, findFiles) with logs (log_message)
      ensure failures are caught—NULL return lets callers (e.g., execute_code_completion_command) handle.
    - Modularity: Relies on findFiles, get_clang_target, store_lines—reuses logic for a complex task.

  Maintenance Notes:
    - Buffer Sizes: max_path_len (+256) and command_length (512 + variables) are estimates—test with long
      filenames/paths to avoid truncation; consider PATH_MAX consistently.
    - Memory Leaks: Frees temps thoroughly—verify with tools like Dr. Memory on Windows for edge cases.
    - Cache Sync: Global buffers must match cache—test clear_cache/update_cache interactions to avoid stale data.
    - Error Logging: Logs failures but could detail why (e.g., GetLastError)—enhance for debugging.
    - Extensibility: Add more clang flags (e.g., -D) by expanding store_lines or command format if needed.
*/

// Windows-specific collect_code_completion_args
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
    free(found_at); free(target_output); free(lines); free(sorted_lines); free(abs_filename); free(ccls_path); free(compile_flags_path);
    return NULL;
  }

  int count = 0;

  // Check if the file exists
  if(_access(filename, 0) != 0) {
    perror("File does not exist");
    log_message("fn collect_code_completion_args: File does not exist\n");
    free(found_at); free(target_output); free(lines); free(sorted_lines); free(abs_filename); free(ccls_path); free(compile_flags_path);
    return NULL;
  }

  if(!GetFullPathNameA(filename, max_path_len, abs_filename, NULL)) {
    perror("GetFullPathName");
    free(found_at); free(target_output); free(lines); free(sorted_lines); free(abs_filename); free(ccls_path); free(compile_flags_path);
    return NULL;
  }

  // Get the absolute path of the filename and its directory
  // Replace dirname with Windows-compatible path extraction
  char abs_copy[MAX_PATH];
  strncpy(abs_copy, abs_filename, MAX_PATH);
  abs_copy[MAX_PATH - 1] = '\0'; // Ensure null-termination
  char *last_sep = strrchr(abs_copy, PATH_SEPARATOR);

  if(last_sep) {
    *last_sep = '\0'; // Truncate to get directory path
  }

  char *dir_path = abs_copy;
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
        free(found_at); free(target_output); free(lines); free(sorted_lines); free(abs_filename); free(ccls_path); free(compile_flags_path);
        return NULL;
      }

      int offset = snprintf(command, command_length, "clang -target %s -fsyntax-only -Xclang -code-completion-macros", global_buffer_cpu_arc);

      for(int i = 0; i < count; i++) {
        offset += snprintf(command + offset, command_length - offset, " %s", cached_paths[i]);
      }

      snprintf(command + offset, command_length - offset, " -Xclang -code-completion-at=%s:%d:%d %s", filename, line, column, filename);
      free(found_at); free(target_output); free(lines); free(sorted_lines); free(abs_filename); free(ccls_path); free(compile_flags_path);
      return command;
    }
  }

  // Cache miss - recalculate
  if(findFiles(dir_path, found_at) != 0) {
    printf("Error finding .ccls and compile_flags.txt\n");
    log_message("fn collect_code_completion_args: Error finding .ccls and compile_flags.txt\n");
    // Free all allocated memory
    free(found_at); free(target_output); free(lines); free(sorted_lines); free(abs_filename); free(ccls_path); free(compile_flags_path);
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
    free(found_at); free(target_output); free(lines); free(sorted_lines); free(abs_filename); free(ccls_path); free(compile_flags_path);
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
  snprintf(ccls_path, max_path_len, "%s\\.ccls", global_buffer_project_dir);
  snprintf(compile_flags_path, max_path_len, "%s\\compile_flags.txt", global_buffer_project_dir);

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
    free(found_at); free(target_output); free(lines); free(sorted_lines); free(abs_filename); free(ccls_path); free(compile_flags_path);
    return NULL;
  }

  // Construct the clang command
  int offset = snprintf(command, command_length, "clang -target %s -fsyntax-only -Xclang -code-completion-macros", global_buffer_cpu_arc);

  for(size_t i = 0; i < num_lines; i++) {
    offset += snprintf(command + offset, command_length - offset, " %s", global_buffer_header_paths[i]);
  }

  snprintf(command + offset, command_length - offset, " -Xclang -code-completion-at=%s:%d:%d %s", filename, line, column, filename);

  // Free allocated memory
  for(int i = 0; i < count; i++) {
    free(lines[i]);
  }

  // Free all temporary allocated memory (except command, which is returned)
  free(found_at); free(target_output); free(lines); free(sorted_lines); free(abs_filename); free(ccls_path); free(compile_flags_path);
  return command;
}

/*
  Function Description:
    Executes a clang code completion command for a given file position and captures the output as a string.
    This function builds the command using collect_code_completion_args, runs it via popen, and returns
    the completion suggestions (e.g., function names) for processing (e.g., by Vim).

  Parameters:
    - filename (const char *): Path to the source file (e.g., "/project/main.c"), not modified.
    - line (int): Line number in the file for completion (1-based).
    - column (int): Column number in the file for completion (1-based).

  Return Value:
    - char *: A dynamically allocated string containing clang’s completion output, or NULL on failure.
      Caller must free this string.

  Detailed Steps:
    1. Build Command:
       - Calls collect_code_completion_args with filename, line, and column to get the clang command.
       - If it returns NULL (e.g., file missing), logs and returns NULL.
    2. Open Pipe:
       - Uses popen to run the command in read mode ("r"), capturing stdout.
       - If popen fails (e.g., fork error), logs, frees command, and returns NULL.
    3. Read Output:
       - Allocates a buffer (output) of MAX_OUTPUT size.
       - Reads from pipe into buffer with fread (up to MAX_OUTPUT - 1 bytes).
       - Null-terminates the buffer.
    4. Clean Up:
       - Closes pipe with pclose; frees command string.
       - If fread read nothing (size == 0), frees output and returns NULL.
       - Otherwise, returns output.

  Flow and Logic:
    - Step 1: Get the clang command ready.
    - Step 2: Start clang and listen for its answer.
    - Step 3: Grab what clang says into a note.
    - Step 4: Finish up, check the note’s not empty, and hand it over.
    - Why this order? Build first, execute next, read then clean—standard pipe/command pattern.

  How It Works (For Novices):
    - Imagine you’re asking clang for a list of word suggestions (completions) for your code at a
      specific spot (filename:line:column), and you want that list as a note (the output string).
    - execute_code_completion_command is like this:
      - Step 1: Write a question for clang (e.g., "clang -code-completion-at=main.c:5:3") using
        collect_code_completion_args. If you can’t write it, give up.
      - Step 2: Shout the question through a tube (popen) so clang can answer back.
      - Step 3: Get a blank note (allocate output), listen through the tube (fread), and write down
        clang’s suggestions (e.g., "printf, scanf").
      - Step 4: Close the tube (pclose), toss the question paper (free command), and if the note’s
        empty, toss it too—otherwise, give it to you.
    - It’s like asking a smart friend for help and taking notes on what they say!

  Why It Works (For Novices):
    - Ease: Builds the question for you, so you don’t have to.
    - Safety: Checks every step (command, pipe, read) to avoid trouble.
    - Usefulness: Gives you clang’s ideas in a handy note you can read later.

  Why It’s Designed This Way (For Maintainers):
    - Integration: Ties into collect_code_completion_args for modularity, feeding clang’s completion
      output to Vim via processCompletionDataFromString—core to UNIX tooling (per _POSIX_C_SOURCE).
    - Pipe Usage: popen simplifies running clang and capturing stdout, standard for UNIX command
      execution, though it’s less flexible than fork/exec.
    - Memory: Allocates output dynamically (MAX_OUTPUT), avoiding stack issues; command is freed
      early, but output persists for caller—clean ownership.
    - Error Handling: NULL returns on failure (command build, popen, empty output) with logs
      (log_message) allow tracing—caller (e.g., Vim plugin) decides next steps.
    - Simplicity: Minimal parsing—raw output is returned, leaving interpretation to processCompletionDataFromString.

  Maintenance Notes:
    - Buffer Size: MAX_OUTPUT must fit clang’s completion output—test with large suggestion lists
      (e.g., big structs) to avoid truncation.
    - Error Detail: Logs "popen failed" but not why (e.g., errno)—add strerror for clarity if frequent.
    - Memory Leaks: Frees command and output on all paths—verify with valgrind, especially on failure.
    - Robustness: popen hangs if clang stalls—consider a timeout (not trivial with popen) or switch
      to fork/pipe for control.
    - Extensibility: To filter output here (e.g., strip errors), parse before returning—current raw
      approach is simpler but less refined.
*/

// Function to execute the code completion command: `clang -fsyntax-only -Xclang -code-completion-macros -Xclang -code-completion-at=file.c:line:column file.c`
char *execute_code_completion_command(const char *filename, int line, int column) {
  //printf("DEBUG: fn execute_code_completion_command: Starting code completion for file %s at line %d, column %d\n", filename, line, column);
  char *command = collect_code_completion_args(filename, line, column);

  if(!command) {
    /* printf("DEBUG: Failed to collect code completion arguments\n"); */
    return NULL;
  }

  //printf("DEBUG: Command to execute: %s\n", command);
  char *output = (char *)malloc(MAX_OUTPUT * sizeof(char));

  if(!output) {
    /* printf("DEBUG: Failed to allocate output buffer\n"); */
    free(command);
    return NULL;
  }

  output[0] = '\0';
  /* printf("DEBUG: Output buffer allocated, size: %d\n", MAX_OUTPUT); */
  FILE *fp = _popen(command, "r");

  if(!fp) {
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
      _pclose(fp);
      free(command);
      free(output);
      return NULL;
    }

    strcat(output, buffer);
    total_length += chunk_length;
    /* printf("DEBUG: Current total output length: %zu\n", total_length); */
  }

  int exit_status = _pclose(fp);

  if(exit_status != 0) {
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

  //printf("DEBUG: Final command output:\n%s\n", output);
  char *result = strdup(output);
  free(command);
  free(output);

  if(!result) {
    printf("DEBUG: Failed to allocate result string\n");
    return NULL;
  }

  //printf("DEBUG: Returning result:\n%s, length: %zu\n", result, strlen(result));
  return result;
}

/*
  Function Description:
    Filters clang’s code completion output on Windows using a regex pattern, extracting function names and
    parameters, then formats them with provided pre- and post-parenthesis text. This function refines raw
    clang output into a structured string (e.g., "printf(`int`, `<char*>`)" for Vim display.

  Parameters:
    - input (const char *): Raw clang completion output (e.g., "COMPLETION: printf : [#int#]printf(<#char*#>)"), not modified.
    - pre_paren_text (const char *): Text to prepend before the parameters (e.g., "printf"), not modified.
    - post_paren_text (const char *): Text to append after the parameters (e.g., ""), not modified.

  Return Value:
    - char *: A dynamically allocated string with filtered and formatted completions (e.g., "printf(`int`, `<char*>`)"),
      or NULL on failure (e.g., memory issues, regex error). Caller must free this string. Returns empty string ("")
      if no match is found.

  Detailed Steps:
    1. Allocate Output Buffer:
       - Allocates output buffer (MAX_OUTPUT size); if fails, returns NULL.
    2. Set Up Regex:
       - Allocates regmatch_t array (MAX_REGX_MATCHES); if fails, frees output and returns NULL.
       - Compiles regex pattern "^COMPLETION: ([^ ]+) : \\[#([^#]+)#\\]\\1\\(<#([^#]+)#>(, <#([^#]+)#>)*\\)"
         with regcomp; if fails, logs error and returns NULL.
    3. Process Input:
       - Initializes output as empty, tracks length and remaining space (MAX_OUTPUT - 1).
       - Runs regexec on input for first match; if no match, returns empty string ("").
    4. Build Formatted Output:
       - Appends pre_paren_text, "(", then processes matches:
         - First param (matches[3]): Adds "`", "<", param text, ">`, " (e.g., "`int`").
         - Optional second param (matches[5], if matches[4] exists): Adds ", `<", param text, ">`".
       - Closes with ")" and post_paren_text.
       - Checks buffer space at each step; if exceeded, frees and returns NULL.
    5. Clean Up and Return:
       - Frees regex with regfree, duplicates output to result, frees temps, returns result.

  Flow and Logic:
    - Steps 1-2: Prepare memory and regex for parsing.
    - Step 3: Test input against pattern; exit early if no match.
    - Step 4: Construct formatted string from match groups, respecting buffer limits.
    - Step 5: Finalize and return the result.
    - Why this order? Setup first, match next, build then clean—standard regex parsing flow.

  How It Works (For Novices):
    - Imagine clang hands you a messy note (input) like "COMPLETION: printf : [#int#]printf(<#char*#>)", and you
      want a neat version (e.g., "printf(`int`, `<char*>`)") using extra labels (pre_paren_text, post_paren_text).
    - filter_clang_output_mswin is like this cleanup:
      - Step 1: Grab a big blank card (output) to write on—if you can’t, give up.
      - Step 2: Set up a magic filter (regex) to spot the good parts; get helpers (matches) to mark them.
      - Step 3: Check the note with the filter—if it’s gibberish, hand back a blank card ("").
      - Step 4: Write the neat version: stick on the front label (pre_paren_text), add "(", then for each
        found bit (e.g., "int", "char*"), wrap it in "`" or "`<>`", add commas, finish with ")" and back label.
      - Step 5: Toss the helpers (free matches), copy your card (strdup), and give it away.
    - It’s like turning a scribble into a polished note with special markers!

  Why It Works (For Novices):
    - Clarity: Pulls out just the useful bits (function params) and dresses them up nicely.
    - Safety: Watches the card size (MAX_OUTPUT) so it doesn’t overflow.
    - Flexibility: Uses your labels (pre/post text) to customize the note.

  Why It’s Designed This Way (For Maintainers):
    - Windows Context: Tailored for Windows in code_connector_shared_windows.c, refining clang output for
      Vim via processCompletionDataFromString—uses regex.h (Windows-compatible) unlike UNIX’s simpler strstr.
    - Regex Power: Pattern captures function name and params precisely, handling clang’s structured output
      (e.g., "[#type#]"), more sophisticated than UNIX’s filter_clang_output.
    - Formatting: Adds "`" and "<>" markers around params, likely for Vim syntax highlighting—specific to
      this workflow.
    - Memory: Dynamic allocation (output, matches) with careful freeing avoids leaks; result persists for caller.
    - Error Handling: Returns NULL on allocation/regex failures, "" on no match—caller (e.g.,
      processCompletionDataFromString) decides next steps.

  Maintenance Notes:
    - Regex Fragility: Pattern assumes clang’s exact format—test with clang versions (e.g., 18) for changes;
      log mismatches (via log_message) to debug.
    - Buffer Limits: MAX_OUTPUT caps output—test with many params (e.g., complex structs) to avoid truncation.
    - Memory Safety: Frees all temps—verify with Dr. Memory on Windows; strdup failure isn’t checked but rare.
    - Extensibility: To handle more params (beyond two), adjust regex and loop over matches—current limit is
      one optional extra param.
    - Debugging: Add logs for match groups (e.g., matches[3], matches[5]) to trace parsing issues.
*/

char *filter_clang_output_mswin(const char *input, const char *pre_paren_text, const char *post_paren_text) {
  char *output = (char *)malloc(MAX_OUTPUT * sizeof(char));

  if(!output) {
    return NULL;
  }

  output[0] = '\0';
  regex_t regex;
  regmatch_t *matches = (regmatch_t *)malloc(MAX_REGX_MATCHES * sizeof(regmatch_t));

  if(!matches) {
    free(output);
    return NULL;
  }

  const char *pattern = "^COMPLETION: ([^ ]+) : \\[#([^#]+)#\\]\\1\\(<#([^#]+)#>(, <#([^#]+)#>)*\\)";
  int regex_result = regcomp(&regex, pattern, REG_EXTENDED | REG_NEWLINE);

  if(regex_result != 0) {
    char errbuf[256];
    regerror(regex_result, &regex, errbuf, sizeof(errbuf));
    free(output);
    free(matches);
    return NULL;
  }

  const char *cursor = input;
  size_t output_len = 0;
  size_t remaining_space = MAX_OUTPUT - 1;

  // Process only the first match
  if(regexec(&regex, cursor, MAX_REGX_MATCHES, matches, 0) == 0) {
    // Prepend the pre-parenthesis text
    size_t pre_paren_len = strlen(pre_paren_text);

    if(remaining_space < pre_paren_len) {
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, pre_paren_text, pre_paren_len);
    output_len += pre_paren_len;
    remaining_space -= pre_paren_len;

    // Opening parenthesis
    if(remaining_space < 1) {
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, "(", 2);
    output_len += 1;
    remaining_space -= 1;

    // First parameter (matches[3])
    if(remaining_space < 2) {
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, "`", 2);
    output_len += 1;
    remaining_space -= 2;
    size_t match_len = (size_t)(matches[3].rm_eo - matches[3].rm_so);

    if(match_len >= remaining_space) {
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, "<", 2);
    output_len += 1;
    remaining_space -= 2;
    strncat(output, cursor + matches[3].rm_so, match_len);
    output_len += match_len;
    remaining_space -= match_len;
    strncat(output, ">`", 3);
    output_len += 2;
    remaining_space -= 2;

    // Additional parameters (matches[4] and matches[5])
    if(matches[4].rm_so != -1) {
      if(remaining_space < 4) {
        regfree(&regex);
        free(output);
        free(matches);
        return NULL;
      }

      strncat(output, ", `<", 5);
      output_len += 4;
      remaining_space -= 4;
      match_len = (size_t)(matches[5].rm_eo - matches[5].rm_so);

      if(match_len >= remaining_space) {
        regfree(&regex);
        free(output);
        free(matches);
        return NULL;
      }

      strncat(output, cursor + matches[5].rm_so, match_len);
      output_len += match_len;
      remaining_space -= match_len;
      strncat(output, ">`", 3);
      output_len += 2;
      remaining_space -= 2;
    }

    // Closing parenthesis
    if(remaining_space < 1) {
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, ")", 2);
    output_len += 1;
    remaining_space -= 1;
    // Append post-parenthesis text
    size_t post_paren_len = strlen(post_paren_text);

    if(remaining_space < post_paren_len) {
      regfree(&regex);
      free(output);
      free(matches);
      return NULL;
    }

    strncat(output, post_paren_text, post_paren_len);
    output_len += post_paren_len;
    remaining_space -= post_paren_len;
  }

  else {
    // No match found
    regfree(&regex);
    free(output);
    free(matches);
    return strdup("");
  }

  regfree(&regex);
  char *result = strdup(output);
  free(output);
  free(matches);
  return result;
}

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
  return result == 0 ? 0 : -1;
}

void split_input_string(const char *input, char *file_path, int *line, int *column) {
  // Dynamically allocate memory for input_copy and log_file_path
  size_t input_len = strlen(input) + 1;  // +1 for null terminator
  char *input_copy = (char *)malloc(input_len * sizeof(char));

  // Check for allocation failures
  if(!input_copy) {
    fprintf(stderr, "Memory allocation failed\n");
    free(input_copy);
    return;
  }

  // Make a copy of the input string to use with strtok
  strcpy(input_copy, input);
  // Divide input string into three parts
  char *str = strtok(input_copy, " ");

  if(str == NULL) {
    fprintf(stderr, "Invalid input format\n");
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
    free(input_copy);
    return;
  }

  *line = atoi(line_str);
  *column = atoi(column_str);
  // Free allocated memory
  free(input_copy);
}

/*
  Function Description:
    Processes a Vim-style input string (e.g., "main.c 5 3") on Windows to generate and filter clang completion
    suggestions, returning them as a formatted string. Acts as the driver function for code completion,
    similar to the UNIX version, but adapted for Windows with file-based parsing and custom filtering.

  Parameters:
    - vimInputString (const char *): Input string from Vim (e.g., "file line col"), not modified.

  Return Value:
    - char *: Dynamically allocated string with filtered completions (e.g., "printf(`int`)"), or NULL on failure.
      Caller must free this string.

  Detailed Steps:
    1. Allocate Resources:
       - Allocates output_to_return (MAX_OUTPUT) and file_path (1024); if fails, logs and returns NULL.
    2. Parse Input:
       - Calls split_input_string to extract file_path, line, and column from vimInputString.
    3. Validate File and Line:
       - Opens file_path, reads to specified line; if fails or line not found, frees and returns NULL.
    4. Extract Context:
       - Finds nearest '(' before column, splits line into pre_paren_text and post_paren_text.
    5. Execute and Filter:
       - Calls execute_code_completion_command for raw output, then filter_clang_output_mswin with context.
       - Copies result to output_to_return, frees temps, and returns; NULL on any failure.

  Flow and Logic:
    - Setup, parse, validate, context, execute/filter—mirrors UNIX version’s pipeline but uses Windows paths
      and regex-based filtering.

  How It Works (For Novices):
    - Like a librarian taking your request (vimInputString), finding your book (file), and suggesting words
      at a spot (line:column), giving you a neat list (output).
    - Grabs paper (allocate), reads your note (split), checks the book, finds the page spot, asks clang,
      tidies the answer, and hands it over.

  Why It Works:
    - Similar to UNIX version: driver role, parses input, runs completion, filters output—adapted for Windows
      with file I/O and custom format.

  Why Designed This Way (For Maintainers):
    - Windows tweaks: Uses space-separated input (not ":") and filter_clang_output_mswin for regex parsing,
      differing from UNIX’s simpler strstr approach, but retains core flow.

  Maintenance Notes:
    - Test with long paths (1024 limit), add logs for failures, ensure filter matches clang output format.
*/

char *processCompletionDataFromString(const char *vimInputString) {
  if(!vimInputString) {
    printf("DEBUG: fn processCompletionDataFromString: vimInputString is NULL\n");
    return NULL;
  }

  char *output_to_return = (char *)malloc(MAX_OUTPUT * sizeof(char));

  if(!output_to_return) {
    printf("DEBUG: fn processCompletionDataFromString: Failed to allocate output_to_return\n");
    return NULL;
  }

  char *file_path = (char *)malloc(1024 * sizeof(char));

  if(!file_path) {
    printf("DEBUG: fn processCompletionDataFromString: Failed to allocate file_path\n");
    free(output_to_return);
    return NULL;
  }

  int extracted_line = 0;
  int extracted_column = 0;
  split_input_string(vimInputString, file_path, &extracted_line, &extracted_column);
  //printf("DEBUG: fn processCompletionDataFromString: file_path=%s, line=%d, col=%d\n", file_path, extracted_line, extracted_column);
  FILE *file = fopen(file_path, "r");

  if(!file) {
    //printf("DEBUG: fn processCompletionDataFromString: Failed to open file %s\n", file_path);
    free(output_to_return);
    free(file_path);
    return NULL;
  }

  char line_buffer[1024];
  int current_line = 1;

  while(fgets(line_buffer, sizeof(line_buffer), file) && current_line < extracted_line) {
    current_line++;
  }

  fclose(file);

  if(current_line != extracted_line) {
    //printf("DEBUG: fn processCompletionDataFromString: Line %d not found in file\n", extracted_line);
    free(output_to_return);
    free(file_path);
    return NULL;
  }

  size_t line_len = strlen(line_buffer);

  if(line_len > 0 && line_buffer[line_len - 1] == '\n') {
    line_buffer[line_len - 1] = '\0';
  }

  //printf("DEBUG: fn processCompletionDataFromString: line_buffer=%s\n", line_buffer);
  char pre_paren_text[256] = "";
  char post_paren_text[256] = "";
  int paren_pos = extracted_column - 1; // 1-based to 0-based

  // Search backward for the nearest opening parenthesis
  while(paren_pos >= 0 && line_buffer[paren_pos] != '(') {
    paren_pos--;
  }

  if(paren_pos < 0 || (size_t)paren_pos >= strlen(line_buffer)) {
    //printf("DEBUG: fn processCompletionDataFromString: No opening parenthesis found before column %d\n", extracted_column);
    free(output_to_return);
    free(file_path);
    return NULL;
  }

  // Found the parenthesis; split the line
  strncpy(pre_paren_text, line_buffer, paren_pos);
  pre_paren_text[paren_pos] = '\0';
  int close_paren_pos = -1;
  int i = paren_pos + 1;
  int paren_count = 1;

  while((size_t)i < strlen(line_buffer) && paren_count > 0) {
    if(line_buffer[i] == '(') {
      paren_count++;
    }

    else if(line_buffer[i] == ')') {
      paren_count--;
    }

    i++;
  }

  if(paren_count == 0) {
    close_paren_pos = i - 1;
    strcpy(post_paren_text, line_buffer + close_paren_pos + 1);
  }

  else {
    close_paren_pos = strlen(line_buffer) - 1;
    post_paren_text[0] = '\0'; // Empty post-parenthesis text for incomplete calls
  }

  //printf("DEBUG: fn processCompletionDataFromString: pre_paren_text=%s, post_paren_text=%s\n", pre_paren_text, post_paren_text);
  char *result = execute_code_completion_command(file_path, extracted_line, extracted_column);

  if(!result) {
    //printf("DEBUG: fn processCompletionDataFromString: execute_code_completion_command returned NULL\n");
    free(output_to_return);
    free(file_path);
    return NULL;
  }

  //printf("DEBUG: fn processCompletionDataFromString: Clang output=%s\n", result);
  char *filtered_result = filter_clang_output_mswin(result, pre_paren_text, post_paren_text);
  free(result);

  if(filtered_result) {
    //printf("DEBUG: fn processCompletionDataFromString: filtered_result=%s\n", filtered_result);
    size_t filtered_len = strlen(filtered_result);

    if(filtered_len < MAX_OUTPUT) {
      strncpy(output_to_return, filtered_result, filtered_len);
      output_to_return[filtered_len] = '\0';
    }

    else {
      printf("fn processCompletionDataFromString: Filtered result too large.\n");
      free(filtered_result);
      free(output_to_return);
      free(file_path);
      return NULL;
    }

    free(filtered_result);
    free(file_path);
    return output_to_return;
  }

  else {
    printf("DEBUG: fn processCompletionDataFromString: Failed to filter output\n");
    free(output_to_return);
    free(file_path);
    return NULL;
  }
}

char *writeResultToTempFile(const char *result) {
  size_t max_path_len = MAX_PATH_LENGTH;
  char *tempFilePath = (char *)malloc(max_path_len * sizeof(char));

  if(!tempFilePath) {
    return NULL;
  }

  char temp_dir[MAX_PATH];
  GetTempPathA(MAX_PATH, temp_dir);
  snprintf(tempFilePath, max_path_len, "%s\\code_connector_output_%lu.txt", temp_dir, GetCurrentProcessId());
  system("del /Q %TEMP%\\code_connector_output_*.*");
  FILE *tempFile = fopen(tempFilePath, "w");

  if(!tempFile) {
    free(tempFilePath);
    return NULL;
  }

  fprintf(tempFile, "%s", result);
  fclose(tempFile);
  char *result_path = strdup(tempFilePath);
  free(tempFilePath);
  return result_path;
}

void processCompletionDataForVim(const char *vimInputString) {
  char *result = processCompletionDataFromString(vimInputString);

  if(result) {
    strncpy(global_result_buffer, result, MAX_OUTPUT - 1);
    global_result_buffer[MAX_OUTPUT - 1] = '\0';
    char *tempFilePath = writeResultToTempFile(global_result_buffer);

    if(tempFilePath) {
      free(tempFilePath);
    }

    free(result);
  }

  else {
    global_result_buffer[0] = '\0';
  }
}

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

/*
  Function Description:
    Logs a message to a file ("C:\\Temp\\vim_parser_log.txt") on Windows by appending the message with a
    newline. This function provides a simple, file-based logging mechanism for debugging or error reporting,
    specific to the Windows environment.

  Parameters:
    - message (const char *): The message string to log (e.g., "fn collect_code_completion_args: File does not exist"),
      not modified.

  Return Value: None
    - Writes to the log file if possible; no return value or error indication.

  Detailed Steps:
    1. Open Log File:
       - Opens "C:\\Temp\\vim_parser_log.txt" in append mode ("a") using fopen.
       - If fopen fails (e.g., directory missing, no permissions), silently skips logging.
    2. Write Message:
       - If file opened successfully, writes message followed by "\n" using fprintf.
    3. Close File:
       - Closes the file with fclose to flush the write and free resources.

  Flow and Logic:
    - Step 1: Try to open the log file for adding text.
    - Step 2: If it worked, write the message with a line break.
    - Step 3: Close the file to finish up.
    - Why this order? Open first to access, write next to log, close to clean—basic file I/O sequence.

  How It Works (For Novices):
    - Imagine you’re keeping a diary (log file) on your computer to note what’s happening in your program,
      and you want to add a new line (message) each time something notable occurs.
    - log_message is like this:
      - Step 1: Open your diary ("C:\\Temp\\vim_parser_log.txt") to the last page (append mode)—if you can’t
        find it or open it, just shrug and move on.
      - Step 2: If it’s open, scribble your note (message) and hit enter ("\n") to start a new line.
      - Step 3: Close the diary (fclose) so it’s safe and ready for next time.
    - It’s like jotting down a quick memo in a fixed spot on your desk!

  Why It Works (For Novices):
    - Simplicity: Just adds your note to the end of the diary, no fuss.
    - Persistence: Keeps the notes in a file so you can check them later.
    - Quietness: If the diary’s missing, it doesn’t complain—it just skips it.

  Why It’s Designed This Way (For Maintainers):
    - Windows Context: Hardcodes "C:\\Temp\\..." path, specific to code_connector_shared_windows.c—replaces
      UNIX version’s stderr output for a file-based log, fitting Windows debugging workflows (e.g., Vim integration).
    - Append Mode: "a" ensures logs accumulate over time, unlike UNIX’s immediate stderr—useful for post-mortem
      analysis but grows unbounded.
    - Minimalism: No timestamp or error handling—focuses on basic logging, called by functions like
      collect_code_completion_args for tracing.
    - Silence on Failure: Ignores fopen errors, assuming logging is non-critical—differs from UNIX’s visible
      stderr feedback, prioritizing simplicity over robustness.
    - File I/O: Standard C file operations (fopen, fprintf, fclose) work across platforms, though path is Windows-specific.

  Maintenance Notes:
    - Path Hardcoding: "C:\\Temp" assumes directory exists and is writable—test on restricted systems (e.g.,
      no Temp dir, no permissions); consider GetTempPathA (used elsewhere) for flexibility.
    - Error Handling: No feedback on failure—add return value (e.g., int) or stderr fallback if logging’s
      critical; integrate with UNIX log_message for consistency.
    - File Growth: Append mode never truncates—add rotation (e.g., size check, rename old logs) if used heavily.
    - Extensibility: To add timestamps (like UNIX version), use time() and strftime before fprintf—current
      form lacks context.
    - Debugging: Test with missing C:\Temp or read-only file to ensure silent failure is acceptable; log fopen
      errors for tracing if needed.
*/

void log_message(const char *message) {
  FILE *log_file = fopen("C:\\Temp\\vim_parser_log.txt", "a");

  if(log_file) {
    fprintf(log_file, "%s\n", message);
    fclose(log_file);
  }
}

char *vim_parser(const char *combined_input) {
  processCompletionDataForVim(combined_input);
  size_t max_path_len = MAX_PATH_LENGTH;
  char *tempFilePath = (char *)malloc(max_path_len * sizeof(char));

  if(!tempFilePath) {
    return strdup("Error: Memory allocation failed");
  }

  char temp_dir[MAX_PATH];
  GetTempPathA(MAX_PATH, temp_dir);
  snprintf(tempFilePath, max_path_len, "%s\\code_connector_output_%lu.txt", temp_dir, GetCurrentProcessId());
  FILE *tempFile = fopen(tempFilePath, "r");

  if(!tempFile) {
    free(tempFilePath);
    return strdup("Error: Temporary file not found");
  }

  fseek(tempFile, 0, SEEK_END);
  long fileSize = ftell(tempFile);
  fseek(tempFile, 0, SEEK_SET);

  if(fileSize < 0) {
    fclose(tempFile);
    free(tempFilePath);
    return strdup("Error: Invalid file size");
  }

  char *fileContent = (char *)malloc((size_t)fileSize + 1);

  if(!fileContent) {
    fclose(tempFile);
    free(tempFilePath);
    return strdup("Error: Memory allocation failed");
  }

  fread(fileContent, (size_t)fileSize, 1, tempFile);
  fileContent[fileSize] = '\0';
  fclose(tempFile);
  free(tempFilePath);
  return fileContent;
}

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
    if(pos > str && !isspace(*(pos - 1)) && *(pos - 1) != '(' && *(pos - 1) != ',') {
      pos++;
      continue;
    }

    *call_start = pos;
    pos += func_name_len;

    while(pos < str_end && isspace(*pos)) {
      pos++;
    }

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
        *args_end = str_end;
        *call_end = str_end;
        return 1;
      }

      *args_end = pos - 1;
      *call_end = pos;
      return 1;
    }

    else {
      *args_start = pos;
      *args_end = pos;
      *call_end = str_end;
      return 1;
    }
  }

  fprintf(stderr, "Error: Function name '%s' not found in source\n", func_name);
  return 0;
}

static int match_function_signature(const char *source_args, const char *pattern_args) {
  // For pattern matching purposes, we'll accept any source args
  // since we're focusing on the pattern rather than strict signature matching
  return 1;
}

char *substitute_function_pattern(const char *source, const char *pattern) {
  if(!source || !pattern) {
    fprintf(stderr, "Error: Null source or pattern\n");
    return NULL;
  }

  const char *pattern_func_start;
  size_t pattern_func_name_len;

  if(!extract_function_name(pattern, &pattern_func_start, &pattern_func_name_len)) {
    return NULL;
  }

  char *pattern_func_name = (char *)malloc(pattern_func_name_len + 1);

  if(!pattern_func_name) {
    fprintf(stderr, "Error: Memory allocation failed for function name\n");
    return NULL;
  }

  strncpy(pattern_func_name, pattern_func_start, pattern_func_name_len);
  pattern_func_name[pattern_func_name_len] = '\0';
  const char *source_call_start, *source_call_end, *source_args_start, *source_args_end;

  if(!find_function_call(source, pattern_func_name, pattern_func_name_len,
                         &source_call_start, &source_call_end,
                         &source_args_start, &source_args_end)) {
    free(pattern_func_name);
    return NULL;
  }

  const char *pattern_call_start, *pattern_call_end, *pattern_args_start, *pattern_args_end;

  if(!find_function_call(pattern, pattern_func_name, pattern_func_name_len,
                         &pattern_call_start, &pattern_call_end,
                         &pattern_args_start, &pattern_args_end)) {
    free(pattern_func_name);
    return NULL;
  }

  size_t source_args_len = (source_args_end >= source_args_start) ?
                           (size_t)(source_args_end - source_args_start) : 0;
  size_t pattern_args_len = (pattern_args_end >= pattern_args_start) ?
                            (size_t)(pattern_args_end - pattern_args_start) : 0;
  char *source_args = (char *)malloc(source_args_len + 1);
  char *pattern_args = (char *)malloc(pattern_args_len + 1);

  if(!source_args || !pattern_args) {
    free(pattern_func_name);
    free(source_args);
    free(pattern_args);
    return NULL;
  }

  if(source_args_len > 0 && source_args_start < source + strlen(source)) {
    strncpy(source_args, source_args_start, source_args_len);
  }

  source_args[source_args_len] = '\0';

  if(pattern_args_len > 0 && pattern_args_start < pattern + strlen(pattern)) {
    strncpy(pattern_args, pattern_args_start, pattern_args_len);
  }

  pattern_args[pattern_args_len] = '\0';
  free(source_args);
  free(pattern_args);
  size_t prefix_len = (source_call_start >= source) ? (size_t)(source_call_start - source) : 0;
  size_t pattern_call_len = (pattern_call_end >= pattern_call_start) ? (size_t)(pattern_call_end - pattern_call_start) : 0;
  size_t suffix_len = (source_call_end <= source + strlen(source)) ? strlen(source_call_end) : 0;
  char *result = (char *)malloc(prefix_len + pattern_call_len + suffix_len + 1);

  if(!result) {
    free(pattern_func_name);
    return NULL;
  }

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
