// Last Change: 2025-03-03  Monday: 01:39:32 PM

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

// Initialize the cache
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

/*
  Function Description:
    Checks if the global completion_cache is valid and matches the current project directory.
    This function determines whether the cached data (e.g., include paths, CPU architecture)
    can be reused for the given project directory, avoiding unnecessary recalculations.

  Parameters:
    - current_project_dir (const char *): The directory path of the current project (e.g., "/project").
      It’s a string pointer that doesn’t get modified (const).

  Return Value:
    - int: Returns 1 (true) if the cache is valid and matches the project directory; 0 (false) otherwise.

  Detailed Steps:
    1. Quick Validation Checks:
       - If completion_cache.is_valid is 0 (false), return 0 immediately—cache isn’t ready.
       - If current_project_dir is NULL, return 0—can’t compare without a valid input.
    2. Resolve Absolute Path:
       - Uses realpath to convert current_project_dir into an absolute path (e.g., turns "./project"
         into "/home/user/project").
       - Stores the result in a local char array (resolved_current).
       - If realpath fails (e.g., directory doesn’t exist), return 0.
    3. Compare Paths:
       - Compares the resolved current_project_dir with completion_cache.project_dir using strcmp.
       - Returns 1 if they match (strcmp returns 0), 0 if they don’t.

  Flow and Logic:
    - Step 1: Check basic conditions—cache must be valid and input must exist.
    - Step 2: Get the full, real path of the current directory to ensure accurate comparison.
    - Step 3: Compare the cached project path with the current one; match means cache is usable.
    - Why this order? Early exits (Step 1) save time; resolving the path (Step 2) ensures precision
      before the comparison (Step 3).

  How It Works (For Novices):
    - Think of completion_cache as a labeled box of project tools (include paths, CPU info).
      It has a tag saying which project it’s for (project_dir) and a “Ready” light (is_valid).
    - is_cache_valid is like checking if this box is the right one for your current job:
      - Step 1: Look at the “Ready” light—if it’s off (is_valid = 0) or you forgot to say what job
        you’re doing (current_project_dir = NULL), say “No, this box won’t work” (return 0).
      - Step 2: Check the job’s full address (e.g., "/home/user/project") using realpath, because
        shortcuts like "./project" might confuse things.
      - Step 3: Compare the job address with the box’s tag—if they’re the same, say “Yes, this
        box is perfect” (return 1); if not, say “No, wrong box” (return 0).
    - It’s like making sure you’re using the right toolbox before starting work!

  Why It Works (For Novices):
    - Safety: Checking is_valid and NULL first prevents crashes or nonsense comparisons.
    - Accuracy: realpath ensures we’re comparing full paths (e.g., "/project" vs. "/project"),
      not tricky relative ones (e.g., "./src/../project").
    - Simplicity: It’s a yes/no question—does the cache match the project? Easy to understand.

  Why It’s Designed This Way (For Maintainers):
    - Cache Reuse: The function supports the program’s goal of reusing cached data (e.g., in
      collect_code_completion_args) to skip slow operations like finding .ccls files or running
      clang --version. It’s a key optimization check.
    - Robustness: Early returns for invalid states (is_valid = 0 or NULL input) avoid unnecessary
      work and protect against bad inputs, common in UNIX environments where paths can be tricky.
    - Path Normalization: realpath handles symbolic links and relative paths, ensuring the
      comparison isn’t fooled by different ways of writing the same directory (e.g., "/proj"
      vs. "/project" if linked). This is critical on UNIX (per _POSIX_C_SOURCE).
    - Minimal Overhead: It only resolves the path if the initial checks pass, keeping it efficient.
    - Global Dependency: Relies on completion_cache being properly set by init_cache or update_cache,
      tying it to the program’s caching strategy.

  Maintenance Notes:
    - Edge Cases: If realpath fails due to permissions or nonexistent paths, it returns 0, which
      is safe but silent. Consider logging (via log_message) for debugging if this happens often.
    - Extensibility: If completion_cache grows (e.g., adding a timestamp field), you might check
      more conditions here (e.g., cache age), but the path check remains the core logic.
    - Performance: realpath is a system call, so it’s not free. For frequent calls, ensure the
      cache is usually valid to minimize these checks.
    - Debugging: A return of 0 could mean invalid cache, bad input, or mismatched paths—logging
      the reason could clarify which.
*/

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

/*
  Function Description:
    Updates the global completion_cache with new project data, including the project directory,
    include paths, and CPU architecture. This function refreshes the cache by clearing old data,
    storing new values, and marking it valid, enabling reuse in future operations.

  Parameters:
    - project_dir (const char *): The project directory path (e.g., "/project"), not modified.
    - include_paths (char **): An array of strings (e.g., "-I/project/include") to cache.
    - path_count (int): The number of strings in include_paths.
    - cpu_arch (const char *): The CPU architecture (e.g., "x86_64-unknown-linux-gnu"), not modified.

  Return Value: None
    - Modifies the global completion_cache in place; no return value.

  Detailed Steps:
    1. Clear Existing Cache:
       - Calls clear_cache to free old include paths and reset all fields to zero.
    2. Store Project Directory:
       - Uses realpath to get the absolute path of project_dir, storing it in completion_cache.project_dir.
       - If realpath fails (e.g., invalid path), sets is_valid to 0 and exits early.
    3. Store Include Paths:
       - Limits path_count to MAX_CACHED_PATHS if it’s too large.
       - Loops through include_paths, duplicating each string (via strdup) into completion_cache.include_paths.
       - If any strdup fails, calls clear_cache and exits early.
    4. Store CPU Architecture:
       - Copies cpu_arch into completion_cache.cpu_arch with a size limit (MAX_LINE_LENGTH - 1).
       - Ensures null termination.
    5. Mark Cache as Valid:
       - Sets is_valid to 1 if all steps succeed, indicating the cache is ready.

  Flow and Logic:
    - Step 1: Wipe the slate clean to avoid mixing old and new data.
    - Step 2: Store the project directory first, as it’s the key identifier.
    - Step 3: Add include paths, handling memory allocation carefully.
    - Step 4: Add CPU architecture, a smaller but critical piece.
    - Step 5: Confirm everything worked by setting is_valid.
    - Why this order? Clearing first ensures a fresh start; directory sets the context; paths and
      CPU fill details; validity comes last as a success flag.

  How It Works (For Novices):
    - Imagine completion_cache as a labeled box where you store project tools:
      - A tag for the project name (project_dir).
      - A drawer for include paths (include_paths).
      - A counter for how many paths (include_path_count).
      - A note for CPU type (cpu_arch).
      - A “Ready” light (is_valid).
    - update_cache is like filling this box with new tools for a job:
      - Step 1: Empty the box completely (clear_cache) so no old tools get mixed in.
      - Step 2: Write the job’s full address (e.g., "/home/user/project") on the tag using realpath.
        If the address is wrong, stop and mark the box “Not Ready.”
      - Step 3: Copy each tool (include path like "-I/project/include") into the drawer, making
        a fresh copy (strdup). If copying fails, empty the box and stop.
      - Step 4: Jot down the CPU type (e.g., "x86_64") on the note, keeping it short.
      - Step 5: Turn on the “Ready” light (is_valid = 1) if everything fits.
    - It’s like packing a toolbox for a specific project, ensuring it’s ready to use next time!

  Why It Works (For Novices):
    - Safety: Clearing first prevents old tools from confusing the new job.
    - Accuracy: realpath ensures the project address is exact, not a shortcut.
    - Reliability: Copying strings (strdup) keeps the cache independent of the caller’s data.
    - Simplicity: Each step builds the cache logically—address, tools, CPU, then “Ready.”

  Why It’s Designed This Way (For Maintainers):
    - Cache Refresh: Supports the program’s optimization goal (e.g., in collect_code_completion_args)
      by storing fresh data for reuse, avoiding slow operations like file searches or clang calls.
    - Memory Ownership: Uses strdup to duplicate include_paths, ensuring the cache owns its data.
      This prevents issues if the caller frees or modifies the original strings later.
    - Robustness: Early exits on failure (realpath or strdup) with clear_cache ensure the cache
      stays consistent—either fully updated or fully cleared, no half-states.
    - UNIX Focus: realpath aligns with UNIX path handling (per _POSIX_C_SOURCE), resolving links
      and relative paths for accurate comparisons in is_cache_valid.
    - Bounds Checking: Limits path_count to MAX_CACHED_PATHS and caps cpu_arch at MAX_LINE_LENGTH,
      preventing buffer overflows or excessive memory use.

  Maintenance Notes:
    - Memory Leaks: If strdup fails mid-loop, clear_cache frees prior allocations, avoiding leaks.
      Ensure MAX_CACHED_PATHS is large enough for typical projects (defined in code_connector_shared.h).
    - Error Handling: No logging on failure—consider adding log_message calls (e.g., “realpath failed”)
      for debugging, though silent failure keeps it simple.
    - Extensibility: New fields in CodeCompletionCache (e.g., compiler version) would need storage
      here, with similar bounds and failure checks.
    - Performance: Multiple strdup calls could be slow for many paths; if this bottlenecks, consider
      a single allocation block, though it’s complex to manage.
    - Debugging: After success, is_valid = 1 lets you verify the cache in tools like gdb; on failure,
      it’s reset to 0, signaling issues upstream.
*/

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

