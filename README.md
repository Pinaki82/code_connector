# Code Connector Vim Plugin Documentation

## Overview

The Code Connector plugin is designed to enhance the Vim editing experience by providing advanced features such as function parameter completion, code snippets, and more. It leverages LLVM Clang for code completion and is optimised for seamless integration with C/C++ projects.

## Installation

1. **Using vim-plug**:
   Add the following line to your `.vimrc` file:
   
   ```vimscript
   Plug 'Pinaki82/Code-Connector'
   ```
   
   Then run `:PlugInstall` in Vim to install the plugin.

2. **Manual Installation**:
   Place the `code_connector` folder in your Vim plugin directory (e.g., `~/.vim/plugin`).

3. **Project Configuration**:
   Create two files in the project root directory: `.ccls` and `compile_flags.txt`. For detailed instructions on generating these files, refer to the [CCLS_GEN](https://github.com/Pinaki82/Tulu-C-IDE/tree/main/CCLS_GEN) directory of the [Tulu-C-IDE](https://github.com/Pinaki82/Tulu-C-IDE) repository.

## Usage

- **Function Parameter Completion**:
  Press `<C-CR>` (Ctrl+Enter) to complete function parameters with placeholders once the function name completion is obtained from a LLVM Clang-based completion plugin.
  
  - Add the first bracket, i.e., `(` after the function name in Insert Mode, such as `double result = remainderf(`, and hit Enter. Placeholders can be selected by pressing Ctrl+Enter.
  - **Always press the Enter key after the first bracket.**

- **Code Snippets**:
  Press `<C-x>` followed by `<C-CR>` ('CTRL + x' and then 'CTRL + Enter') to get a list of available code snippets. Select a snippet using the Up/Down Arrow keys. Snippet names are abbreviated.

- **Reloading the Buffer**:
  If an undesirable output gets in the way, reload the buffer using the [Reload-Button](https://github.com/Pinaki82/Reload-Button) plugin or press `u` in the Normal Mode.

## Configuration

- `g:disable_codeconnector`: Disable the Code Connector plugin (default: enabled).
- `g:completekey`: The key used to complete function parameters and keywords (default: `<C-CR>`).
- `g:rs` and `g:re`: Region start and stop markers (can be changed as needed).
- `g:user_defined_snippets`: File name of user-defined snippets.
- `g:CodeComplete_Ignorecase`: Use ignore case for keywords (default: disabled).

## Repository

For more information, visit the [Tulu-C-IDE](https://github.com/Pinaki82/Tulu-C-IDE) repository.

## Building and Running

### Linux:

```bash
mkdir -p build
cd build
cmake ..
make config=Debug platform=Linux
# Or,
make config=Release platform=Linux
```

### Windows:

```bash
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make config=Debug
# Or,
mingw32-make config=Release
```

### Post-Build Steps:

Enter `code_connector/build/plugin`.

Copy the following files to the `code_connector/plugin` directory:

- `code_connector_executable`
- `libcode_connector_shared.so` (on Linux systems)
- `code_connector_executable.exe`
- `ccls_index_gen_windows.exe`
- `code_connector_shared.dll`

Binary files for Intel 64-bit Linux and Microsoft Windows architectures are provided.

## Example Commands

### Linux:

```bash
./code_connector_executable file.c 12 24
./code_connector_executable file1.c 10 13
./code_connector_executable file2.c 9 13
```

### Windows:

```bash
code_connector_executable.exe file.c 12 24
code_connector_executable.exe file1.c 10 13
code_connector_executable.exe file2.c 9 13
```

## CMakeLists.txt

The `CMakeLists.txt` file is used to configure the build process for the plugin. It supports both Linux and Windows platforms and includes settings for different build types (Debug and Release).

## Source Files

- `code_connector_executable.c`: Main executable for code completion.
- `code_connector_executable_windows.c`: Windows-specific implementation for code completion.
- `ccls_index_gen_windows.c`: Windows-specific implementation for generating CCLS index.
- `code_connector_shared.c`: Shared library for common functionality.
- `code_connector_shared_windows.c`: Windows-specific implementation for shared library.

## Header Files

- `code_connector_shared.h`: Header file for shared library.

## Snippets and Templates

The plugin includes a variety of code snippets and templates for C and C++ languages. These can be customised and extended to fit specific needs.

## Logging

The plugin includes a logging mechanism to help with debugging and monitoring its unexpected behaviour. Logs are written to `/tmp/vim_parser_log.txt` on Linux and `C:\Temp\vim_parser_log.txt` on Windows.

## Vim Help

Ensure this file is placed in the `doc/` directory of Vim or the `doc/` directory of your plugin (e.g., `$HOME/.vim/plugged/code_connector/doc/`). To access the plugin's help documentation within Vim, use the following commands:

```vimscript
:helptags $HOME/.vim/doc
:h code_connector
```

## Contributing

Contributions are welcome! Please follow the guidelines in the repository for submitting pull requests and reporting issues.
