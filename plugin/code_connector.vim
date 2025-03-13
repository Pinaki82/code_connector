"==================================================
" File:         code_connector.vim
" Brief:        function parameter complete, code snippets, and much more.
" Author:       Pinaki Sekhar Gupta <appugupta007 AT gmail DOT com>
" Last Change:  2025-03-04 00:23:44 IST
" Version:      3.0
"
" **Code Connector Plugin**
" ==========================
"
" **Overview**
" ------------
"
" The Code Connector plugin is a comprehensive tool designed to enhance the Vim
" editing experience. It offers features such as function parameter completion,
" code snippets, and more.
"
" **Installation**
" ---------------
"
" To install the plugin, follow these steps:
"
" 1. Place the `code_connector.vim` file in the plugin directory.
" If you have Cream (https://cream.sourceforge.net/) installed, put the plugin
" in the `$VIMRUNTIME` directory and source it in your `_vimrc` file using one
" of the following commands:
"    * `:source $VIMRUNTIME/code_connector.vim`
"    * `:source $HOME/vimfiles/plugin/code_connector.vim`
"    * `:source $HOME/.vim/plugin/code_connector.vim`
" 2. Create two files in the project root directory:
"                                              `.ccls` and `compile_flags.txt`.
" For more information, refer to the
" [CCLS_GEN](https://github.com/Pinaki82/Tulu-C-IDE/tree/main/CCLS_GEN) repository.
"
" If issues arise during the use of this plugin, reload the buffer using the
" [Reload Button](https://github.com/Pinaki82/Reload-Button) plugin.
"
" **Origin and Dependencies**
" -------------------------
"
" This plugin is a modified version of
" [code_complete](https://github.com/mbbill/code_complete) by Ming Bai.
" Unlike the original plugin, which relies on Ctags, Code Connector uses LLVM Clang.
"
" **Default Key Mapping**
" ---------------------
"
" The default completion key mapping has been changed
" from `<Tab>` to `<C-CR>` (Ctrl+Enter) to avoid conflicts with other plugins,
" such as Supertab.
"
" **Usage**
" -----
"
" ### Hotkeys
"
" * `<C-CR>` (default value of `g:completekey`): Press this key to complete
" function parameters and keywords.
" Add the first bracket after the function name,
" such as `double result = remainderf(`, and hit Enter.
" Always press the Enter key after the first bracket.
"
" ### Examples
"
" * Press `<C-CR>` after a function name and opening parenthesis to
"   complete the function parameters:
"
"   * `foo ( <C-CR>` becomes `foo ( `<first param>`,`<second param>` )`
" * Press `<C-CR>` after a code template to expand it:
"   * `if <C-CR>` becomes:
"     ```c
" if( `<...>` )
" {
"     `<...>`
" }
" ```
"
" ### Variables
"
" The following variables can be customised:
"
" * `g:disable_codeconnector`: Disable the Code Connector plugin (default: enabled).
" * `g:completekey`: The key used to complete function parameters and keywords (default: `<C-CR>`).
" * `g:rs` and `g:re`: Region start and stop markers (can be changed as needed).
" * `g:user_defined_snippets`: File name of user-defined snippets.
" * `g:CodeComplete_Ignorecase`: Use ignore case for keywords (default: disabled).
"
" ### Keywords
"
" Refer to the "templates" section for a list of available keywords.
" Alternatively, type `<C-x> <C-CR>` (Ctrl+x followed by Ctrl+Enter) in a blank
" space to get a full list of keywords for snippets. Use the UP-DOWN arrow keys
" to select the desired snippet name and hit Enter.
" Press `<C-CR>` (Ctrl+Enter) to expand the snippet.
"
" **Repository**
" --------------
"
" For more information, visit the [Tulu-C-IDE](https://github.com/Pinaki82/Tulu-C-IDE) repository.
"==================================================
"
if v:version < 700
    finish
endif

if exists("g:disable_codeconnector")
    finish
endif

" Activate the plugin only for C/C++ files
augroup CodeConnector
    autocmd!
    autocmd BufReadPost,BufNewFile *.c,*.cpp,*.h,*.hpp,*.ino,*.pde,*.cxx,*.hxx,*.C,*.H call CodeCompleteStart()
augroup END

" Variable Definitions: {{{1
" options, define them as you like in vimrc:
if !exists("g:completekey")
    let g:completekey = "<C-CR>"   "hotkey
endif

if !exists("g:rs")
    let g:rs = '`<'    "region start
endif

if !exists("g:re")
    let g:re = '>`'    "region stop
endif

if !exists("g:user_defined_snippets")
    let g:user_defined_snippets = ""
endif

" ----------------------------
let s:expanded = 0  "in case of inserting char after expand
let s:signature_list = []
let s:jumppos = -1
let s:doappend = 1

" Autocommands: {{{1
autocmd BufReadPost,BufNewFile * call CodeCompleteStart()

" Menus:
menu <silent>       &Tools.Code\ Complete\ Start          :call CodeCompleteStart()<CR>
menu <silent>       &Tools.Code\ Complete\ Stop           :call CodeCompleteStop()<CR>

" Function Definitions: {{{1

function! CodeCompleteStart()
    exec "silent! iunmap  <buffer> ".g:completekey
    exec "inoremap <buffer> ".g:completekey." <c-r>=CodeComplete()<cr><c-r>=SwitchRegion()<cr>"
endfunction

function! CodeCompleteStop()
    exec "silent! iunmap <buffer> ".g:completekey
endfunction

" Function to Determine Executable and Shared Library Paths
function! GetExecutableAndLibraryPaths()
    if has('win32') || has('win64')
        let s:codeConnectorTestExecutable = fnamemodify(resolve(expand('<script>')), ':p:h') . '/code_connector_executable.exe'
        let s:ccls_index_gen_exec_mswin = expand('<script>:p:h') . '/ccls_index_gen_windows.exe'
    else
        let s:codeConnectorTestExecutable = fnamemodify(resolve(expand('<script>')), ':p:h') . '/code_connector_executable'
        let s:codeConnectorSharedLibrary = fnamemodify(resolve(expand('<script>')), ':p:h') . '/libcode_connector_shared.so'
    endif
endfunction

" Call the function to set the paths when the plugin is loaded
call GetExecutableAndLibraryPaths()

" --------------------------------------------"