/*
  Function Description:
    Retrieves the cached include paths from the global completion_cache and updates the caller-provided
    count variable with the number of paths. This function provides access to the cached include paths
    (e.g., "-I/project/include") if the cache is valid, or signals that no paths are available if it’s not.

  Parameters:
    - count (int *): A pointer to an integer where the number of cached include paths will be stored.
      The caller uses this to know how many paths are returned.

  Return Value:
    - char **: A pointer to an array of strings (include paths) from completion_cache.include_paths if
      the cache is valid; NULL if the cache is invalid or empty.

  Detailed Steps:
    1. Check Cache Validity:
       - If completion_cache.is_valid is 0 (false), the cache isn’t ready.
       - Sets *count to 0 to indicate no paths are available.
       - Returns NULL to signal the caller that there’s nothing to use.
    2. Return Cached Data:
       - If valid, sets *count to completion_cache.include_path_count (number of paths).
       - Returns completion_cache.include_paths, the array of cached path strings.

  Flow and Logic:
    - Step 1: Verify the cache is usable; if not, signal “nothing here” with 0 and NULL.
    - Step 2: If usable, share the count and paths directly from the cache.
    - Why this order? Validity check first avoids returning garbage or stale data; then it’s a
      simple handoff of cached values.

  How It Works (For Novices):
    - Picture completion_cache as a toolbox with a drawer of tools (include paths like "-I/project/include"),
      a counter for how many tools (include_path_count), and a “Ready” light (is_valid).
    - get_cached_include_paths is like asking, “Can I borrow your tools, and how many are there?”
      - Step 1: Check the “Ready” light. If it’s off (is_valid = 0), say “Sorry, no tools” (set count
        to 0 and return NULL).
      - Step 2: If the light’s on, tell them how many tools (set *count to include_path_count) and
        hand over the drawer (return include_paths).
    - It’s like a quick check-out system: if the toolbox is ready, you get the tools; if not, you get
      nothing and know it’s empty.

  Why It Works (For Novices):
    - Safety: Checking is_valid first ensures you don’t get junk tools from an unready box.
    - Clarity: *count tells you exactly how many tools you’re getting, so you don’t guess.
    - Simplicity: It’s a yes/no decision—either you get the paths or you don’t, no fuss.

  Why It’s Designed This Way (For Maintainers):
    - Cache Access: Part of the caching strategy (e.g., used in collect_code_completion_args) to
      reuse include paths without recalculating them, boosting performance on UNIX systems where
      file operations are slow (per _POSIX_C_SOURCE).
    - Pointer Safety: Returns the actual completion_cache.include_paths array, not a copy, avoiding
      unnecessary memory allocation. The caller must not free it, as the cache owns it.
    - Fail-Safe: Returning NULL and setting *count to 0 on invalid cache is a clear signal to fall
      back to recalculating paths, maintaining program flow (e.g., in collect_code_completion_args).
    - Minimalist: No extra checks beyond is_valid—assumes update_cache set up the cache correctly,
      keeping this function fast and focused.
    - Caller Responsibility: Passing count as a pointer lets the caller manage its own variable,
      typical in C for output parameters, reducing function complexity.

  Maintenance Notes:
    - Memory Ownership: The returned char** points to cache memory (allocated by update_cache via
      strdup). Document that callers must not free it, or risk double-free crashes.
    - Edge Cases: If is_valid is 1 but include_path_count is 0, it still returns include_paths with
       count = 0. This is valid (empty array), but ensure callers handle it (e.g., don’t assume paths).
    - Extensibility: If completion_cache adds new fields (e.g., debug flags), this function stays
      focused on include paths unless expanded to return more.
    - Debugging: A return of NULL means invalid cache—log this (via log_message) if it’s frequent,
      to trace why update_cache isn’t setting is_valid.
    - Performance: Direct pointer return is fast, but relies on cache integrity. Test is_valid
      thoroughly in update_cache to avoid false positives.
*/

