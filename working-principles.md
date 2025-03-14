### Overview of the Program’s Purpose - For Maintainers

The program is a **snippet completion tool** tailored for Vim, aimed at enhancing the coding experience for C/C++ developers. It bridges Vim with the power of LLVM’s `clang` compiler and the `ccls` language server to:

1. **Provide Real-Time Snippet Completion**: Offer function signatures, variable types as placeholders in the argument, fixed template completion, and other contextual suggestions at a specific cursor position (file:line:column).
2. **Support Project Configuration**: Locate and parse `.ccls` and `compile_flags.txt` files to understand include paths, compiler flags, and project structure.
3. **Optimise Performance**: Use caching while providing completion to avoid unnecessary computation of project settings and CPU architecture.
4. **Handle UNIX-Specific Quirks**: Adapt to platform-specific behaviours (e.g., the UNIX requirement for config files to be one directory level up).

The ultimate goal is to integrate seamlessly with a Vim plugin (which is `code_connector.vim`), allowing developers to trigger completions from inputs like `:%p` and receive formatted suggestions (e.g., ```double result = remainderf(`<float x>`, `<float y>`)```) derived from `clang`’s output.

---

### LLVM Clang backend

```bash
clang -fsyntax-only -I... -I... -I... -Xclang -code-completion-macros -Xclang -code-completion-at=file.c:32:5 file.c
```

This is how the program derives completion information from Clang.

---

### Program Structure: Parts and Components

The codebase consists of **two main parts** that work together:

1. **Shared Library (`libcode_connector_shared.so`)**:
   
   - **Purpose**: Contains core functionality for code completion, caching, and file handling, intended to be loaded by Vim or other tools such as the executable that talks to Vim.
   - **Key Functions**:
     - `processCompletionDataForVim`: Processes input from Vim and stores results in a global buffer (currently redundant but kept for potential future use).
     - `transfer_global_buffer`: Returns the global result buffer to the caller executable or other callers.
     - `vim_parser`: Reads results from a temp file (redundant but retained for testing). Currently it is not being called by any other functions.
   - **Scope**: Provides reusable logic that could be called directly from Vim via a shared library interface (e.g., using Vim’s `libcall()`).

2. **Executable (e.g., `code_connector_executable`)**:
   
   - **Purpose**: A standalone binary that Vim invokes with a command-line argument (e.g., `file.c line column`), processes completion data, and returns results via `stdout` or a temp file.
   - **Key Function**: `processCompletionDataFromString`: The driver function that takes Vim’s input string, executes the completion pipeline, and returns a dynamically allocated string.
   - **Scope**: `processCompletionDataForVim`  was designed to be called externally by Vim (e.g., via `:!code_connector %:p line column`), with results piped back or written to a file.

**Additional Components**:

- **Header File (`code_connector_shared.h`)**: It defines constants (e.g., `MAX_OUTPUT`, `PATH_MAX`), structs (e.g., `CodeCompletionCache`), and function prototypes shared between the library and executable. It also defines platform-specific preprocessor directives and includes necessary system header files.
- **Vim Plugin (`code_connector.vim`)**: Assumed to exist separately, it interfaces with the executable and/or library, passing cursor position data and utilising results.

**Total Parts**: 

- 1 shared library (`.so`).
- 1 executable (binary).
- Supporting files (header, Vim plugin).

---

### Program Flow: How It Works

Here’s the step-by-step flow of how the program operates, focusing on the executable path (since `processCompletionDataFromString` is the driver) and noting the library’s role:

#### 1. **Input from Vim**

- **Trigger**: The user invokes a Vim command (e.g., `:call CallCodeCompletionExec()` or a mapping), passing the current file path, line, and column (e.g., `"file.c 5 10"`).
- **Entry Point**: Vim calls the executable (`code_connector "file.c 5 10"`) or, historically, the shared library function `processCompletionDataForVim`. However, `processCompletionDataFromString` presently handles the task of processing the pipeline.
- **Function**: `processCompletionDataFromString` receives the input string.

#### 2. **Parsing Input**

- **Action**: `split_input_string` splits `"file.c 5 10"` into `file_path = "file.c"`, `line = 5`, `column = 10`.
- **Purpose**: Prepares data for the completion engine.
- **Error Handling**: Logs invalid formats or memory issues.

#### 3. **Executing Code Completion**