" Define the function to call the Clang-based executable
function! CallCodeCompletionExec()
    " Define the log file path
    let s:logFilePath = $HOME . '/vim_code_completion.log'

    " Get the full path of the file being edited
    let filepath = expand('%:p')  " Full path of the current file

    " Get the directory of the current file
    let filedir = fnamemodify(filepath, ':h')  " Extract directory path

    " Get the filename without the path and its base name and extension
    let filename = fnamemodify(filepath, ':t:r')  " Base name without extension
    let extension = fnamemodify(filepath, ':e')  " File extension

    " Construct a temporary file name with the original extension
    let tmpfilename = filename . '.tmp.' . extension
    let tmpfilepath = filedir . '/' . tmpfilename  " Full path to the temporary file

    " Write the current buffer to the temporary file
    execute 'silent write! ' . tmpfilepath
    "echom "Temporary file saved to: " . tmpfilepath

    " Get the current cursor position
    let line_num = line('.')
    let col_num = col('.') - 1    " Important adjustment
    let combined_input = tmpfilepath . ' ' . line_num . ' ' . col_num

    " Log the input data
    call writefile(['Input data: ' . combined_input], s:logFilePath, 'a')

    " Define the path to the Clang-based executable file
    " let s:codeConnectorTestExecutable = $HOME . '/.vim/plugin/code_connector/code_connector_executable'
                                    " = '/path/to/your/clang-based-executable'
    " let s:codeConnectorTestExecutable = expand('<sfile>:p:h') . '/code_connector_executable'
    "let s:codeConnectorTestExecutable = fnamemodify(resolve(expand('<script>')), ':p:h') . '/code_connector_executable'
    "echom "Executable path: " . s:codeConnectorTestExecutable  " Debugging output"

    " -------------------------------------------------------------
    " Explanation:
    " expand('<sfile>:p:h'):
    "    <sfile>: The name of the current script file.
    "    :p: The full path of the current script file.
    "    :h: The directory part of the path.
    " This expression dynamically resolves to the directory where the current script file is located, ensuring that the path to the executable is correctly constructed regardless of the user's environment setup.
    " Benefits
    " Portability: This approach makes your plugin more portable and easier to set up, as it doesn't rely on hardcoded paths.
    " Flexibility: Users can place the plugin in different directories without needing to adjust the path manually.
    " -------------------------------------------------------------

    " Determine the paths for the executable and shared library
    call GetExecutableAndLibraryPaths()

    " Call the executable file and capture its output as a list of lines
    let processed_output = systemlist(s:codeConnectorTestExecutable . ' ' . combined_input)
    "echom "Produced by the executable: " . join(processed_output, "\n")

    " Convert the output to the correct encoding
    let processed_output = map(processed_output, 'iconv(v:val, "UTF-8", "UTF-8")')

    " Log the output data
    call writefile(['Output data: ' . join(processed_output, "\n")], s:logFilePath, 'a')

    " Check if the output is not empty
    if !empty(processed_output)
        " Replace the current line with the processed output
        try
            call setline('.', processed_output[0])  " Replace the current line with the first line of output
            if len(processed_output) > 1
                call append(line('.'), processed_output[1:])  " Append any additional lines below
            endif
        catch
            " Log any errors that occur during processing
            call writefile(['Error: ' . v:exception], s:logFilePath, 'a')
        endtry
    else
        " Log the error if the output is empty
        call writefile(['Error: No output from the executable'], s:logFilePath, 'a')
    endif

    " Clean up: Delete the temporary file after logging
    call delete(tmpfilepath)
endfunction

" Bind the function to a key in insert mode
"inoremap <silent> <C-CR> <C-R>=CallCodeCompletionExec()<ESC>

" Bind the function to a key in visual mode
"vnoremap <silent> <C-CR> <Cmd>call CallCodeCompletionExec()<ESC>

" Bind the function to a key in normal mode
"nnoremap <silent> <C-CR> :call CallCodeCompletionExec()<ESC>

" --------------------------------------------"

function! ExpandTemplate(cword)
    let cword = substitute(getline('.')[:(col('.')-2)],'\zs.*\W\ze\w*$','','g')
    if has_key(g:template,&ft)
      if ( exists('g:CodeComplete_Ignorecase') && g:CodeComplete_Ignorecase )
        if has_key(g:template[&ft],tolower(a:cword))
            let s:jumppos = line('.')
            return "\<c-w>" . g:template[&ft][tolower(a:cword)]
        endif
      else
        if has_key(g:template[&ft],a:cword)
            let s:jumppos = line('.')
            return "\<c-w>" . g:template[&ft][a:cword]
        endif
      endif
    endif
    if ( exists('g:CodeComplete_Ignorecase') && g:CodeComplete_Ignorecase )
      if has_key(g:template['_'],tolower(a:cword))
          let s:jumppos = line('.')
          return "\<c-w>" . g:template['_'][tolower(a:cword)]
      endif
    else
      if has_key(g:template['_'],a:cword)
          let s:jumppos = line('.')
          return "\<c-w>" . g:template['_'][a:cword]
      endif
    endif
    return ''
endfunction

function! SwitchRegion()
    if len(s:signature_list)>1
        let s:signature_list=[]
        return ''
    endif
    if s:jumppos != -1
        call cursor(s:jumppos,0)
        let s:jumppos = -1
    endif
    if match(getline('.'),g:rs.'.*'.g:re)!=-1 || search(g:rs.'.\{-}'.g:re)!=0
        normal 0
        call search(g:rs,'c',line('.'))
        normal v
        call search(g:re,'e',line('.'))
        if &selection == "exclusive"
            exec "norm l"
        endif
        return "\<c-\>\<c-n>gvo\<c-g>"
    else
        if s:doappend == 1
            if g:completekey == "<C-CR>"
                return "\<C-CR>"
            endif
        endif
        return ''
    endif
endfunction

function! CodeComplete()
    let s:doappend = 1
    let function_name = matchstr(getline('.')[:(col('.')-2)], '\zs\w*\ze\s*(\s*$')
    if function_name != ''
        " Trigger Clang-based code completion
        call CallCodeCompletionExec()
        let s:doappend = 0
        return ''
    else
        let template_name = substitute(getline('.')[:(col('.')-2)], '\zs.*\W\ze\w*$', '', 'g')
        let tempres = ExpandTemplate(template_name)
        if tempres != ''
            let s:doappend = 0
        endif
        return tempres
    endif
endfunction

" [Get converted file name like __THIS_FILE__ ]
function! GetFileName()
    let filename=expand("%:t")
    let filename=toupper(filename)
    let _name=substitute(filename,'\.','_',"g")
    "let _name="__"._name."__"
    return _name