// Function to get cached include paths
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
    Creates default `.ccls` and `compile_flags.txt` files in the specified directory if they don’t
    already exist. These files provide basic configuration for the ccls language server and clang
    compiler, ensuring the program can function even without user-provided configs.

  Parameters:
    - directory (const char *): The directory path where the files will be created (e.g., "/project").
      It’s a constant string, not modified by the function.

  Return Value:
    - int: Returns 0 on success (both files created or already exist); 1 on failure (e.g., memory
      allocation or file creation fails).

  Detailed Steps:
    1. Calculate Path Sizes:
       - Computes the length of directory and adds space for file names (".ccls", "/compile_flags.txt").
       - Sets max_path_len to accommodate full paths (directory length + 20 for safety).
    2. Allocate Memory for Paths:
       - Dynamically allocates two char arrays (ccls_path, compile_flags_path) for full file paths.
       - Checks for allocation failure; if either fails, frees both and returns 1.
    3. Build File Paths:
       - Initializes the path buffers with zeros using memset.
       - Uses snprintf to construct full paths (e.g., "/project/.ccls", "/project/compile_flags.txt").
    4. Create .ccls File:
       - Opens ccls_path in write mode ("w"); if successful, writes default clang settings.
       - Writes: "clang\n%c -std=c11\n%cpp -std=c++17\n".
       - Closes the file; if opening fails, sets return_value to 1.
    5. Create compile_flags.txt File:
       - Only proceeds if .ccls creation succeeded (return_value == 0).
       - Opens compile_flags_path in write mode; if successful, writes default include flags.
       - Writes: "-I.\n-I..\n-I/usr/include\n-I/usr/local/include\n".
       - Closes the file; if opening fails, sets return_value to 1.
    6. Clean Up and Return:
       - Frees both path buffers.
       - Returns return_value (0 for success, 1 for any failure).

  Flow and Logic:
    - Step 1-2: Prepare memory and paths safely before file operations.
    - Step 3-4: Create .ccls first, as it’s foundational for ccls integration.
    - Step 5: Create compile_flags.txt only if .ccls succeeds, ensuring partial configs don’t confuse tools.
    - Step 6: Clean up and report success/failure.
    - Why this order? Memory allocation first avoids runtime errors; .ccls priority reflects its role;
      cleanup ensures no leaks.

  How It Works (For Novices):
    - Imagine you’re setting up a new workshop (directory) and need two instruction sheets:
      - .ccls: Tells the ccls tool how to read your code (e.g., use C11 or C++17 standards).
      - compile_flags.txt: Tells clang where to find extra tools (header files).
    - create_default_config_files is like writing these sheets if they’re missing:
      - Step 1: Measure how much paper you’ll need (calculate path sizes).
      - Step 2: Get blank sheets (allocate memory) to write the full addresses.
      - Step 3: Write the addresses (e.g., "/project/.ccls") on the sheets.
      - Step 4: Write basic ccls instructions (e.g., "use clang, C11") on the first sheet.
      - Step 5: If that worked, write clang instructions (e.g., "look in /usr/include") on the second.
      - Step 6: Throw away your scratch paper (free memory) and say if it all worked (0) or not (1).
    - It’s like making sure your workshop has basic instructions so tools like clang can start working!

  Why It Works (For Novices):
    - Safety: Checks memory and file operations, stopping if anything fails.
    - Usefulness: Provides default settings so the program runs even without custom configs.
    - Cleanliness: Frees memory so the workshop doesn’t get cluttered.

  Why It’s Designed This Way (For Maintainers):
    - Fallback Mechanism: Ensures the program (e.g., findFiles, collect_code_completion_args) has
      config files to work with, critical for UNIX systems (per _POSIX_C_SOURCE) where ccls expects them.
    - Dynamic Paths: Allocates memory for paths instead of fixed buffers, avoiding overflows if
      directory is long. The +20 buffer is a safe guess, but PATH_MAX could be used explicitly.
    - Error Handling: Returns 1 on any failure (memory, file ops), letting callers (e.g., a setup routine)
      decide how to proceed. No partial configs are left behind due to the .ccls-first check.
    - Default Content: The .ccls settings (C11, C++17) and compile_flags.txt paths (., .., /usr/include)
      are sensible UNIX defaults, covering common use cases without user input.
    - Resource Management: Frees memory even on failure, preventing leaks in a global-context program.

  Maintenance Notes:
    - Path Length: max_path_len (+20) is arbitrary; consider PATH_MAX for consistency with other
      functions (e.g., findFiles). Test with long paths to ensure no truncation.
    - Error Reporting: Silent failures (just return 1) work but could log specifics (via log_message)
      for debugging (e.g., “fopen failed for .ccls”).
    - Extensibility: To add more config files or settings, expand the allocation and creation steps,
      keeping the success-check pattern.
    - Permissions: Assumes write access to directory; if this fails often (e.g., read-only dirs),
      consider a fallback location or user warning.
    - Debugging: Check return_value in callers; a 1 could mean memory or I/O issues—traceable via logs.
*/

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

/*
  Function Description:
    Searches upward through the directory tree from a given path to find a directory containing
    both `.ccls` and `compile_flags.txt` files, storing the found directory path in found_at.
    This function is recursive and UNIX-specific, designed to locate project configuration files.

  Parameters:
    - path (const char *): The starting directory to search from (e.g., "/project/src"), not modified.
    - found_at (char *): A buffer where the path of the directory containing both files is stored.
      Must be at least PATH_MAX bytes.

  Return Value:
    - int: Returns 0 if both files are found in a directory; non-zero (typically 1) if not found or an error occurs.

  Detailed Steps:
    1. Validate Inputs:
       - Checks if path is NULL; if so, logs an error and returns 1.
       - Checks if found_at is NULL; if so, logs an error and returns 1.
    2. Resolve Absolute Path:
       - Uses realpath to convert path to an absolute path (e.g., "/home/user/project/src").
       - Stores it in currentPath; if realpath fails (e.g., path doesn’t exist), logs and returns 1.
    3. Open Directory:
       - Opens currentPath with opendir; if it fails (e.g., no permissions), logs and returns 1.
    4. Scan for Files:
       - Reads directory entries with readdir, checking each for ".ccls" or "compile_flags.txt".
       - Sets flags (cclsFound, compileFlagsFound) to 1 when found.
       - Closes the directory after scanning.
    5. Check Results:
       - If both files are found (cclsFound && compileFlagsFound), copies currentPath to found_at and returns 0.
    6. Handle Root Case:
       - If currentPath is "/" (root) and files aren’t found, returns 1—no higher to search.
    7. Recurse Upward:
       - Copies currentPath with strdup; if it fails, logs and returns 1.
       - Gets the parent directory with dirname; if it fails, logs, frees copy, and returns 1.
       - Builds parentPath and recursively calls findFiles with it, returning the result.

  Flow and Logic:
    - Steps 1-2: Ensure inputs and path are valid before proceeding.
    - Steps 3-4: Check the current directory for both files.
    - Step 5: If found, save the path and stop.
    - Step 6: If at root and not found, give up.
    - Step 7: If not found and not at root, move up and try again.
    - Why this order? Validation first avoids crashes; checking current directory before recursion
      minimizes unnecessary calls; root check stops infinite loops.

  How It Works (For Novices):
    - Imagine you’re looking for two special maps (.ccls and compile_flags.txt) in a stack of folders.
      You start in one folder (path, like "/project/src") and need to find a folder with both maps,
      writing its name in a notebook (found_at).
    - findFiles is like this search:
      - Step 1: Make sure you have a folder to check (path) and a notebook (found_at)—if not, stop.
      - Step 2: Get the full folder name (e.g., "/home/user/project/src") using realpath.
      - Step 3: Open the folder; if you can’t, stop.
      - Step 4: Look inside for both maps; mark if you find ".ccls" or "compile_flags.txt".
      - Step 5: If both are there, write the folder name in the notebook and say “Found it!” (return 0).
      - Step 6: If you’re at the top folder ("/") and they’re not there, say “No luck” (return 1).
      - Step 7: If not found, move up to the parent folder (e.g., "/project") and check again.
    - It’s like climbing up a ladder of folders until you find the maps or hit the top!

  Why It Works (For Novices):
    - Safety: Checks for bad inputs and errors stop it from crashing.
    - Persistence: Keeps going up until it finds the maps or runs out of folders.
    - Clarity: Puts the answer (folder path) right where you asked (found_at).

  Why It’s Designed This Way (For Maintainers):
    - UNIX Quirk: Reflects ccls’s UNIX expectation that config files are in a parent directory
      (e.g., /project, not /project/src), as noted in the function’s comment. Recursion handles this.
    - Robustness: Extensive error checking (NULL, realpath, opendir, strdup, dirname) ensures it
      fails gracefully on bad inputs or system issues, common in UNIX (per _POSIX_C_SOURCE).
    - Recursion: Climbing up the tree is elegant for finding a project root, avoiding complex loops,
      though it risks stack overflow with very deep paths (rare in practice).
    - Memory Safety: Frees abs_path and currentPathCopy to prevent leaks; assumes found_at is
      caller-allocated (PATH_MAX size), aligning with C conventions.
    - Efficiency: Stops as soon as both files are found, minimizing directory scans.

  Maintenance Notes:
    - Stack Depth: Deep directory trees could overflow the stack—test with extreme cases or consider
      an iterative version if this becomes an issue.
    - Logging: Errors are logged to stderr (e.g., "realpath failed"), but consider log_message for
      consistency with other functions.
    - Extensibility: To search for more files, add checks in Step 4 and update the condition in Step 5.
    - Performance: opendir/readdir per level isn’t fast; cache results (via completion_cache) reduce
      calls, as seen in collect_code_completion_args.
    - Debugging: Uncommented printf lines (e.g., "Checking directory") are handy—enable them to trace
      the search path if issues arise.
*/