- **Function**: `execute_code_completion_command(file_path, line, column)`:
  
  - **Step 3.1: Collect Arguments** (`collect_code_completion_args`):
    
    - **Cache Check**: If `global_buffer_project_dir` is valid and cached, reuses include paths and CPU architecture.
    - **Find Config Files**: Calls `findFiles` to locate `.ccls` and `compile_flags.txt`, starting from the file’s directory (e.g., `/project/src`), climbing to root if needed.
      - **UNIX Quirk**: Works when files are in `/project` (one level up), setting `global_buffer_project_dir`.
    - **Get CPU Arch**: `get_clang_target` runs `clang --version` to extract the target (e.g., `x86_64-unknown-linux-gnu`), caches it in `global_buffer_cpu_arc`.
    - **Read Config**: `store_lines` parses `.ccls` and `compile_flags.txt` for `-I` and `-isystem` flags, storing them in `global_buffer_header_paths`.
    - **Build Command**: Constructs a `clang` command (e.g., `clang -target x86_64... -I... -Xclang -code-completion-at=file.c:5:10 file.c`).
  
  - **Step 3.2: Run Command**:
    
    - Uses `popen` to execute the `clang` command, capturing output like:
      
      ```
      COMPLETION: remainderf : [#float#]remainderf(<#float x#>, <#float y#>)
      ```
    
    - Returns the raw output string.

#### 4. **Filtering Output**

- **Function**: `filter_clang_output`:
  - **Pattern Matching**: Uses regex to parse `clang`’s completion output, extracting function names and parameters.
  - **Transformation**: Converts to a Vim-friendly format (e.g., `remainderf(<float x>, <float y>)`).
  - **Output**: Returns the first line of filtered results.

#### 5. **Returning Results**

- **Executable Path**:
  - `processCompletionDataFromString` returns the filtered string (e.g., ```remainderf(`<float x>`, `<float y>`)```), which Vim reads from `stdout` or a temp file (via `writeResultToTempFile` if used).
- **Library Path (Redundant)**:
  - `processCompletionDataForVim` stores results in `global_result_buffer`, which `transfer_global_buffer` exposes to Vim.
- **Vim Display**: The plugin displays the suggestion at the cursor.

#### 6. **Caching and Optimisation**

- **Mechanism**: `init_cache`, `update_cache`, `is_cache_valid`, etc., store project directory, include paths, and CPU architecture in `completion_cache`. Caching is presently done in-memory, minimising read/write to the permanent storage, reducing wear and tear on the drive.
- **Purpose**: Avoids repeated `read_files` and `clang --version` calls when working in the same project.

#### 7. **Indexing (Optional)**

- **Function**: `execute_ccls_index`:
  - Runs `ccls --index /project` to generate `.ccls-cache` when called with a directory (e.g., via Vim’s `:%p`).
  - Uses `findFiles` to locate the root of the project.

---

### Detailed Flow Example

**Scenario**: User types `remainderf(` in `/project/src/file.c` at line 5, column 10, presses a completion key as the trigger.

1. **Vim**: Executes `code_connector "/project/src/file.c 5 10"`.
2. **Split**: `file_path = "/project/src/file.c"`, `line = 5`, `column = 10`.
3. **Completion**:
   - `findFiles("/project/src", found_at)` finds `/project` (where `.ccls` and `compile_flags.txt` are).
   - `global_buffer_project_dir = "/project"`.
   - `get_clang_target` sets `global_buffer_cpu_arc = "x86_64-unknown-linux-gnu"` on Linux (for example).
   - `store_lines` populates `global_buffer_header_paths` with `-I/project/include`, etc, derived from the listed entries found in the files `.ccls` and `compile_flags.txt`.
   - Command: `clang -target x86_64... -I/project/include -I... -Xclang -code-completion-at=/project/src/file.c:5:10 /project/src/file.c`.
   - Output: `COMPLETION: remainderf : [#float#]remainderf(<#float x#>, <#float y#>)`.
4. **Filter**: `filter_clang_output` returns ```remainderf(`<float x>`, `<float y>`)```.
5. **Return**: `processCompletionDataFromString` sends this to the executable binary `code_connector_executable` which was invoked by Vim.
6. **Vim**: Inserts ```remainderf(`<float x>`, `<float y>`)``` as the completion.

---

### How Many Parts

- **2 Core Components**:
  - Shared Library: 1. For the executable application that communicates with Vim, and 2. Utilising Vim's libcallnr() function to directly integrate Vim with the shared library for creating CCLS Index Cache files on Linux.
  - Executable: Primary interface between Vim and interaction via command-line calls.
- **Supporting Elements**:
  - Header file for shared definitions.
  - Vim plugin for UI and invocation.

---