endfunction

" Templates: {{{1
" to add templates for new file type, see below
"
" "some new file type
" let g:template['newft'] = {}
" let g:template['newft']['keyword'] = "some abbrevation"
" let g:template['newft']['anotherkeyword'] = "another abbrevation"
" ...
"
"
" ---------------------------------------------
"
" ===========================================================================
" Usage:
"
"     Triggering Completion: Users can trigger the completion suggestions by
"     pressing C-CR in insert mode. This will display a list of snippet names
"     that match the current word under the cursor.
"     Selecting a Suggestion: Users can then use the
"     usual completion keys (<C-n> and <C-p>) to navigate through the
"     suggestions and select the desired snippet.

" set dictionary+=$HOME/.vim/plugin//code_connector/snippets.txt
" set complete+=k$HOME/.vim/plugin//code_connector/snippets.txt

" Function Definitions:

" Define the function to provide completion suggestions from a file
function! CompleteFromFile()
    " Define the path to the snippets file using the directory of the current script
    let l:snippet_file = fnamemodify(resolve(expand('<script>')), ':h') . '/snippets.txt'

    " Fallback logic to find the file in the runtime path if not found in the script directory
    if !filereadable(l:snippet_file)
        let l:snippet_file = findfile('plugin/code_connector/snippets.txt', &rtp)
    endif

    " Debugging output to verify the snippet file path
    "echom "Snippet file path: " . l:snippet_file

    " -------------------------------------------------------------
    " I don't want to use any absolute path that might be changed.
    " claude-3-7-sonnet-20250219
    " That's a valid concern. For a more portable solution that doesn't rely on absolute paths, you could try:

    " 1. Use a relative path from your current working directory:
    "```vimscript
    "let l:snippet_file = 'plugin/code_connector/snippets.txt'
    "```

    " 2. Use runtime path to find the file (better for plugins):
    "```vimscript
    " let l:snippet_file = findfile('plugin/code_connector/snippets.txt', &rtp)
    "```

    " 3. If your plugin follows standard structure, use:
    "```vimscript
    " let l:snippet_file = fnamemodify(resolve(expand('<script>')), ':h') . '/snippets.txt'
    "```

    " The `<script>` approach is more reliable than `<sfile>` when used in functions.

    " You could also add fallback logic:
    "```vimscript
    " let l:snippet_file = fnamemodify(resolve(expand('<script>')), ':h') . '/snippets.txt'
    " if !filereadable(l:snippet_file)
    "   let l:snippet_file = findfile('plugin/code_connector/snippets.txt', &rtp)
    " endif
    "```

    " This would first try to find it relative to the script, then fall back to searching in the runtime path.
    " -------------------------------------------------------------

    " Check if the file exists and is readable
    if !filereadable(l:snippet_file)
        echom "Error: Snippet file not found or not readable"
        return ''
    endif

    " Read the snippet names from the file into a list
    let l:lines = readfile(l:snippet_file)
    "echom "Read lines: " . join(l:lines, ', ')  " Debugging output"

    " Get the current word under the cursor
    let l:base = expand('<cword>')
    "echom "Base word: " . l:base  " Debugging output"

    " Filter the snippet names to include only those that start with the current word
    let l:matches = filter(l:lines, 'v:val =~ "^' . l:base . '"')
    "echom "Matches: " . join(l:matches, ', ')  " Debugging output"

    " If there are matching snippet names, provide them as completion suggestions
    if !empty(l:matches)
        " Use the built-in complete() function to provide the completion suggestions
        call complete(col('.'), l:matches)
    endif

    " Return an empty string to indicate that the function has completed successfully
    return ''
endfunction

" Bind the function to a key in insert mode
" This allows users to trigger the completion suggestions by pressing C-CR
inoremap <C-x><C-CR> <C-r>=CompleteFromFile()<CR>

" Menus:
menu <silent>       &Tools.Code\ Complete\ -\ Trigger\ completion\ suggestions\ (\Insert\ Mode\ -\>\ <\CTRL-x\>\ \<\CTRL-Enter\>\)          :call CompleteFromFile()<CR>
" ---------------------------------------------
"
" --------------------------------------------- Function to generate the CCLS Index directory .ccls-cache/ in the project root
if !(has('win32') || has('win64'))
	function! CclsIndexGen()
		" Define the path to the shared library that contains the C functiion `int execute_ccls_index(const char *directory);`
		"let s:codeConnectorSharedLibrary = expand('<script>:p:h') . '/libcode_connector_shared.so'
		" Or,
		" let s:codeConnectorSharedLibrary = findfile('plugin/code_connector/libcode_connector_shared.so', &rtp)
		"echom "Shared Library path: " . s:codeConnectorSharedLibrary  " Debugging output"
		" Determine the paths for the executable and shared library
		call GetExecutableAndLibraryPaths()

		" Define the path to the shared library that contains the C function `int execute_ccls_index(const char *directory);`
		"echom "Shared Library path: " . s:codeConnectorSharedLibrary  " Debugging output"
		let curr_dir = expand('%:h')
		if curr_dir == ''
			let curr_dir = '.'
		endif
		execute 'lcd ' . curr_dir
		call libcallnr(s:codeConnectorSharedLibrary, 'execute_ccls_index', curr_dir)
		execute 'lcd -'
	endfunction
endif

if has('win32') || has('win64')
    function! CclsIndexGen()
        " Determine the paths for the executable and shared library
        call GetExecutableAndLibraryPaths()

        " Define the current directory
        let curr_dir = expand('%:h')
        if curr_dir == ''
            let curr_dir = '.'
        endif

        " Change to the current directory
        execute 'lcd ' . curr_dir

        " Call the executable file and capture its output as a list of lines
        let output = system(s:ccls_index_gen_exec_mswin . ' ' . curr_dir)

        " Log the output data
        let s:logFilePath = $HOME . '/vim_code_completion.log'
        call writefile(['CCLS Index Generation Output: ' . output], s:logFilePath, 'a')

        " Change back to the original directory
        execute 'lcd -'
    endfunction
endif

" Command
command! CclsIndexCreation call CclsIndexGen()