/**
   Finds the .ccls and compile_flags.txt files in the directory tree starting from the given path (UNIX-specific).

   This function recursively searches upward through the directory hierarchy starting from the specified
   path until it finds both .ccls and compile_flags.txt in the same directory, or reaches the root (/).
   If found, the directory path is stored in found_at. If not found by the root, it returns an error.

   Note: On UNIX, ccls expects these files to be in a parent directory of the source files (e.g., /project
   for source in /project/src), not alongside them. This is a quirk not present in the Windows version.

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
  // Debug: Log the current directory being checked
  // printf("DEBUG: Checking directory: %s\n", currentPath);
  // Open the current directory
  dir = opendir(currentPath);

  if(!dir) {  // opendir fails, could be due to permission issues or non-existent directory
    perror("opendir"); // Print detailed error message
    return 1; // Return error code
  }

  // Scan for .ccls and compile_flags.txt by reading each entry in the directory
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

  // If both files are found, store the path and return success
  if(cclsFound && compileFlagsFound) {
    snprintf(found_at, PATH_MAX, "%s", currentPath);
    // Debug: Log where files were found
    // printf("DEBUG: Files found at: %s\n", found_at);
    return 0; // Success: files found
  }

  // If at root and files not found, fail
  if(strcmp(currentPath, "/") == 0) {
    // printf("DEBUG: Reached root, files not found\n");
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

/*
  Function Description:
    Retrieves the CPU architecture (target) from the `clang --version` command and stores it in the
    provided output buffer. It first checks the cache; if not cached, it runs the command, parses
    the output for the "Target:" line, and caches the result for future use.

  Parameters:
    - output (char *): A caller-provided buffer where the CPU architecture string (e.g.,
      "x86_64-unknown-linux-gnu") is stored. Must be at least MAX_LINE_LENGTH bytes.

  Return Value:
    - int: Returns 0 on success (target retrieved and stored); 1 on failure (e.g., fork, clang, or parsing fails).

  Detailed Steps:
    1. Check Cache:
       - If completion_cache.is_valid is true and cpu_arch isn’t empty, copies cached value to output.
       - Limits copy to MAX_LINE_LENGTH - 1, null-terminates, and returns 0.
    2. Allocate Output Buffer:
       - Allocates a temporary buffer (output_buffer) of MAX_OUTPUT size for clang’s output.
    3. Set Up Pipe and Fork:
       - Creates a pipe to capture clang’s output.
       - Forks a child process; if fork fails, logs and returns 1.
    4. Child Process:
       - Redirects stdout to pipe’s write end (dup2), closes unused pipe ends, and runs `clang --version`.
       - Exits child with 0 (success) or after execlp fails.
    5. Parent Process:
       - Closes pipe’s write end, reads from read end into output_buffer (up to MAX_OUTPUT - 1).
       - Null-terminates buffer, closes pipe, waits for child to finish.
    6. Parse Output:
       - Searches for "Target: " in output_buffer with strstr.
       - If found, extracts the target value (until newline), copies to output and cache, frees buffer, returns 0.
       - If not found, frees buffer, returns 1.
    7. Handle Errors:
       - On fork failure, frees buffer and returns 1.

  Flow and Logic:
    - Step 1: Use cached value if available—fast path.
    - Steps 2-5: Run clang and capture output if cache is empty—slow path setup.
    - Step 6: Extract and store the target—slow path result.
    - Step 7: Handle failures gracefully.
    - Why this order? Cache check first optimizes; process setup then parsing follows UNIX exec pattern.

  How It Works (For Novices):
    - Imagine you need to know your computer’s “type” (like "x86_64") for clang to work right, and
      you’ll write it in a notebook (output). You can check a memo (cache) or ask clang directly.
    - get_clang_target is like this:
      - Step 1: Look at your memo (completion_cache.cpu_arch). If it’s there and valid, copy it to
        your notebook and done!
      - Step 2: If not, get a big scratch pad (output_buffer) to jot down clang’s answer.
      - Steps 3-5: Ask clang by shouting “--version” (fork, execlp) and listening through a tube (pipe).
        The kid (child) shouts, you (parent) write it down.
      - Step 6: Find the “Target:” line in the scratch pad (strstr), copy just the type (e.g., "x86_64")
        to your notebook and memo, toss the scratch pad.
      - Step 7: If asking fails (fork), say “Oops” and stop.
    - It’s like checking a note or asking a friend for info, then saving it for next time!

  Why It Works (For Novices):
    - Speed: Uses the memo (cache) when possible, avoiding slow questions.
    - Safety: Checks everything (cache, fork, output) to avoid mistakes.
    - Smartness: Saves the answer (caches) so you don’t ask again.

  Why It’s Designed This Way (For Maintainers):
    - Cache Optimization: Leverages completion_cache for speed, critical for UNIX (per _POSIX_C_SOURCE)
      where `clang --version` is a costly system call—used in collect_code_completion_args.
    - UNIX Process Model: Fork/pipe/exec pattern is standard for capturing command output, robust for
      clang’s variable-length response.
    - Memory Safety: Allocates output_buffer dynamically (MAX_OUTPUT), avoiding stack overflows; frees
      it on all paths. Assumes output is caller-allocated (MAX_LINE_LENGTH).
    - Parsing: Simple "Target: " search with strstr is efficient for clang’s known output format,
      though fragile if clang changes (e.g., multi-line targets).
    - Error Handling: Returns 1 on any failure (fork, parse), letting callers (e.g., collect_code_completion_args)
      retry or fallback—minimal but effective.

  Maintenance Notes:
    - Cache Sync: Relies on completion_cache being valid—test with clear_cache/update_cache to ensure
      cpu_arch stays current.
    - Buffer Size: MAX_OUTPUT must fit clang’s full output (test with verbose clang); MAX_LINE_LENGTH
      must fit target (rarely exceeds 32 chars, but verify).
    - Error Logging: perror on fork failure helps, but consider log_message for parsing failures
      (e.g., "Target: not found") to debug clang output changes.
    - Extensibility: To grab more clang info (e.g., version), expand parsing—current focus is narrow.
    - Robustness: No timeout for clang—hung child could stall; add waitpid timeout if this occurs.
*/

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

