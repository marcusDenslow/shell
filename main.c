#include <fileapi.h>
#include <minwinbase.h>
#include <timezoneapi.h>
#include <windows.h>
#include <process.h>  // For _spawn functions
#include <direct.h>   // For *chdir and *getcwd
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>  // For _getch
#include <ctype.h>  // For isprint
#include <winerror.h>
#include <winnt.h>

int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_dir(char **args);
int lsh_clear(char **args);
int lsh_mkdir(char **args);
int lsh_rmdir(char **args);
int lsh_del(char **args);
int lsh_touch(char **args);
int lsh_pwd(char **args);
int lsh_cat(char **args);

#define KEY_TAB 9
#define KEY_BACKSPACE 8
#define KEY_ENTER 13
#define KEY_ESC 27

char *builtin_str[] = {
  "cd",
  "help",
  "exit",
  "ls",
  "dir",
  "clear",
  "cls",
  "mkdir",
  "rmdir",
  "del",
  "rm",
  "touch",
  "pwd",
  "cat",
};

int (*builtin_func[]) (char **) = {
  &lsh_cd,
  &lsh_help,
  &lsh_exit,
  &lsh_dir,
  &lsh_dir,
  &lsh_clear,
  &lsh_clear,
  &lsh_mkdir,
  &lsh_rmdir,
  &lsh_del,
  &lsh_del,
  &lsh_touch,
  &lsh_pwd,
  &lsh_cat,
};

int lsh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}


int lsh_pwd(char **args){
  char cwd[1024];

  if (_getcwd(cwd, sizeof(cwd)) == NULL){
    perror("lsh: pwd");
    return 1;
  }

  printf("\n%s\n\n", cwd);
  return 1;
}


int lsh_cat(char **args) {
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected file argument to \"cat\"\n");
    return 1;
  }
  
  // Process each file argument
  int i = 1;
  int success = 1;
  
  while (args[i] != NULL) {
    // Print filename and blank line before content
    printf("\n--- %s ---\n\n", args[i]);
    
    // Open the file in binary mode to avoid automatic CRLF conversion
    FILE *file = fopen(args[i], "rb");
    
    if (file == NULL) {
      fprintf(stderr, "lsh: cannot open '%s': ", args[i]);
      perror("");
      success = 0;
      i++;
      continue;
    }
    
    // Read and print file contents
    char buffer[4096]; // Larger buffer for efficiency
    size_t bytes_read;
    
    // Use fread instead of fgets to avoid line-based processing
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
      fwrite(buffer, 1, bytes_read, stdout);
    }
    
    // Check for read errors
    if (ferror(file)) {
      fprintf(stderr, "lsh: error reading from '%s': ", args[i]);
      perror("");
      success = 0;
    }
    
    // Close the file
    fclose(file);
    
    // Print blank line after content
    printf("\n\n");
    
    i++;
  }
  
  return success;
}


int lsh_del(char **args) {
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected file argument to \"del\"\n");
    return 1;
  }
  
  // Handle multiple files
  int i = 1;
  int success = 1;
  
  while (args[i] != NULL) {
    // Try deleting each file
    if (DeleteFile(args[i]) == 0) {
      // DeleteFile returns 0 on failure, non-zero on success
      DWORD error = GetLastError();
      fprintf(stderr, "lsh: failed to delete '%s': ", args[i]);
      
      switch (error) {
        case ERROR_FILE_NOT_FOUND:
          fprintf(stderr, "file not found\n");
          break;
        case ERROR_ACCESS_DENIED:
          fprintf(stderr, "access denied\n");
          break;
        default:
          fprintf(stderr, "error code %lu\n", error);
          break;
      }
      
      success = 0;
    } else {
      printf("Deleted '%s'\n", args[i]);
    }
    
    i++;
  }
  
  return success;
}





int lsh_mkdir(char **args){
  if (args[1] == NULL){
    fprintf(stderr, "lsh: expected argument to \"mkdir\"\n");
    return 1;
  }

  if (_mkdir(args[1]) != 0){
    perror("lsh: mkdir");
  }

  return 1;
}


int lsh_rmdir(char **args){
  if (args[1] == NULL){
    fprintf(stderr, "lsh: expected argument to \"rmdir\"\n");
    return 1;
  }


  if (_rmdir(args[1]) != 0){
    perror("lsh: rmdir");
  }

  return 1;
}