" Menus:
menu <silent>       &Tools.Code\ Complete\ -\ Create\ CCLS\ Index\ (\:CclsIndexGen\)          :call CclsIndexGen()<CR>
" ---------------------------------------------
"
" ---------------------------------------------
" C templates
let g:template = {}
let g:template['c'] = {}
let g:template['c']['cc'] = "/*  */\<left>\<left>\<left>"
let g:template['c']['cd'] = "/**<  */\<left>\<left>\<left>"
let g:template['c']['de'] = "#define  ".g:rs."MACRO_TEMPLATE_DEFINITION_UPPERCASE_ONLY(_with,_arguments,_small,_case)".g:re."  ".g:rs."macro_expansion_value_(or_math_expression)".g:re.""
let g:template['c']['in'] = "#include \"\"\<left>"
let g:template['c']['is'] = "#include <>\<left>"
let g:template['c']['ff'] = "#ifndef  __\<c-r>=GetFileName()\<cr>__\<CR>#define  __\<c-r>=GetFileName()\<cr>__".
            \repeat("\<cr>",5)."#endif  /* __\<c-r>=GetFileName()\<cr>__ */".repeat("\<up>",3)
let g:template['c']['for'] = "for (".g:rs."init".g:re."; ".g:rs."condition".g:re."; ".g:rs."increment".g:re.") {\<cr>}"
let g:template['c']['main'] = "int main(int argc, char \*argv\[\]) {\<cr>".g:rs."...".g:re."\<cr>}"
let g:template['c']['switch'] = "switch ( ".g:rs."...".g:re." ) {\<cr>case ".g:rs."...".g:re." :\<cr>break;\<cr>case ".
            \g:rs."...".g:re." :\<cr>break;\<cr>default :\<cr>break;\<cr>}"
let g:template['c']['if'] = "if( ".g:rs."...".g:re." ) {\<cr>".g:rs."...".g:re."\<cr>}"
let g:template['c']['ifelse'] = "if (".g:rs."condition".g:re.") {\<cr>}\<cr>else {\<cr>}"
let g:template['c']['while'] = "while (".g:rs."condition".g:re.") {\<cr>".g:rs."...".g:re."\<cr>}"
let g:template['c']['ife'] = "if( ".g:rs."...".g:re." )\<cr>{\<cr>".g:rs."...".g:re."\<cr>}\<cr>else\<cr>{\<cr>".g:rs."...".
            \g:re."\<cr>}"