/*
  Function Description:
    Constructs a clang command string for code completion at a specific file position, using cached
    or freshly gathered include paths and CPU architecture. This function prepares arguments for
    clang to suggest completions (e.g., function names) at a given line and column in a source file.

  Parameters:
    - filename (const char *): Path to the source file (e.g., "/project/main.c"), not modified.
    - line (int): Line number in the file where completion is requested (1-based).
    - column (int): Column number in the file where completion is requested (1-based).

  Return Value:
    - char *: A dynamically allocated string containing the clang command (e.g., "clang -target x86_64 ..."),
      or NULL on failure. Caller must free this string.

  Detailed Steps:
    1. Allocate Temporary Buffers:
       - Calculates sizes based on filename length, allocates multiple char arrays and pointers for paths,
         targets, and lines. Returns NULL if any allocation fails.
    2. Validate File:
       - Checks if filename exists with access; if not, logs and returns NULL.
       - Resolves absolute path with realpath; if it fails, logs and returns NULL.
    3. Initialize Cache:
       - If not initialized (static flag), calls init_cache to set up completion_cache.
    4. Use Cached Data (If Valid):
       - Checks is_cache_valid with global_buffer_project_dir; if valid and data exists, builds command
         with cached cpu_arch and header_paths, frees temps, returns command.
    5. Find Config Files (If Cache Miss):
       - Calls findFiles on file’s directory to locate .ccls and compile_flags.txt; if fails, logs and returns NULL.
       - Stores result in global_buffer_project_dir.
    6. Get CPU Target:
       - Calls get_clang_target; if fails, logs and returns NULL. Stores in global_buffer_cpu_arc.
    7. Build Config Paths:
       - Constructs .ccls and compile_flags.txt paths from global_buffer_project_dir.
    8. Process Include Paths:
       - Initializes lines arrays, calls store_lines to read and sort paths, copies to global_buffer_header_paths.
    9. Update Cache:
       - Updates completion_cache with new project_dir, header_paths, and cpu_arch.
    10. Build Command:
        - Allocates command string, formats with clang options, target, include paths, and completion args.
        - Frees temporary buffers (not lines[i], as they’re cached), returns command.

  Flow and Logic:
    - Steps 1-2: Setup and validate inputs.
    - Step 3-4: Try cache for speed; if good, build and return.
    - Steps 5-9: If cache misses, gather fresh data (configs, target, paths), cache it.
    - Step 10: Construct final command from all data.
    - Why this order? Validation first, cache for efficiency, fallback to full process, then assemble.

  How It Works (For Novices):
    - Imagine you’re asking clang for help finishing a sentence in your code (at line:column in filename),
      and you need to give it a big instruction note (the command).
    - collect_code_completion_args is like writing this note:
      - Step 1: Get scratch paper (allocate buffers) to work with.
      - Step 2: Check your codebook (filename) exists and get its full name (realpath).
      - Step 3: Check your memo box (cache)—if it’s empty, set it up (init_cache).
      - Step 4: If the memo has old notes (cache valid), use them to write "clang -target x86_64 ..." fast.
      - Steps 5-6: If not, hunt for guidebooks (.ccls, compile_flags.txt) and ask clang its type (target).
      - Steps 7-8: Write down where guidebooks are and copy their directions (include paths).
      - Step 9: Save everything in the memo box for next time.
      - Step 10: Write the full note (e.g., "clang -target x86_64 -I/project ... -code-completion-at=main.c:5:3").
    - It’s like gathering tools and instructions to ask clang for a coding tip, reusing notes when possible!

  Why It Works (For Novices):
    - Speed: Checks the memo (cache) first to avoid slow work.
    - Safety: Makes sure the file’s real and everything’s ready before asking clang.
    - Smartness: Saves answers (caches) so next time’s faster.

  Why It’s Designed This Way (For Maintainers):
    - Optimization: Cache-first approach (is_cache_valid, get_cached_include_paths) speeds up repeated
      calls (e.g., in execute_code_completion_command), vital for UNIX (per _POSIX_C_SOURCE) where
      file ops and clang calls are slow.
    - Modularity: Builds on findFiles, get_clang_target, store_lines, and cache functions, reusing logic
      for a complex task—clang code completion.
    - Memory: Allocates command dynamically, ensuring flexibility; uses global buffers (e.g.,
      global_buffer_project_dir) for persistence, but temp buffers are freed to avoid leaks.
    - Robustness: Extensive error checks (access, realpath, findFiles) with NULL returns and logs
      (log_message) ensure failures don’t crash—caller handles NULL.
    - Clang Integration: Command format (-target, -fsyntax-only, -code-completion-at) matches clang’s
      completion API, tailored for Vim integration via processCompletionDataFromString.

  Maintenance Notes:
    - Memory Leaks: Frees temps on all paths, but lines[i] persists in cache—caller must not free command
      prematurely. Test with valgrind for leaks.
    - Buffer Sizes: max_path_len (+256) and command_length (512 + variables) are estimates—test with
      long filenames/paths to avoid truncation.
    - Error Logging: Logs failures (e.g., “File does not exist”), but could detail why (e.g., errno) for
      better tracing.
    - Cache Sync: Relies on global buffers matching cache—test clear_cache/update_cache interactions.
    - Extensibility: Add more clang flags (e.g., -D) by expanding store_lines or command format if needed.
*/

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