int lsh_touch(char **args) {
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected file argument to \"touch\"\n");
    return 1;
  }
  
  // Handle multiple files
  int i = 1;
  int success = 1;
  
  while (args[i] != NULL) {
    // Check if file exists
    HANDLE hFile = CreateFile(
      args[i],                       // filename
      FILE_WRITE_ATTRIBUTES,         // access mode
      FILE_SHARE_READ | FILE_SHARE_WRITE, // share mode
      NULL,                          // security attributes
      OPEN_ALWAYS,                   // create if doesn't exist, open if does
      FILE_ATTRIBUTE_NORMAL,         // file attributes
      NULL                           // template file
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
      // Failed to create/open file
      DWORD error = GetLastError();
      fprintf(stderr, "lsh: failed to touch '%s': error code %lu\n", args[i], error);
      success = 0;
    } else {
      // File was created or opened successfully
      // Get current system time
      SYSTEMTIME st;
      FILETIME ft;
      GetSystemTime(&st);
      SystemTimeToFileTime(&st, &ft);
      
      // Update file times
      if (!SetFileTime(hFile, &ft, &ft, &ft)) {
        fprintf(stderr, "lsh: failed to update timestamps for '%s'\n", args[i]);
        success = 0;
      }
      
      // Close file handle
      CloseHandle(hFile);
      
      printf("Created/updated '%s'\n", args[i]);
    }
    
    i++;
  }
  
  return success;
}


int lsh_cd(char **args) {
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  } else {
    if (_chdir(args[1]) != 0) {  // Use _chdir for Windows
      perror("lsh");
    }
  }
  return 1;
}

char **find_matches(const char *partial_path, int *num_matches) {
    char cwd[1024];
    char search_dir[1024] = "";
    char search_pattern[256] = "";
    char **matches = NULL;
    int matches_capacity = 10;
    *num_matches = 0;
    
    // Allocate initial array for matches
    matches = (char**)malloc(sizeof(char*) * matches_capacity);
    if (!matches) {
        fprintf(stderr, "lsh: allocation error in tab completion\n");
        return NULL;
    }
    
    // Parse the partial path to separate directory and pattern
    char *last_slash = strrchr(partial_path, '\\');
    if (last_slash) {
        // There's a directory part
        int dir_len = last_slash - partial_path + 1;
        strncpy(search_dir, partial_path, dir_len);
        search_dir[dir_len] = '\0';
        strcpy(search_pattern, last_slash + 1);
    } else {
        // No directory specified, use current directory
        _getcwd(cwd, sizeof(cwd));
        strcpy(search_dir, cwd);
        strcat(search_dir, "\\");
        strcpy(search_pattern, partial_path);
    }
    
    // Prepare for file searching
    char search_path[1024];
    strcpy(search_path, search_dir);
    strcat(search_path, "*");
    
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(search_path, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        free(matches);
        return NULL;
    }
    
    // Find all matching files/directories
    do {
        // Skip . and .. directories
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }
        
        // Check if file matches our pattern
        if (strncmp(findData.cFileName, search_pattern, strlen(search_pattern)) == 0) {
            // Add to matches
            if (*num_matches >= matches_capacity) {
                matches_capacity *= 2;
                matches = (char**)realloc(matches, sizeof(char*) * matches_capacity);
                if (!matches) {
                    fprintf(stderr, "lsh: allocation error in tab completion\n");
                    FindClose(hFind);
                    return NULL;
                }
            }
            
            matches[*num_matches] = _strdup(findData.cFileName);
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // Append a backslash to directory names
                size_t len = strlen(matches[*num_matches]);
                matches[*num_matches] = (char*)realloc(matches[*num_matches], len + 2);
                strcat(matches[*num_matches], "\\");
            }
            (*num_matches)++;
        }
    } while (FindNextFile(hFind, &findData));
    
    FindClose(hFind);
    
    return matches;
}

