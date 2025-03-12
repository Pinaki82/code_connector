#!/bin/bash

# Find all executable files in the current directory and append their basenames to .gitignore
find . -type f -executable -exec basename {} \; >> .gitignore

# Optional: Remove duplicates from .gitignore (if any)
sort -u -o .gitignore .gitignore

echo "Executable filenames have been added to .gitignore."