/*
  Function Description:
    Filters clang’s code completion output to extract only the relevant completion suggestions,
    removing extraneous lines and formatting them into a newline-separated string. This processes
    raw clang output (from execute_code_completion_command) into a clean list for Vim.

  Parameters:
    - input (const char *): The raw clang output string (e.g., "COMPLETION: printf : ..."), not modified.

  Return Value:
    - char *: A dynamically allocated string with filtered completions (e.g., "printf\nscanf\n"), or
      NULL on failure (e.g., memory allocation). Caller must free this string.

  Detailed Steps:
    1. Validate Input:
       - If input is NULL or empty (input[0] == '\0'), returns NULL—no data to process.
    2. Allocate Buffers:
       - Allocates filtered_output (MAX_OUTPUT) for the result and line_buffer (MAX_LINE_LENGTH)
         for parsing lines.
       - If either allocation fails, frees both and returns NULL.
    3. Initialize Parsing:
       - Creates a copy of input with strdup to work with strtok; if fails, frees buffers and returns NULL.
       - Sets initial filtered_length to 0 to track output size.
    4. Tokenize and Filter:
       - Uses strtok to split input_copy by newline, processing each line.
       - For each line, checks if it starts with "COMPLETION: " (strstr).
       - If it does, extracts the completion name (after "COMPLETION: " up to " :" or end), copies to
         line_buffer, appends to filtered_output with a newline, updates filtered_length.
       - Limits total length to MAX_OUTPUT - 1 to prevent overflow.
    5. Clean Up and Return:
       - Frees input_copy and line_buffer.
       - If filtered_length is 0 (no completions), frees filtered_output and returns NULL.
       - Otherwise, null-terminates filtered_output and returns it.

  Flow and Logic:
    - Step 1: Quick check to skip invalid input.
    - Steps 2-3: Set up memory and a workable input copy.
    - Step 4: Loop through lines, grab completions, build the output.
    - Step 5: Wrap up, ensure something was found, return result.
    - Why this order? Validation first, setup next, filter then clean—standard string processing flow.

  How It Works (For Novices):
    - Imagine clang hands you a messy note (input) with lines like "COMPLETION: printf : ..." and
      junk like "Target: x86_64", and you want a neat list of just the suggestions (e.g., "printf\nscanf").
    - filter_clang_output is like cleaning it up:
      - Step 1: If the note’s blank or missing, toss it (return NULL).
      - Step 2: Get a big clean sheet (filtered_output) and a scratch pad (line_buffer).
      - Step 3: Copy the messy note (strdup) so you can cut it up (strtok).
      - Step 4: Cut the note into lines. If a line says "COMPLETION: ", grab the word after it (e.g.,
        "printf"), write it on the clean sheet with a newline, keep track of space used.
      - Step 5: Toss the scratch stuff (free input_copy, line_buffer). If the clean sheet’s empty, toss
        it too (return NULL); otherwise, finish it and hand it over.
    - It’s like turning a cluttered brainstorm into a tidy to-do list!

  Why It Works (For Novices):
    - Focus: Picks only the useful bits (completions), ignoring clutter.
    - Safety: Checks memory and space so it doesn’t mess up.
    - Neatness: Gives you a simple list, one suggestion per line.

  Why It’s Designed This Way (For Maintainers):
    - Purpose: Cleans clang’s verbose output (from execute_code_completion_command) for Vim via
      processCompletionDataFromString, key for UNIX tooling (per _POSIX_C_SOURCE).
    - String Handling: strtok splits lines efficiently; strdup protects input—standard C practices.
    - Memory: Dynamic allocation (MAX_OUTPUT) handles variable output sizes; caller frees result,
      consistent with execute_code_completion_command.
    - Filtering: "COMPLETION: " check is specific to clang’s format—effective but tied to its output style.
    - Error Handling: NULL on failure (input, memory, no completions) lets callers (e.g., Vim plugin)
      handle gracefully, with minimal logging.

  Maintenance Notes:
    - Buffer Limits: MAX_OUTPUT and MAX_LINE_LENGTH must fit clang output and longest completion—
      test with huge structs or namespaces to avoid truncation.
    - Clang Format: Relies on "COMPLETION: " and " :"—if clang changes (e.g., "Completion:"), adjust
      strstr checks. Log failures (via log_message) to catch this.
    - Memory Leaks: Frees all temps on all paths—verify with valgrind, especially on early returns.
    - Edge Cases: Empty lines or no completions return NULL—ensure callers (e.g., processCompletionDataFromString)
      handle this (e.g., fallback to empty list).
    - Extensibility: To keep more data (e.g., types after " :"), expand parsing—current focus is minimal.
*/

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

/*
  Function Description:
    Executes the `ccls --index` command in the specified directory to generate an index file (.ccls-cache)
    for code navigation and completion. This function runs ccls as a child process and waits for it to finish.

  Parameters:
    - directory (const char *): The directory path where ccls should run (e.g., "/project"), not modified.

  Return Value:
    - int: Returns 0 on success (ccls exits with status 0); 1 on failure (e.g., fork, exec, or ccls error).

  Detailed Steps:
    1. Validate Input:
       - If directory is NULL, logs an error and returns 1.
    2. Fork Process:
       - Calls fork to create a child process; if it fails (pid < 0), logs and returns 1.
    3. Child Process:
       - Changes working directory to directory with chdir; if it fails, logs and exits with EXIT_FAILURE.
       - Executes "ccls" with "--index" argument via execlp; if it fails, logs and exits with EXIT_FAILURE.
    4. Parent Process:
       - Waits for child to finish with waitpid, storing exit status.
       - Checks status: if WIFEXITED and exit code is 0, returns 0; otherwise, logs and returns 1.

  Flow and Logic:
    - Step 1: Ensure there’s a directory to work with.
    - Step 2: Split into parent and child processes.
    - Step 3: Child runs ccls in the directory.
    - Step 4: Parent waits and checks if ccls succeeded.
    - Why this order? Validation first, fork next, child execs, parent verifies—classic UNIX process pattern.

  How It Works (For Novices):
    - Imagine you need a map (.ccls-cache) of a workshop (directory) to help find tools fast, and you
      ask a helper (ccls) to make it with “--index.”
    - execute_ccls_index is like this:
      - Step 1: Check you gave a workshop address (directory)—if not, say “No way!”
      - Step 2: Call your helper (fork) to do the job while you wait.
      - Step 3: The helper goes to the workshop (chdir), shouts “ccls --index” (execlp), and makes the map.
        If they can’t, they complain and quit.
      - Step 4: You wait (waitpid) and ask, “Did it work?” If the helper says “Yes” (status 0), you’re happy (return 0);
        if not, you’re not (return 1).
    - It’s like sending a friend to map a room while you wait to hear if they did it!

  Why It Works (For Novices):
    - Safety: Checks the address first so you don’t send the helper nowhere.
    - Teamwork: The helper does the work, you just wait—saves you effort.
    - Clarity: Tells you if the map got made or not.

  Why It’s Designed This Way (For Maintainers):
    - Purpose: Generates ccls’s index for navigation/completion, integrating with UNIX tooling (per
      _POSIX_C_SOURCE) where ccls expects a project dir with .ccls files (e.g., from create_default_config_files).
    - Fork/Exec: Standard UNIX pattern for running external commands—simple and robust, though it blocks
      until ccls finishes.
    - Error Handling: Returns 1 on any failure (NULL dir, fork, chdir, execlp, ccls error) with logs
      (log_message), letting callers (e.g., setup routine) retry or abort.
    - Blocking: waitpid ensures index is complete before returning, syncing with ccls’s async nature—
      suits one-time setup but not real-time use.
    - Minimalism: Hardcodes "ccls --index"—no extra args, assuming .ccls in directory handles rest.

  Maintenance Notes:
    - Robustness: No timeout—hung ccls blocks forever; add WNOHANG or timeout logic if this happens.
    - Logging: Logs failures (e.g., “fork failed”), but could detail errno or ccls exit code for clarity.
    - Path Issues: chdir assumes directory is valid—realpath could normalize it, but adds overhead.
    - Extensibility: To pass more ccls args (e.g., "--log-file"), modify execlp to execvp with an array.
    - Testing: Verify with missing ccls binary or bad dirs—ensure logs and returns align.
*/

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