// New function to find the best match for current input
char* find_best_match(const char* partial_text) {
    // Start from the beginning of the current word
    int len = strlen(partial_text);
    if (len == 0) return NULL;
    
    // Find the start of the current word
    int word_start = len - 1;
    while (word_start >= 0 && partial_text[word_start] != ' ' && partial_text[word_start] != '\\') {
        word_start--;
    }
    word_start++; // Move past the space or backslash
    
    // Extract the current word
    char partial_path[1024] = "";
    strncpy(partial_path, partial_text + word_start, len - word_start);
    partial_path[len - word_start] = '\0';
    
    // Skip if we're not typing a path
    if (strlen(partial_path) == 0) return NULL;
    
    // Find matches
    int num_matches;
    char **matches = find_matches(partial_path, &num_matches);
    
    if (matches && num_matches > 0) {
        // Create the full suggestion by combining the prefix with the matched path
        char* full_suggestion = (char*)malloc(len + strlen(matches[0]) - strlen(partial_path) + 1);
        if (!full_suggestion) {
            for (int i = 0; i < num_matches; i++) {
                free(matches[i]);
            }
            free(matches);
            return NULL;
        }
        
        // Copy the prefix (everything before the current word)
        strncpy(full_suggestion, partial_text, word_start);
        full_suggestion[word_start] = '\0';
        
        // Append the matched path
        strcat(full_suggestion, matches[0]);
        
        // Free matches array
        for (int i = 0; i < num_matches; i++) {
            free(matches[i]);
        }
        free(matches);
        
        return full_suggestion;
    }
    
    return NULL;
}

int lsh_clear(char **args) {
    // Get the handle to the console
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    
    if (hConsole == INVALID_HANDLE_VALUE) {
        perror("lsh: failed to get console handle");
        return 1;
    }
    
    // Get console information
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        perror("lsh: failed to get console info");
        return 1;
    }
    
    // Calculate total cells in console
    DWORD cellCount = csbi.dwSize.X * csbi.dwSize.Y;
    
    // Fill the entire buffer with spaces
    DWORD count;
    COORD homeCoords = {0, 0};
    
    if (!FillConsoleOutputCharacter(hConsole, ' ', cellCount, homeCoords, &count)) {
        perror("lsh: failed to fill console");
        return 1;
    }
    
    // Fill the entire buffer with the current attributes
    if (!FillConsoleOutputAttribute(hConsole, csbi.wAttributes, cellCount, homeCoords, &count)) {
        perror("lsh: failed to set console attributes");
        return 1;
    }
    
    // Move the cursor to home position
    SetConsoleCursorPosition(hConsole, homeCoords);
    
    return 1;
}

int lsh_dir(char **args) {
  char cwd[1024];
  WIN32_FIND_DATA findData;
  HANDLE hFind;
  
  // Get current directory
  if (_getcwd(cwd, sizeof(cwd)) == NULL) {
    perror("lsh");
    return 1;
  }
  
  // Print current directory
  // printf("Directory of %s\n\n", cwd);
  
  // Prepare search pattern for all files
  char searchPath[1024];
  strcpy(searchPath, cwd);
  strcat(searchPath, "\\*");
  
  // Find first file
  hFind = FindFirstFile(searchPath, &findData);
  
  if (hFind == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "lsh: Failed to list directory contents\n");
    return 1;
  }
  
  // List all files
  do {
    // Skip . and .. directories for cleaner output
    if (strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {
      // Check if it's a directory
      if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        printf("<DIR>\t%s\n", findData.cFileName);
      } else {
        // Print file name and size
        printf("%u\t%s\n", findData.nFileSizeLow, findData.cFileName);
      }
    }
    printf("\n");
  } while (FindNextFile(hFind, &findData));
  
  // Close find handle
  FindClose(hFind);
  
  return 1;
}

int lsh_help(char **args) {
  int i;
  printf("Marcus Denslow's LSH\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");
  for (i = 0; i < lsh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }
  printf("Use the command for information on other programs.\n");
  return 1;
}

int lsh_exit(char **args) {
  return 0;
}

int lsh_launch(char **args) {
    // Construct command line string for CreateProcess
    char command[1024] = "";
    for (int i = 0; args[i] != NULL; i++) {
        strcat(command, args[i]);
        strcat(command, " ");
    }
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    // Create a new process
    if (!CreateProcess(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "lsh: failed to execute %s\n", args[0]);
        return 1;
    }
    // Wait for the process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 1;
}

int lsh_execute(char **args) {
  int i;
  if (args[0] == NULL) {
    return 1;
  }
  for (i = 0; i < lsh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }
  return lsh_launch(args);
}

#define LSH_RL_BUFSIZE 1024

