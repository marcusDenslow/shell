#include <windows.h>
#include <process.h>  // For _spawn functions
#include <direct.h>   // For _chdir and _getcwd
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_dir(char **args);
int lsh_clear(char **args);

char *builtin_str[] = {
  "cd",
  "help",
  "exit",
  "ls",
  "dir",
  "clear",
  "cls",
};

int (*builtin_func[]) (char **) = {
  &lsh_cd,
  &lsh_help,
  &lsh_exit,
  &lsh_dir,
  &lsh_dir,
  &lsh_clear,
  &lsh_clear,
};

int lsh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
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
  } while (FindNextFile(hFind, &findData));
  
  // Close find handle
  FindClose(hFind);
  printf("\n");
  
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

char *lsh_read_line(void) {
  int bufsize = LSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    c = getchar();

    if (c == EOF || c == '\n') {
      buffer[position] = '\0';  // Fix: Terminate with '\0' instead of '\n'
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    if (position >= bufsize) {
      bufsize += LSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
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
  char username[256] = "user"; // Default value if username can't be retrieved
  DWORD username_len = sizeof(username);
  
  // Get the Windows username once (doesn't change during execution)
  if (!GetUserName(username, &username_len)) {
    perror("lsh: failed to get username");
  }
  
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