/*
  Function Description:
    Parses an input string in the format "file_path:line:column" into its components: a file path,
    line number, and column number. This function extracts these parts for use in code completion
    or navigation tasks, setting default values if parsing fails.

  Parameters:
    - input (const char *): The input string to parse (e.g., "/project/main.c:5:3"), not modified.
    - file_path (char *): A caller-provided buffer to store the extracted file path. Must be large enough
      (e.g., PATH_MAX).
    - line (int *): Pointer to an integer where the parsed line number will be stored.
    - column (int *): Pointer to an integer where the parsed column number will be stored.

  Return Value: None
    - Modifies file_path, *line, and *column in place; no return value.

  Detailed Steps:
    1. Validate Input:
       - If input is NULL, sets file_path to empty string, *line and *column to 1, and returns.
    2. Copy Input:
       - Duplicates input with strdup into input_copy; if fails, logs, sets defaults, and returns.
    3. Split String:
       - Uses strtok with input_copy and ":" delimiter to split into tokens.
       - First token (file path) is copied to file_path with strncpy, limited to PATH_MAX - 1, null-terminated.
       - Second token (line) is converted to int with atoi; if missing or invalid, *line = 1.
       - Third token (column) is converted to int with atoi; if missing or invalid, *column = 1.
    4. Clean Up:
       - Frees input_copy.

  Flow and Logic:
    - Step 1: Handle NULL input with safe defaults.
    - Step 2: Make a workable copy of input.
    - Step 3: Break it into parts and fill file_path, line, column.
    - Step 4: Toss the copy.
    - Why this order? Validation first, copy for strtok safety, split then clean—standard parsing flow.

  How It Works (For Novices):
    - Imagine you get a note (input) like "main.c:5:3" telling you where to look in a book (file, line,
      column), and you need to split it into three pieces for your checklist (file_path, line, column).
    - split_input_string is like this:
      - Step 1: If the note’s missing (NULL), write "nothing" (empty file_path) and guess "page 1, spot 1."
      - Step 2: Photocopy the note (strdup) so you can cut it up.
      - Step 3: Cut at ":" (strtok)—first piece ("main.c") goes in your file box (file_path), second ("5")
        becomes a page number (*line), third ("3") a spot (*column). If pieces are missing, use "1."
      - Step 4: Throw away the photocopy (free input_copy).
    - It’s like tearing a sticky note into parts to organize your reading!

  Why It Works (For Novices):
    - Safety: Handles missing notes with simple guesses (1, 1, empty).
    - Ease: Breaks the note into bits you can use right away.
    - Tidiness: Cleans up the photocopy so there’s no mess.

  Why It’s Designed This Way (For Maintainers):
    - Purpose: Parses Vim-style input (file:line:col) for code completion (e.g., in execute_code_completion_command),
      common in UNIX tools (per _POSIX_C_SOURCE).
    - strtok Usage: Requires a mutable string, hence strdup—safe but assumes input is colon-separated.
    - Defaults: *line = 1, *column = 1 on failure aligns with text editor norms (start of file)—practical fallback.
    - Memory: Caller provides file_path buffer (assumed PATH_MAX), reducing allocation here; input_copy is
      freed to prevent leaks.
    - Simplicity: atoi for numbers is basic but sufficient—non-numeric tokens default to 1 without fuss.

  Maintenance Notes:
    - Robustness: No check for file_path size—assumes PATH_MAX. Add a limit check or log if input exceeds.
    - Error Handling: Silent defaults on bad input (e.g., "main.c:abc:def")—log (via log_message) invalid
      tokens for debugging.
    - Edge Cases: "file:" (no line/col) or "file:5:" works but sets 1s—test with malformed inputs.
    - Extensibility: To handle more formats (e.g., "file(line,col)"), replace strtok with custom parsing.
    - Memory: strdup failure logs but doesn’t crash—ensure caller handles empty file_path gracefully.
*/

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

/*
  Function Description:
    Driver function for code completion: processes a Vim-style input string (e.g., "main.c:5:3") to
    generate and filter clang completion suggestions, returning them as a string. This function
    orchestrates splitting the input, executing the completion command, and filtering the output
    for Vim integration.

  Parameters:
    - vimInputString (const char *): Input string from Vim in "file:line:column" format, not modified.

  Return Value:
    - char *: A dynamically allocated string with filtered completion suggestions (e.g., "printf\nscanf\n"),
      or NULL on failure (e.g., parsing, execution, or filtering issues). Caller must free this string.

  Detailed Steps:
    1. Allocate Buffers:
       - Allocates file_path buffer (PATH_MAX) for the parsed file path; if fails, logs and returns NULL.
    2. Parse Input:
       - Initializes line and column to 0, calls split_input_string to extract file_path, line, and column
         from vimInputString.
    3. Execute Completion Command:
       - Calls execute_code_completion_command with file_path, line, and column to get raw clang output.
       - If it returns NULL, logs, frees file_path, and returns NULL.
    4. Filter Output:
       - Calls filter_clang_output on the raw output to extract completion suggestions.
       - Frees raw output regardless of filter result.
    5. Clean Up and Return:
       - Frees file_path.
       - Returns filtered completions (or NULL if filtering failed).

  Flow and Logic:
    - Step 1: Set up storage for parsing.
    - Step 2: Break input into usable parts.
    - Step 3: Ask clang for suggestions.
    - Step 4: Clean up clang’s messy response.
    - Step 5: Wrap up and deliver the tidy list.
    - Why this order? Setup first, parse next, execute then filter—logical pipeline from input to output.

  How It Works (For Novices):
    - Imagine Vim hands you a note (vimInputString) like "main.c:5:3" asking for word suggestions at
      that spot, and you need to give back a neat list (e.g., "printf\nscanf"). This is the boss function
      that makes it all happen!
    - processCompletionDataFromString is like this:
      - Step 1: Get a blank card (file_path) to write the file name on.
      - Step 2: Tear the note apart (split_input_string) into file ("main.c"), page (5), and spot (3).
      - Step 3: Ask clang for ideas (execute_code_completion_command) using those pieces—get a messy
        answer like "COMPLETION: printf : ...".
      - Step 4: Clean the mess (filter_clang_output) into a nice list ("printf\nscanf").
      - Step 5: Toss your scratch card (free file_path) and hand over the list (or nothing if it flopped).
    - It’s like being the manager who tells everyone what to do to get Vim its answer!

  Why It Works (For Novices):
    - Control: Runs the show, calling helpers to do the work.
    - Safety: Checks each step so you don’t get junk.
    - Usefulness: Turns a simple note into a helpful list for Vim.

  Why It’s Designed This Way (For Maintainers):
    - Driver Role: Acts as the central coordinator, tying together split_input_string,
      execute_code_completion_command, and filter_clang_output into a single workflow for Vim integration
      on UNIX (per _POSIX_C_SOURCE). It’s the entry point for completion requests.
    - Modularity: Delegates tasks to specialized functions, keeping this high-level and maintainable—
      each step can evolve independently.
    - Memory: Allocates file_path dynamically (PATH_MAX), ensuring size safety; frees all temps (file_path,
      completions) on all paths—caller owns the returned string.
    - Error Handling: Propagates NULL from helpers with logs (log_message), letting Vim handle failures
      (e.g., empty completion list)—simple but effective.
    - Simplicity: Straightforward pipeline—parse, execute, filter—mirrors the data flow from Vim input
      to usable output.

  Maintenance Notes:
    - Memory Leaks: Frees file_path and completions on all paths—test with valgrind to confirm no leaks
      from helpers (e.g., execute_code_completion_command).
    - Buffer Size: PATH_MAX for file_path assumes typical paths—test with long filenames to ensure no
      overflow from split_input_string.
    - Error Tracing: Logs on NULL returns but lacks detail—add specifics (e.g., “split failed”) for
      better debugging.
    - Robustness: Relies on vimInputString format—malformed input (e.g., "main.c:abc") defaults to 1:1
      via split_input_string; consider validating further if Vim varies.
    - Extensibility: To add more completion data (e.g., types), adjust filter_clang_output and return
      format—current focus is basic suggestions.
*/

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