"
" Additional C templates
"
let g:template['c']['case'] = "\<cr>case ".g:rs."...".g:re." :\<cr>break;\<cr>"
let g:template['c']['printf'] = "printf( \"".g:rs."...".g:re."\\n\" );\<left>\<left>"
let g:template['c']['print'] = "printf(\"".g:rs."message".g:re."\\n\");"
let g:template['c']['snprintf'] = "int result = snprintf(".g:rs."buffer".g:re.", ".g:rs."size".g:re.", \"".g:rs."format_string".g:re."\", ".g:rs."...".g:re.");"
" let g:template['c']['log'] = "fprintf(stderr, \"".g:rs."message".g:re."\\n\");"
let g:template['c']['log'] = "fprintf(stderr, \"".g:rs."message".g:re."\\n\");"
let g:template['c']['scanf'] = "scanf( \"%".g:rs."...".g:re."  %".g:rs."...".g:re."\", ".g:rs."&".g:re."".g:rs."...".g:re.", ".g:rs."&".g:re."".g:rs."...".g:re." );\<left>\<left>"
let g:template['c']['sscanf'] = "int result = sscanf(".g:rs."input_string".g:re.", \"".g:rs."format_string".g:re."\", ".g:rs."...".g:re.");"
let g:template['c']['do'] = "do {\<cr> ".g:rs."...".g:re." \<cr>} while (".g:rs."condition".g:re.");\<cr>"
let g:template['c']['elf'] = "else if ( ".g:rs."...".g:re." )\<cr>{\<cr>".g:rs."...".g:re."\<cr>}\<cr>\<Space>\<BS>"
let g:template['c']['else'] = "else\<cr>{\<cr>".g:rs."...".g:re."\<cr>}\<cr>"
let g:template['c']['fin'] = "fflush(stdin);\<cr>\<right>"
let g:template['c']['system'] = "system(\"".g:rs."...".g:re."\");\<left>\<left>"
let g:template['c']['TODO'] = "/* TODO: ".g:rs."...".g:re." */\<left>\<left>\<left>"
let g:template['c']['FIXME'] = "/* FIXME: ".g:rs."...".g:re." */\<left>\<left>\<left>"
let g:template['c']['NOTE'] = "/* NOTE: ".g:rs."...".g:re." */\<left>\<left>\<left>"
let g:template['c']['XXX'] = "/* XXX: ".g:rs."...".g:re." */\<left>\<left>\<left>"
let g:template['c']['enum'] = "enum ".g:rs."function_name".g:re." {\<cr>".g:rs."...".g:re."\<cr>}; /* --- end of enum ".g:rs."function_name".g:re." --- */\<cr>\<left>\<cr>typedef enum ".g:rs."function_name".g:re." ".g:rs."Function_name".g:re.";"
let g:template['c']['struct'] = "struct ".g:rs."srtucture_name".g:re." {\<cr>".g:rs."...".g:re."\<cr>}; /* --- end of struct ".g:rs."srtucture_name".g:re." --- */\<cr>\<left>\<cr>typedef struct ".g:rs."srtucture_name".g:re." ".g:rs."Srtucture_name".g:re.";"
let g:template['c']['union'] = "union ".g:rs."union_name".g:re." {\<cr>".g:rs."...".g:re."\<cr>}; /* --- end of union ".g:rs."union_name".g:re." --- */\<cr>\<left>\<cr>typedef union ".g:rs."union_name".g:re." ".g:rs."Union_name".g:re.";"
let g:template['c']['calloc'] = "".g:rs."int/char/float/TYPE *pointer;".g:re."\<cr>\<Space>\<BS>\<cr>".g:rs."pointer".g:re." = (".g:rs."int/char/float/TYPE".g:re."  *)calloc ( (size_t)(".g:rs."COUNT".g:re."), sizeof(".g:rs."TYPE".g:re.") );\<cr>if ( ".g:rs."pointer".g:re."==NULL ) {\<cr>fprintf ( stderr, \"\\ndynamic memory allocation failed\\n\");\<cr>exit (EXIT_FAILURE);\<cr>}\<cr>free (".g:rs."pointer".g:re.");\<cr>".g:rs."pointer".g:re."	= NULL;\<cr>"
let g:template['c']['malloc'] = "".g:rs."int/char/float/TYPE *pointer;".g:re."\<cr>\<Space>\<BS>\<cr>".g:rs."pointer".g:re." = (".g:rs."int/char/float/TYPE".g:re."  *)malloc (".g:rs." (size_t)COUNT_if_needed  *  ".g:re." sizeof (".g:rs."TYPE".g:re.") );\<cr>if ( ".g:rs."pointer".g:re."==NULL ) {\<cr>fprintf ( stderr, \"\\ndynamic memory allocation failed\\n\");\<cr>exit (EXIT_FAILURE);\<cr>}\<cr>free (".g:rs."pointer".g:re.");\<cr>".g:rs."pointer".g:re."	= NULL;\<cr>"
let g:template['c']['free'] = "free (".g:rs."pointer".g:re.");\<cr>".g:rs."pointer".g:re."	= NULL;\<cr>"
let g:template['c']['realloc'] = "".g:rs."pointer".g:re." = realloc (  ".g:rs."pointer".g:re.", sizeof (".g:rs."TYPE".g:re.") );\<cr>if ( ".g:rs."pointer".g:re."==NULL ) {\<cr>fprintf ( stderr, \"\\ndynamic memory allocation failed\\n\");\<cr>exit (EXIT_FAILURE);\<cr>}\<cr>"
" --------------------------------------
" Simple malloc, calloc, realloc, and free
let g:template['c']['mallocs'] = "void *".g:rs."pointer".g:re." = malloc(".g:rs."size".g:re.");"
let g:template['c']['callocs'] = "void *".g:rs."pointer".g:re." = calloc(".g:rs."num".g:re.", sizeof(".g:rs."type".g:re."));"
let g:template['c']['reallocs'] = "void *".g:rs."pointer".g:re." = realloc(".g:rs."oldPointer".g:re.", ".g:rs."newSize".g:re.");"
let g:template['c']['frees'] = "free(".g:rs."pointer".g:re.");"
" --------------------------------------
let g:template['c']['sizeof'] = "sizeof (".g:rs."TYPE".g:re.")"
let g:template['c']['assert'] = "assert (".g:rs."...".g:re.");\<cr>"
let g:template['c']['filein'] = "FILE	*".g:rs."input-file".g:re.";      /* input-file pointer */\<cr>\<Space>\<BS>\<cr>char	*".g:rs."input-file".g:re."_file_name = \"".g:rs."...".g:re."\";      /* input-file name */ /* use extension within double quotes */\<cr>\<Space>\<BS>\<cr>\<cr>".g:rs."input-file".g:re."	= fopen( ".g:rs."input-file".g:re."_file_name, \"r\" );\<cr>if ( ".g:rs."input-file".g:re."==NULL ) {\<cr>fprintf ( stderr, \"\\ncouldn't open file '%s'; %s\\n\", ".g:rs."input-file".g:re."_file_name,  strerror(errno) );\<cr>exit (EXIT_FAILURE);\<cr>}\<cr>\<cr>else if ( ".g:rs."input-file".g:re."!=NULL ) {\<cr>fprintf ( stderr, \"\\nopened file '%s'; %s\\n\", ".g:rs."input-file".g:re."_file_name,  strerror(errno) );\<cr>\<cr>".g:rs."-continue_here-".g:re."\<cr>\<cr>if ( fclose (".g:rs."input-file".g:re.")==EOF )  {  /* close input file */\<cr>fprintf ( stderr, \"\\ncouldn't close file '%s'; %s\\n\", ".g:rs."input-file".g:re."_file_name,  strerror(errno) );\<cr>exit (EXIT_FAILURE);\<cr>}\<cr>}"
let g:template['c']['fileout'] = "FILE	*".g:rs."output-file".g:re.";      /* output-file pointer */\<cr>\<Space>\<BS>\<cr>char	*".g:rs."output-file".g:re."_file_name = \"".g:rs."...".g:re."\";      /* output-file name */ /* use extension within double quotes */\<cr>\<Space>\<BS>\<cr>\<cr>".g:rs."output-file".g:re."	= fopen( ".g:rs."output-file".g:re."_file_name, \"w\" );\<cr>if ( ".g:rs."output-file".g:re."==NULL ) {\<cr>fprintf ( stderr, \"\\ncouldn't open file '%s'; %s\\n\", ".g:rs."output-file".g:re."_file_name,  strerror(errno) );\<cr>exit (EXIT_FAILURE);\<cr>}\<cr>\<cr>else if ( ".g:rs."output-file".g:re."!=NULL ) {\<cr>fprintf ( stderr, \"\\nopened file '%s'; %s\\n\", ".g:rs."output-file".g:re."_file_name,  strerror(errno) );\<cr>\<cr>".g:rs."-continue_here-".g:re."\<cr>\<cr>if ( fclose (".g:rs."output-file".g:re.")==EOF )  {  /* close output file */\<cr>fprintf ( stderr, \"\\ncouldn't close file '%s'; %s\\n\", ".g:rs."output-file".g:re."_file_name,  strerror(errno) );\<cr>exit (EXIT_FAILURE);\<cr>}\<cr>}"
let g:template['c']['fprintf'] = "fprintf ( ".g:rs."file-pointer".g:re.",  \"\\n\",  ".g:rs."...".g:re."  );\<left>\<left>"
let g:template['c']['fscanf'] = "fscanf ( ".g:rs."file-pointer".g:re.",  \"".g:rs."...".g:re."\",  &".g:rs."...".g:re."  );\<left>\<left>"
let g:template['c']['in1'] = "#include <errno.h>\<cr>#include <stdint.h>\<cr>#include <math.h>\<cr>#include <stdio.h>\<cr>#include <stdlib.h>\<cr>#include <string.h>\<cr>#include <".g:rs."...".g:re.">\<cr>#include <".g:rs."...".g:re.">\<cr>#include \"".g:rs."...".g:re."\"\<cr>#include \"".g:rs."...".g:re."\"\<cr>"
let g:template['c']['ffc'] = "#ifndef  __\<c-r>=GetFileName()\<cr>__\<CR>#define  __\<c-r>=GetFileName()\<cr>__".
            \repeat("\<cr>",2).
            \"\<cr>".g:rs."MACRO, global variables, etc..".g:re."\<cr>".
            \repeat("\<cr>",2)."#include <errno.h>\<cr>#include <math.h>\<cr>#include <stdio.h>\<cr>#include <stdlib.h>\<cr>#include <string.h>\<cr>#include <".g:rs."...".g:re.">\<cr>#include <".g:rs."...".g:re.">\<cr>#include \"".g:rs."...".g:re."\"\<cr>#include \"".g:rs."...".g:re."\"\<cr>".
            \"\<cr>#ifdef __cplusplus\<cr>extern \"C\"\<cr>{\<cr>#endif\<cr>".
            \"\<cr>".g:rs."function prototype".g:re."\<cr>".
            \repeat("\<cr>",3)."#ifdef __cplusplus\<cr>}\<cr>#endif\<cr>".
            \"\<cr>#endif  /* __\<c-r>=GetFileName()\<cr>__ */".repeat("\<cr>",7).repeat("\<up>",3)
let g:template['c']['def'] = "defined( ".g:rs."...".g:re." )"
let g:template['c']['und'] = "#undef ".g:rs."...".g:re.""
let g:template['c']['ifm'] = "#if  ".g:rs."conditions like ||, &&, !, !=, <, >, <=, >= etc. can be used only with #if and #elif macro".g:re."\<cr>       ".g:rs."...".g:re."\<cr>#endif"
let g:template['c']['er'] = "#error  \"".g:rs."write everything within double_quotes".g:re."\""
let g:template['c']['ifd'] = "#ifdef  ".g:rs."...".g:re."\<cr>       ".g:rs."...".g:re."\<cr>#endif"
let g:template['c']['ifn'] = "#ifndef  ".g:rs."...".g:re."\<cr>       ".g:rs."...".g:re."\<cr>#endif"
let g:template['c']['elm'] = "#else\<cr>       ".g:rs."Take_the_Steps_after_#else..".g:re.""
let g:template['c']['eli'] = "#elif ".g:rs."conditions like ||, &&, !, !=, <, >, <=, >= etc. can be used with this macro, since #if is associated".g:re.""
let g:template['c']['en'] = "#endif"
let g:template['c']['lin'] = "#line ".g:rs."...".g:re.""
let g:template['c']['pra'] = "#pragma  ".g:rs."...".g:re.""
" File Open:
let g:template['c']['fileopen'] = "FILE *".g:rs."filePointer".g:re." = fopen(\"".g:rs."fileName".g:re."\", \"".g:rs."mode".g:re."\");"
" File Read:
let g:template['c']['fileread'] = "char buffer[1024];\<cr>fread(buffer, sizeof(char), sizeof(buffer), ".g:rs."filePointer".g:re.");"
" File Write:
let g:template['c']['filewrite'] = "fwrite(".g:rs."data".g:re.", sizeof(char), strlen(".g:rs."data".g:re."), ".g:rs."filePointer".g:re.");"
" File Close:
let g:template['c']['fileclose'] = "fclose(".g:rs."filePointer".g:re.");"
" File Read/Write:
let g:template['c']['filereadwrite'] = "fscanf(".g:rs."filePointer".g:re.", \"".g:rs."format".g:re."\");\<cr>fprintf(".g:rs."filePointer".g:re.", \"".g:rs."format".g:re."\");"
" -------------------------------------- Duplicate
" Struct Definition:
"let g:template['c']['struct'] = "struct ".g:rs."StructName".g:re." {\<cr>};"
" Union Definition:
"let g:template['c']['union'] = "union ".g:rs."UnionName".g:re." {\<cr>};"
" Enum Definition:
"let g:template['c']['enum'] = "enum ".g:rs."EnumName".g:re." {\<cr>};"
" -------------------------------------- Duplicate
" Error Handling. Error Check:
let g:template['c']['errorcheck'] = "if (".g:rs."condition".g:re.") {\<cr>fprintf(stderr, \"".g:rs."error message".g:re."\\n\");\<cr>exit(EXIT_FAILURE);\<cr>}"
" Networking. Socket Creation:
let g:template['c']['socket'] = "int ".g:rs."sockfd".g:re." = socket(AF_INET, SOCK_STREAM, 0);"
" Networking. Bind Socket:
let g:template['c']['bind'] = "struct sockaddr_in ".g:rs."addr".g:re.";\<cr>".g:rs."addr".g:re.".sin_family = AF_INET;\<cr>".g:rs."addr".g:re.".sin_port = htons(".g:rs."port".g:re.");\<cr>".g:rs."addr".g:re.".sin_addr.s_addr = INADDR_ANY;\<cr>if (bind(".g:rs."sockfd".g:re.", (struct sockaddr *)&".g:rs."addr".g:re.", sizeof(".g:rs."addr".g:re.")) == -1) {\<cr>    perror(\"bind failed\");\<cr>    exit(EXIT_FAILURE);\<cr>}"
" Multithreading. Thread Creation:
let g:template['c']['pthread_create'] = "pthread_t ".g:rs."thread".g:re.";\<cr>if (pthread_create(&".g:rs."thread".g:re.", NULL, ".g:rs."function".g:re.", ".g:rs."arg".g:re.") != 0) {\<cr>    perror(\"pthread_create failed\");\<cr>    exit(EXIT_FAILURE);\<cr>}"
" Multithreading. Mutex Lock/Unlock:
let g:template['c']['mutex_lock'] = "pthread_mutex_lock(&".g:rs."mutex".g:re.");"
let g:template['c']['mutex_unlock'] = "pthread_mutex_unlock(&".g:rs."mutex".g:re.");"
" Data Structures. Linked List Node:
let g:template['c']['llnode'] = "struct Node {\<cr>int data;\<cr>struct Node *next;\<cr>};"
" Data Structures. Queue:
let g:template['c']['queue'] = "struct Queue {\<cr>struct Node *front, *rear;\<cr>};"
" Bitwise Operations:
let g:template['c']['bitwise_and'] = "int result = ".g:rs."var1".g:re." & ".g:rs."var2".g:re.";"
let g:template['c']['bitwise_or'] = "int result = ".g:rs."var1".g:re." | ".g:rs."var2".g:re.";"
let g:template['c']['bitwise_xor'] = "int result = ".g:rs."var1".g:re." ^ ".g:rs."var2".g:re.";"
" Command Line Arguments:
let g:template['c']['main_args'] = "int main(int argc, char *argv[]) {\<cr>if (argc != ".g:rs."expected_args".g:re.") {\<cr>fprintf(stderr, \"Usage: %s\", argv[0]);\<cr>return 1;\<cr>}\<cr>}"
" typedef struct Snippet:
let g:template['c']['typedef_struct'] = "typedef struct {\<cr>}.\<cr>}. ".g:rs."TypeName".g:re.";"
" typedef enum Snippet:
let g:template['c']['typedef_enum'] = "typedef enum {\<cr>}.\<cr>}. ".g:rs."TypeName".g:re.";"
" typedef union Snippet:
let g:template['c']['typedef_union'] = "typedef union {\<cr>}.\<cr>}. ".g:rs."TypeName".g:re.";"
" Platform Detection. Linux:
let g:template['c']['platform_linux'] = "#if defined(__linux__)\<cr>// Linux-specific code\<cr>#endif"
" Platform Detection. Mac:
let g:template['c']['platform_mac'] = "#if defined(__APPLE__)\<cr>// Mac-specific code\<cr>#endif"
" Platform Detection. Windows:
let g:template['c']['platform_windows'] = "#if defined(_WIN32)\<cr>// Windows-specific code\<cr>#endif"
" Architecture Detection. Intel (x86):
let g:template['c']['arch_intel'] = "#if defined(__x86_64__)\<cr>// x86_64-specific code\<cr>#endif"
" Architecture Detection. ARM:
let g:template['c']['arch_arm'] = "#if defined(__arm__)\<cr>// ARM-specific code\<cr>#endif"
" Architecture Detection. RISC-V:
let g:template['c']['arch_riscv'] = "#if defined(__riscv)\<cr>// RISC-V-specific code\<cr>#endif"
" Bit Detection. 32-bit:
let g:template['c']['bit_32_or_win32'] = "#if defined(__i386__) || defined(_WIN32)\<cr>// 32-bit specific code\<cr>#endif"
" Bit Detection. 64-bit:
let g:template['c']['bit_64_or_win64'] = "#if defined(__x86_64__) || defined(_WIN64)\<cr>// 64-bit specific code\<cr>#endif"
" 64-bit Linux:
let g:template['c']['platform_linux_64'] = "#if defined(__linux__) && defined(__x86_64__)\<cr>// 64-bit Linux-specific code\<cr>#endif"
" 32-bit Windows:
let g:template['c']['platform_windows_32'] = "#if defined(_WIN32) && !defined(_WIN64)\<cr>// 32-bit Windows-specific code\<cr>#endif"
" 32-bit and Windows:
let g:template['c']['bit_32_and_win32'] = "#if defined(__i386__) && defined(_WIN32)\<cr>// 32-bit specific code\<cr>#endif"
" Compiler Detection Macros. GCC (GNU Compiler Collection):
let g:template['c']['compiler_gcc'] = "#if defined(__GNUC__)\<cr>// GCC-specific code\<cr>#endif"
" Compiler Detection Macros. MSVC (Microsoft Visual C++):
let g:template['c']['compiler_msvc'] = "#if defined(_MSC_VER)\<cr>// MSVC-specific code\<cr>#endif"
" Compiler Detection Macros. Clang:
let g:template['c']['compiler_clang'] = "#if defined(__clang__)\<cr>// Clang-specific code\<cr>#endif"
" Compiler Detection Macros. Intel Compiler:
let g:template['c']['compiler_intel'] = "#if defined(__INTEL_COMPILER)\<cr>// Intel Compiler-specific code\<cr>#endif"
" Compiler Detection Macros. Sun Studio (Oracle Solaris Studio):
let g:template['c']['compiler_sun'] = "#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)\<cr>// Sun Studio-specific code\<cr>#endif"
" Compiler Detection Macros. 64-bit Linux with GCC:
let g:template['c']['platform_linux_64_gcc'] = "#if defined(__linux__) && defined(__x86_64__) && defined(__GNUC__)\<cr>// 64-bit Linux with GCC-specific code\<cr>#endif"
" Compiler Detection Macros. 32-bit Windows with MSVC:
let g:template['c']['platform_windows_32_msvc'] = "#if defined(_WIN32) && !defined(_WIN64) && defined(_MSC_VER)\<cr>// 32-bit Windows with MSVC-specific code\<cr>#endif"
"
" Code Snippets - C
"
let g:template['c']['opt'] = "int ".g:rs."option".g:re."; /* Options */\<cr>\<Space>\<BS>\<cr>
                                 \\<cr>\<BS>\<BS>\<BS>printf(\"Please choose an option:\\n\");
                                 \\<cr>printf(\"1. Option One\\n\");
                                 \\<cr>printf(\"2. Option Two\\n\");
                                 \\<cr>scanf(\"\%d\"\, &".g:rs."option".g:re.");
                                 \\<cr>
                                 \\<cr>if (".g:rs."option".g:re." == 1) {
                                 \\<cr>// Code to execute if Option One was chosen
                                 \\<cr>\<BS>\<BS>\<BS>printf(\"You chose Option One.\\n\");
                                 \\<cr>} else if (".g:rs."option".g:re." == 2) {
                                 \\<cr>// Code to execute if Option Two was chosen
                                 \\<cr>\<BS>\<BS>\<BS>printf(\"You chose Option Two.\\n\");
                                 \\<cr>} else {
                                 \\<cr>// Code to execute if an invalid option was chosen
                                 \\<cr>\<BS>\<BS>\<BS>printf(\"Invalid option chosen.\\n\");
                                 \\<cr>}"

let g:template['c']['menu'] = "int ".g:rs."option".g:re."; /* Options */\<cr>\<Space>\<BS>\<cr>
                                 \\<cr>\<BS>\<BS>\<BS>printf(\"Please choose an option:\\n\");
                                 \\<cr>printf(\"1. Option One\\n\");
                                 \\<cr>printf(\"2. Option Two\\n\");
                                 \\<cr>printf(\"\\nUp to...\\n\");
                                 \\<cr>printf(\"\\n10. Option Ten\\n\");
                                 \\<cr>scanf(\"\%d\"\, &".g:rs."option".g:re.");
                                 \\<cr>
                                 \\<cr>switch (".g:rs."option".g:re.") {
                                 \\<cr>\<BS>  case 1:
                                 \\<cr> // Code to execute if Option One was chosen
                                 \\<cr>\<BS>\<BS>\<BS>\<BS>printf(\"You chose Option One.\\n\");
                                 \\<cr>break;
                                 \\<cr>case 2:
                                 \\<cr> // Code to execute if Option Two was chosen
                                 \\<cr>\<BS>\<BS>\<BS>\<BS>printf(\"You chose Option Two.\\n\");
                                 \\<cr>break;
                                 \\<cr>
                                 \\<cr>   // ...
                                 \\<cr>\<BS>\<BS>\<BS>
                                 \\<cr>
                                 \\<cr>\<BS>\<BS>\<BS>    case 10:
                                 \\<cr>\<BS>\<BS>\<BS>       // Code to execute if Option Ten was chosen
                                 \\<cr>\<BS>\<BS>\<BS>\<BS>printf(\"You chose Option Ten.\\n\");
                                 \\<cr>break;
                                 \\<cr>default:
                                 \\<cr> // Code to execute if an invalid option was chosen
                                 \\<cr>\<BS>\<BS>\<BS>\<BS>printf(\"Invalid option chosen.\\n\");
                                 \\<cr>break;
                                 \\<cr>}
                                 \\<cr>"

let g:template['c']['mainarg'] = "/* A rudimentary main() function template in C. */
                               \\<cr>#include <stdio.h> /* includes standard input/output library */
                               \\<cr>#include <stdlib.h> /* includes standard library for C, which provides functions for memory allocation */
                               \\<cr>#include <string.h> /* includes standard library for C, which provides functions for string manipulation */
                               \\<cr>#define VERSION \"".g:rs."version_num".g:re."\" /* defines a constant string called \"VERSION\" with the value ".g:rs."version_num".g:re." */
                               \\<cr>#define NO_OF_ARGS ".g:rs."3".g:re." /* the exact no. of command-line arguments the program takes */
                               \\<cr>int main(int argc, char *argv[]) { /* The Main function. argc means the number of arguments. argv[0] is the program name. argv[1] is the first argument. argv[2] is the second argument and so on. */
                               \\<cr>printf(\"Hey! \"\%s\"\ here!\\n\", argv[0]); /* Displays the program's name */
                               \\<cr>if(argc == 2 && strcmp(argv[1], \"--version\") == 0) { /* checks if the program was called with the argument \"--version\". If it was, it prints out the value of the \"VERSION\" constant and returns 0. Otherwise, it slides down to the next `else if()` block. */
                               \\<cr>printf(\"\%s\\n\"\, VERSION);
                               \\<cr>return 0;
                               \\<cr>}
                               \\<cr>
                               \\<cr>else if(argc == 2 && strcmp(argv[1], \"--version\") != 0) { /* checks if the program was called with only ONE argument and the argument was not \"--version\". In that case, it displays an error message and exits with a return value of 1. */
                               \\<cr>printf(\"Unknown argument! The program will exit!!\\n\");
                               \\<cr>exit(1);
                               \\<cr>}
                               \\<cr>
                               \\<cr>if(argc != NO_OF_ARGS) {  /* Checks if the program was called with the exact number of arguments. If it wasn't, it prints out an error message and returns 1. */
                               \\<cr>printf(\"Please provide \%d arguments\\n\", (NO_OF_ARGS - 1));
                               \\<cr>return 1;
                               \\<cr>}
                               \\<cr>
                               \\<cr>/* After performing all checks, your code starts here */
                               \\<cr>
                               \\<cr>printf(\"First argument: \%s\\n\", argv[1]); /* prints out the first argument passed to the program */
                               \\<cr>printf(\"Second argument: \%s\\n\", argv[2]); /* prints out the second argument passed to the program */
                               \\<cr>return 0;
                               \\<cr>}\<cr>"

" ---------------------------------------------
" C++ templates
let g:template['cpp'] = g:template['c']
"
" Additional C++ templates
"
let g:template['cpp']['usi'] = "using namespace ".g:rs."std".g:re.";"
let g:template['cpp']['in2'] = "#include <cerrno>\<cr>#include <iostream>\<cr>#include <vector>\<cr>#include <ios>\<cr>#include <ostream>\<cr>#include <string>\<cr>#include <cmath>\<cr>#include <cstdio>\<cr>#include <cstdlib>\<cr>#include <".g:rs."...".g:re.">\<cr>#include <".g:rs."...".g:re.">\<cr>#include \"".g:rs."...".g:re."\"\<cr>#include \"".g:rs."...".g:re."\"\<cr>"
let g:template['cpp']['cout'] = "std::cout << ".g:rs."...".g:re." << std::endl;"
let g:template['cpp']['cin1'] = "std::cin >> ".g:rs."...".g:re.";"
let g:template['cpp']['cin2'] = "std::cin.".g:rs."...".g:re.";"
" Class Definition:
let g:template['cpp']['class'] = "class ".g:rs."ClassName".g:re." {\<cr>public:\<cr>}."
" Namespace:
let g:template['cpp']['namespace'] = "namespace ".g:rs."NamespaceName".g:re." {\<cr>}."
" Include Guards:
let g:template['cpp']['includeguard'] = "#ifndef ".g:rs."HEADER_NAME".g:re."\<cr>#define ".g:rs."HEADER_NAME".g:re."\<cr>\<cr>#endif  // ".g:rs."HEADER_NAME".g:re.""
" Lambda Expression:
let g:template['cpp']['lambda'] = "[=](".g:rs."args".g:re.") {\<cr>}."
" Try-Catch Block (C++):
let g:template['cpp']['trycatch'] = "try {\<cr>}.\<cr>catch (".g:rs."ExceptionType".g:re." &e) {\<cr>}"
" Function Overloading:
let g:template['cpp']['overload'] = "void ".g:rs."FunctionName".g:re."(".g:rs."Type1 param1".g:re.", ".g:rs."Type2 param2".g:re.") {\<cr>}."
" Template Function:
let g:template['cpp']['templatefunc'] = "template <typename T> void ".g:rs."FunctionName".g:re."(T param) {\<cr>}."
" Constructor:
let g:template['cpp']['constructor'] = "ClassName(".g:rs."param1".g:re.", ".g:rs."param2".g:re.") {\<cr>}."
" Destructor:
let g:template['cpp']['destructor'] = "~ClassName() {\<cr>}."
" Operator Overloading:
let g:template['cpp']['operator'] = "ClassName& operator+".g:rs."op".g:re."(const ClassName& rhs) {\<cr>return *this;\<cr>}"
" ---------------------------------------------
" common templates
let g:template['_'] = {}
let g:template['_']['xt'] = "\<c-r>=strftime(\"%Y-%m-%d %H:%M:%S\")\<cr>"

" ---------------------------------------------
" load user defined snippets
exec "silent! runtime plugin/my_snippets.vim"
if type(g:user_defined_snippets) == type("")
  exec "silent! runtime ".g:user_defined_snippets
  exec "silent! source ".g:user_defined_snippets
elseif type(g:user_defined_snippets) == type([])
  for snippet in g:user_defined_snippets
    exec "silent! runtime ".snippet
    exec "silent! source ".snippet
  endfor
endif

" vim: set fdm=marker et :