// Modified read_line function with improved tab cycling and enter acceptance
char *lsh_read_line(void) {
    int bufsize = LSH_RL_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;
    char *suggestion = NULL;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    WORD originalAttributes;
    
    // For tab completion cycling
    static int tab_index = 0;
    static char **tab_matches = NULL;
    static int tab_num_matches = 0;
    static char last_tab_prefix[1024] = "";
    static int tab_word_start = 0;  // Track where the word being completed starts
    
    // Flag to track if we're showing a suggestion
    int showing_suggestion = 0;
    
    // Variables to track the prompt and original line
    static char original_line[LSH_RL_BUFSIZE];
    COORD promptEndPos;
    
    // Get original console attributes
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    originalAttributes = consoleInfo.wAttributes;
    
    // Save prompt end position for reference
    promptEndPos = consoleInfo.dwCursorPosition;
    
    if (!buffer) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }
    
    buffer[0] = '\0';  // Initialize empty string
    
    while (1) {
        // Clear any previous suggestion from screen if not in tab cycling
        if (suggestion && !tab_matches) {
            // Get current cursor position
            GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
            // Calculate suggestion length
            int suggestionLen = strlen(suggestion) - strlen(buffer);
            if (suggestionLen > 0) {
                // Set cursor to current position
                SetConsoleCursorPosition(hConsole, consoleInfo.dwCursorPosition);
                // Clear the suggestion by printing spaces
                for (int i = 0; i < suggestionLen; i++) {
                    putchar(' ');
                }
                // Reset cursor position
                SetConsoleCursorPosition(hConsole, consoleInfo.dwCursorPosition);
            }
            free(suggestion);
            suggestion = NULL;
            showing_suggestion = 0;
        }
        
        // Find and display new suggestion only if we're not in tab cycling mode
        if (!tab_matches) {
            buffer[position] = '\0';  // Ensure buffer is null-terminated
            suggestion = find_best_match(buffer);
            if (suggestion) {
                // Get current cursor position
                GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
                // Calculate the start of the current word
                int word_start = position - 1;
                while (word_start >= 0 && buffer[word_start] != ' ' && buffer[word_start] != '\\') {
                    word_start--;
                }
                word_start++; // Move past the space or backslash
                
                // Extract just the last word from the suggested path
                char *lastWord = strrchr(suggestion, ' ');
                if (lastWord) {
                    lastWord++; // Move past the space
                } else {
                    lastWord = suggestion;
                }
                
                // Extract just the last word from what we've typed so far
                char currentWord[1024] = "";
                strncpy(currentWord, buffer + word_start, position - word_start);
                currentWord[position - word_start] = '\0';
                
                // Only display the suggestion if it starts with what we're typing
                if (strncmp(lastWord, currentWord, strlen(currentWord)) == 0) {
                    // Set text color to gray for suggestion
                    SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY);
                    // Print only the part of the suggestion that hasn't been typed yet
                    printf("%s", lastWord + strlen(currentWord));
                    // Reset color
                    SetConsoleTextAttribute(hConsole, originalAttributes);
                    // Reset cursor position
                    SetConsoleCursorPosition(hConsole, consoleInfo.dwCursorPosition);
                    showing_suggestion = 1;
                }
            }
        }
        
        c = _getch();  // Get character without echo
        
        if (c == KEY_ENTER) {
            // If we're in tab cycling mode, accept the current suggestion
            if (tab_matches) {
                // Tab cycling is active - accept the current suggestion but don't execute
                // Find the start of the current word
                int word_start = tab_word_start;
                
                // Insert the selected match into buffer
                char *current_match = tab_matches[tab_index];
                
                // Replace partial path with current match
                strcpy(buffer + word_start, current_match);
                position = word_start + strlen(current_match);
                
                // Clean up tab completion resources
                for (int i = 0; i < tab_num_matches; i++) {
                    free(tab_matches[i]);
                }
                free(tab_matches);
                tab_matches = NULL;
                tab_num_matches = 0;
                tab_index = 0;
                last_tab_prefix[0] = '\0';
                
                // Clear the current line and redraw it
                GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
                
                // Move to start of line and clear everything
                COORD lineStartPos = promptEndPos;
                SetConsoleCursorPosition(hConsole, lineStartPos);
                
                // Print the full command with the accepted match
                buffer[position] = '\0';
                printf("%s", buffer);
                
                // Continue editing - don't submit yet
                continue;
            }
            // Otherwise, if we have a suggestion showing, accept it
            else if (showing_suggestion && suggestion) {
                // Find the start of the current word
                int word_start = position - 1;
                while (word_start >= 0 && buffer[word_start] != ' ' && buffer[word_start] != '\\') {
                    word_start--;
                }
                word_start++; // Move past the space or backslash
                
                // Extract just the last word from the suggested path
                char *lastWord = strrchr(suggestion, ' ');
                if (lastWord) {
                    lastWord++; // Move past the space
                } else {
                    lastWord = suggestion;
                }
                
                // Extract just the last word from what we've typed so far
                char currentWord[1024] = "";
                strncpy(currentWord, buffer + word_start, position - word_start);
                currentWord[position - word_start] = '\0';
                
                // Get current cursor position
                GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
                
                // Print the remainder of the suggestion in normal color
                printf("%s", lastWord + strlen(currentWord));
                
                // Update buffer with the suggestion
                // Keep the prefix (everything before the current word)
                char tempBuffer[1024] = "";
                strncpy(tempBuffer, buffer, word_start);
                tempBuffer[word_start] = '\0';
                
                // Add the completed word
                strcat(tempBuffer, lastWord);
                
                // Copy back to buffer
                strcpy(buffer, tempBuffer);
                position = strlen(buffer);
                
                // Continue editing - don't submit yet
                free(suggestion);
                suggestion = NULL;
                showing_suggestion = 0;
                continue;
            }
            // No tab cycling or suggestion - submit the command
            else {
                putchar('\n');  // Echo newline
                buffer[position] = '\0';
                
                // Clean up
                if (suggestion) free(suggestion);
                
                // Clean up tab completion resources
                if (tab_matches) {
                    for (int i = 0; i < tab_num_matches; i++) {
                        free(tab_matches[i]);
                    }
                    free(tab_matches);
                    tab_matches = NULL;
                    tab_num_matches = 0;
                    tab_index = 0;
                    last_tab_prefix[0] = '\0';
                }
                
                return buffer;
            }
        } else if (c == KEY_BACKSPACE) {
            // User pressed Backspace
            if (position > 0) {
                position--;
                // Move cursor back, print space, move cursor back again
                printf("\b \b");
                buffer[position] = '\0';
                
                // Reset tab completion state when editing
                if (tab_matches) {
                    for (int i = 0; i < tab_num_matches; i++) {
                        free(tab_matches[i]);
                    }
                    free(tab_matches);
                    tab_matches = NULL;
                    tab_num_matches = 0;
                    tab_index = 0;
                    last_tab_prefix[0] = '\0';
                }
            }
        } else if (c == KEY_TAB) {
            // User pressed Tab - activate completion
            buffer[position] = '\0';  // Ensure null termination
            
            // Find the start of the current word
            int word_start = position;
            while (word_start > 0 && buffer[word_start - 1] != ' ' && buffer[word_start - 1] != '\\') {
                word_start--;
            }
            
            // Save word_start for later use
            tab_word_start = word_start;
            
            // Save original command line up to the word being completed
            strncpy(original_line, buffer, word_start);
            original_line[word_start] = '\0';
            
            char partial_path[1024] = "";
            strncpy(partial_path, buffer + word_start, position - word_start);
            partial_path[position - word_start] = '\0';
            
            // Check if we're continuing to tab through the same prefix
            if (strcmp(partial_path, last_tab_prefix) != 0 || tab_matches == NULL) {
                // New prefix or first tab press, find all matches
                // Clean up previous matches if any
                if (tab_matches) {
                    for (int i = 0; i < tab_num_matches; i++) {
                        free(tab_matches[i]);
                    }
                    free(tab_matches);
                }
                
                // Store the current prefix
                strcpy(last_tab_prefix, partial_path);
                
                // Reset tab index
                tab_index = 0;
                
                // Find matches for the new prefix
                tab_matches = find_matches(partial_path, &tab_num_matches);
                
                // If no matches, don't do anything
                if (!tab_matches || tab_num_matches == 0) {
                    if (tab_matches) {
                        free(tab_matches);
                        tab_matches = NULL;
                    }
                    tab_num_matches = 0;
                    last_tab_prefix[0] = '\0';
                    continue;
                }
            } else {
                // Same prefix, cycle to next match
                tab_index = (tab_index + 1) % tab_num_matches;
            }
            
            if (tab_matches && tab_num_matches > 0) {
                // Get cursor position
                GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
                
                // Move cursor to beginning of the line (just after prompt)
                SetConsoleCursorPosition(hConsole, promptEndPos);
                
                // Clear the entire line by printing spaces
                for (int i = 0; i < 80; i++) { // Assumes terminal is at least 80 columns wide
                    putchar(' ');
                }
                
                // Move cursor back to beginning of line
                SetConsoleCursorPosition(hConsole, promptEndPos);
                
                // Redraw the entire line from scratch with new suggestion
                // Print the prefix part that the user typed
                printf("%s", original_line);
                
                // Print the current match
                printf("%s", tab_matches[tab_index]);
                
                // Display cycling indicator
                if (tab_num_matches > 1) {
                    // Get current cursor position after printing
                    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
                    COORD afterMatchPos = consoleInfo.dwCursorPosition;
                    
                    // Set text color to gray for the cycle indicator
                    SetConsoleTextAttribute(hConsole, FOREGROUND_INTENSITY);
                    
                    // Display cycling indicator
                    printf(" (%d/%d)", tab_index + 1, tab_num_matches);
                    
                    // Short delay to show the indicator
                    Sleep(250);
                    
                    // Calculate spaces to erase the indicator
                    char tempBuf[20];
                    sprintf(tempBuf, " (%d/%d)", tab_index + 1, tab_num_matches);
                    int indicatorLen = strlen(tempBuf);
                    
                    // Erase the indicator
                    for (int i = 0; i < indicatorLen; i++) {
                        putchar(' ');
                    }
                    
                    // Reset color and position
                    SetConsoleTextAttribute(hConsole, originalAttributes);
                    SetConsoleCursorPosition(hConsole, afterMatchPos);
                }
                
                // Important: buffer remains unchanged until user accepts with Enter
                // This ensures proper cycling without concatenating suggestions
            }
        } else if (isprint(c)) {
            // Regular printable character
            putchar(c);  // Echo character
            buffer[position] = c;
            position++;
            
            // Reset tab completion state when editing
            if (tab_matches) {
                for (int i = 0; i < tab_num_matches; i++) {
                    free(tab_matches[i]);
                }
                free(tab_matches);
                tab_matches = NULL;
                tab_num_matches = 0;
                tab_index = 0;
                last_tab_prefix[0] = '\0';
            }
            
            // Resize buffer if needed
            if (position >= bufsize) {
                bufsize += LSH_RL_BUFSIZE;
                buffer = realloc(buffer, bufsize);
                if (!buffer) {
                    fprintf(stderr, "lsh: allocation error\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
        
        buffer[position] = '\0';  // Ensure null termination
    }
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

char **lsh_split_line(char *line) {
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;
  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }
  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;
    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
    token = strtok(NULL, LSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

void lsh_loop(void) {
  char *line;
  char **args;
  int status;
  char cwd[1024];
  char prompt_path[1024];
  char username[256] = "Elden Lord";
  /*char username[256] = "user"; // Default value if username can't be retrieved*/
  /*DWORD username_len = sizeof(username);*/
  /**/
  /*// Get the Windows username once (doesn't change during execution)*/
  /*if (!GetUserName(username, &username_len)) {*/
  /*  perror("lsh: failed to get username");*/
  /*}*/
  /**/
  do {
    // Get current directory for the prompt
    if (_getcwd(cwd, sizeof(cwd)) == NULL) {
      perror("lsh");
      strcpy(prompt_path, "unknown_path"); // Fallback in case of error
    } else {
      // Find the last directory in the path
      char *last_dir = strrchr(cwd, '\\');
      
      if (last_dir != NULL) {
        char last_dir_name[256];
        strcpy(last_dir_name, last_dir + 1); // Save the last directory name
        
        *last_dir = '\0';  // Temporarily terminate string at last backslash
        char *parent_dir = strrchr(cwd, '\\');
        
        if (parent_dir != NULL) {
          // We have at least two levels deep
          sprintf(prompt_path, "%s in %s\\%s", username, parent_dir + 1, last_dir_name);
        } else {
          // We're at top level (like C:)
          sprintf(prompt_path, "%s in %s", username, last_dir_name);
        }
      } else {
        // No backslash found (rare case)
        sprintf(prompt_path, "%s in %s", username, cwd);
      }
    }
    
    // Print prompt with username and shortened directory
    printf("%s> ", prompt_path);
    
    line = lsh_read_line();
    args = lsh_split_line(line);
    status = lsh_execute(args);
    free(line);
    free(args);
  } while (status);
}

int main(int argc, char **argv) {
  lsh_loop();
  return EXIT_SUCCESS;
}