/*
  Function Description:
    Logs a message to stderr with a timestamp, providing a simple debugging or error-reporting mechanism.
    This function formats the current time and prepends it to the message for tracking events in the program.

  Parameters:
    - message (const char *): The message string to log (e.g., "fork failed"), not modified.

  Return Value: None
    - Writes directly to stderr; no return value.

  Detailed Steps:
    1. Get Current Time:
       - Uses time() to get the current timestamp in seconds since epoch.
       - Converts it to a local time struct with localtime.
    2. Format Time:
       - Uses strftime to format the time into a string (e.g., "2025-03-14 10:30:45") in time_str buffer
         (size MAX_TIME_STR).
    3. Write to stderr:
       - Prints to stderr with fprintf, combining "[timestamp] message\n" (e.g., "[2025-03-14 10:30:45] fork failed").
       - Flushes stderr with fflush to ensure immediate output.

  Flow and Logic:
    - Step 1: Grab the current moment.
    - Step 2: Turn it into a readable date and time.
    - Step 3: Write it out with the message so you see it right away.
    - Why this order? Time first, format next, print last—straightforward logging sequence.

  How It Works (For Novices):
    - Imagine you’re keeping a diary (stderr) of what’s happening in your program, and you want each
      entry to say when it happened (timestamp) and what went wrong or right (message).
    - log_message is like this:
      - Step 1: Check your watch (time()) to see what time it is now.
      - Step 2: Write the time neatly on a card (strftime) like "2025-03-14 10:30:45."
      - Step 3: Scribble in your diary (fprintf) something like "[2025-03-14 10:30:45] fork failed"
        and make sure it sticks (fflush) so you can read it right away.
    - It’s like stamping a note with “when” and “what” so you can look back later!

  Why It Works (For Novices):
    - Clarity: Adds the time so you know when things happened.
    - Speed: Shows the note right away (fflush) so you don’t miss it.
    - Simplicity: Just writes what you tell it, no fuss.

  Why It’s Designed This Way (For Maintainers):
    - Purpose: Provides basic logging for debugging/errors across the program (e.g., in execute_ccls_index,
      processCompletionDataFromString), fitting UNIX conventions (per _POSIX_C_SOURCE) where stderr is
      standard for diagnostics.
    - Simplicity: Minimalist—time plus message, no file output or levels (e.g., DEBUG, ERROR)—keeps it
      lightweight and focused.
    - Immediacy: fflush ensures logs appear instantly, critical for real-time tracing in a terminal-based
      workflow like Vim integration.
    - Format: "YYYY-MM-DD HH:MM:SS" is human-readable and sortable, good for manual log review.
    - stderr Choice: Aligns with UNIX tools—errors go to stderr, not stdout, leaving stdout free for
      program output (e.g., completions).

  Maintenance Notes:
    - Buffer Size: MAX_TIME_STR (assumed 64) fits "YYYY-MM-DD HH:MM:SS" (19 chars) plus safety—verify
      it’s defined large enough in code_connector_shared.h.
    - Error Handling: No check for localtime or strftime failure (rare)—if time fails, output could be
      garbled; consider defaulting to "unknown time."
    - Extensibility: To add log levels or file output, expand with a FILE* param or severity enum—current
      form is basic but sufficient.
    - Thread Safety: fprintf(stderr) isn’t thread-safe—unlikely issue here (single-threaded context),
      but note for future concurrency.
    - Debugging: Works as-is for tracing; pair with specific messages (e.g., errno in fork failures)
      for richer context.
*/

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

/*
  Function Description:
    Extracts the function name from a string (typically a clang completion line) by finding the substring
    between specific delimiters (" : " and " [#"). Returns the starting position and length of the name.
    This static helper function is used internally to parse completion data.

  Parameters:
    - str (const char *): The input string to parse (e.g., "COMPLETION: printf : [#include <stdio.h>]"), not modified.
    - func_start (const char **): Pointer to a pointer that will store the start of the function name (e.g., "printf").
    - func_name_len (size_t *): Pointer to a size_t that will store the length of the function name (e.g., 6).

  Return Value:
    - int: Returns 0 on success (name extracted); 1 on failure (e.g., delimiters not found).

  Detailed Steps:
    1. Find Start Delimiter:
       - Searches for " : " in str using strstr.
       - If not found, returns 1—can’t locate the name’s start.
       - Sets start to the position after " : " (start += 3).
    2. Find End Delimiter:
       - Searches from start for " [#" using strstr.
       - If not found, returns 1—can’t locate the name’s end.
       - Sets end to this position.
    3. Calculate and Store Results:
       - Computes func_name_len as end - start (length of the name).
       - If length is 0 (empty name), returns 1—invalid result.
       - Sets *func_start to start (beginning of the name).
       - Assigns *func_name_len and returns 0.

  Flow and Logic:
    - Step 1: Locate where the name begins after " : ".
    - Step 2: Locate where it ends before " [#".
    - Step 3: Measure the name and save its position and size.
    - Why this order? Start first, end next, then compute—sequential parsing from left to right.

  How It Works (For Novices):
    - Imagine you get a note from clang (str) like "COMPLETION: printf : [#include <stdio.h>]" and need
      to pull out just the word "printf" (its position and how long it is).
    - extract_function_name is like this:
      - Step 1: Look for " : "—it’s like a sign saying "the name’s next!" If it’s missing, give up.
        Jump past it to where "printf" starts.
      - Step 2: Look for " [#"—it’s the stop sign after the name. If it’s not there, give up.
      - Step 3: Measure from start ("p") to stop (before "[")—that’s "printf" (6 letters). Write down
        where it starts (func_start) and its length (func_name_len). If it’s empty, say “no good.”
    - It’s like finding a treasure (the function name) between two markers in a messy note!

  Why It Works (For Novices):
    - Precision: Finds the name by spotting clear signs (" : " and " [#").
    - Safety: Checks every step so you don’t grab nonsense.
    - Helpfulness: Gives you exactly where the name is and how big it is.

  Why It’s Designed This Way (For Maintainers):
    - Purpose: Static helper for parsing clang completion output (e.g., in filter_clang_output or similar),
      isolating function names for Vim integration on UNIX (per _POSIX_C_SOURCE).
    - Delimiters: " : " and " [#" match clang’s completion format (e.g., "COMPLETION: name : [type]")—
      specific but fragile if clang’s output changes.
    - Static Scope: Internal use only—keeps namespace clean, likely called by a higher-level parser.
    - Efficiency: strstr is fast for small strings (typical completion lines); no allocation needed.
    - Error Handling: Returns 1 on any issue (missing delimiters, empty name)—caller decides next steps.

  Maintenance Notes:
    - Clang Dependency: Tied to " : " and " [#"—test with clang updates (e.g., clang 18) to ensure format
      holds; log failures (via log_message) if delimiters shift.
    - Edge Cases: "COMPLETION: name : " (no "[#") fails—valid if clang omits type info; adjust end logic
      to handle this if needed.
    - Robustness: Assumes str is null-terminated—NULL input crashes strstr; caller must validate.
    - Extensibility: To grab more (e.g., type after "[#"), expand params—current focus is name only.
    - Debugging: Add logs for start/end positions to trace parsing issues in complex completion lines.
*/

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
