#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define WAIT 0
#define DONT_WAIT 1

int running = 1;
int exit_code = 0;

char cwd[256];
char command[256];
char args[10][256];
char buffer[256] __attribute__((aligned(4096)));

int flag_process;


int prepareCommArgs(int index, int argsCount)
{
  strcpy(command, args[index]);
  for (int counter = index; counter < argsCount; counter++)
  {
    strcpy(args[counter - index], args[index + (counter - index) + 1]);
  }
  return (argsCount - index - 1);
}

char* handleQuotes (char arg_ptr[256], int args_count)
{
  int char_position = 0;
  int flag = 0;
  for (int counter = 0; counter <= args_count; counter++)
  {
    if (args[counter][strlen(args[counter])-1] == '\"') flag = 1;
    if (counter == 0) memmove(arg_ptr + char_position, args[counter] + 1, strlen(args[counter]));
    else
    {
      arg_ptr[strlen(arg_ptr)] = ' ';
      memmove(arg_ptr + char_position, args[counter], strlen(args[counter]));
    }
    if (counter == 0) char_position = strlen(args[counter]);
    else char_position += (strlen(args[counter]) + 1);
    if (flag) break;
  }
  if (arg_ptr[strlen(arg_ptr)-1] == '\"') arg_ptr[strlen(arg_ptr)-1] = '\0';
  else
  {
    memmove(arg_ptr+1, arg_ptr, strlen(arg_ptr));
    arg_ptr[0] = '\"';
  }
  return arg_ptr;
}

void runProcess(char comm[256], char arg_ptr[256], int flag)
{
  flag_process = -1;
  int pid = fork();
  if (pid == 0)
  {
    char* const input_argv[] = {comm, arg_ptr, 0};
    execv(comm, input_argv);
    exit(-1);
  }
  else
  {
    int status;
    if (flag == WAIT) 
    {
      waitpid(pid, &status, 0);
      exit_code = WEXITSTATUS(status);
      flag_process = status;
    }
  }
}

void handle_command(char* buffer, int buffer_size)
{
  int c = 0;
  int argsCount = -1;
  int lastIndex = 0;

  for (c = 0; c < buffer_size && buffer[c]; c++)
  {
    if (argsCount > 10)
    {
      argsCount = 10;
      printf("Argument Count is limited to 10 (no dynamic memory allocation) all other arguments will be ignored\n");
      break;
    }
    if (buffer[c] == '\r' || buffer[c] == '\n' || buffer[c] == ' ')
    {
      if (argsCount == -1)
      {
        memcpy(command, buffer + lastIndex, c - lastIndex);
        command[c - lastIndex] = 0;
      }
      else
      {
        memcpy(args[argsCount], buffer + lastIndex, c - lastIndex);
        args[argsCount][c - lastIndex] = 0;
      }
      argsCount++;
      lastIndex = c + 1;

    }
  }

  if (strcmp(command, "exit") == 0)
  {
    c = 4;
    while (buffer[c] == ' ')
      c++;
    exit_code = atoi(&buffer[c]);
    printf("Exiting Shell with exit_code %d\n", exit_code);
    running = 0;
  }
  else if (strcmp(command, "") != 0)
  {
A:  ;

    char arg_ptr[256] = {};

    if (argsCount == 0) strcpy(arg_ptr,"./");
    else if (argsCount == 1) strcpy(arg_ptr, args[0]);

    if (args[0][0] == '"')
    {
      strcpy(arg_ptr, handleQuotes(arg_ptr, argsCount));
      for (int counter = 0; counter <= argsCount; counter++)
      {
        if (args[counter][strlen(args[counter])-1] == '\"' && strcmp(args[counter+1], ";") == 0)
        {
          runProcess(command, arg_ptr, WAIT);
          if (strcmp(args[counter + 2], "") != 0) argsCount = prepareCommArgs(counter + 2, argsCount);
          goto A;
        }
      }
    }

    if (strcmp(args[0], ";") == 0 || strcmp(args[1], ";") == 0)
    {
      runProcess(command, arg_ptr, WAIT);
      for (int counter = 0; counter <= argsCount; counter++)
      {
        if ( (args[counter][strlen(args[counter])-1] == '\"' ||
              args[counter][strlen(args[counter])-1] == '\0') &&
              strcmp(args[counter+1], ";") == 0 )
        {
          if (strcmp(args[counter + 2], "") != 0)
          {
            argsCount = prepareCommArgs(counter + 2, argsCount);
            goto A;
          }
        }
      }
      strcpy(command, args[1]);
      if (args[2][0] == '\"')
      {
        strcpy(arg_ptr, handleQuotes(arg_ptr, argsCount));
      }
      else strcpy(arg_ptr, args[2]);
    }

    if (strcmp(args[0], "&&") == 0 || strcmp(args[1], "&&") == 0)
    {
      int temp;
      if (arg_ptr[0] == '\0') strcpy(arg_ptr, "./");
      runProcess(command, arg_ptr, WAIT);
      if (flag_process == 0)
      {
        for (int counter = 0; counter <= argsCount; counter++)
        {
          if (strcmp(args[counter], "&&") == 0)
          {
            temp = counter;
            command[0] = '\0';
            if (strcmp(args[counter+1], "(") == 0) argsCount = prepareCommArgs(counter + 2, argsCount);
            else argsCount = prepareCommArgs(counter + 1, argsCount);
            goto A;
          }
          else command[0] = '\0';
        }
      }

      else
      {
        if (strcmp(args[temp], "("))
        {
          for (int counter = 0; counter <= argsCount; counter++)
          {
            if (strcmp(args[counter], ")") == 0)
            {
              command[0] = '\0';
              argsCount = prepareCommArgs(counter + 1, argsCount);
              goto A;
            }
            else command[0] = '\0';
          }
        }  
      }
    }

    if (strcmp(args[0], "||") == 0 || strcmp(args[1], "||") == 0)
    {
      int temp;
      if (arg_ptr[0] == '\0') strcpy(arg_ptr, "./");
      runProcess(command, arg_ptr, WAIT);
      if (flag_process != 0)
      {
        for (int counter = 0; counter <= argsCount; counter++)
        {
          if (strcmp(args[counter], "||") == 0)
          {
            temp = counter;
            command[0] = '\0';
            if (strcmp(args[counter+1], "(") == 0) argsCount = prepareCommArgs(counter + 2, argsCount);
            else argsCount = prepareCommArgs(counter + 1, argsCount);
            goto A;
          }
          else command[0] = '\0';
        }
      }
      else
      {
        if (strcmp(args[temp], "("))
        {
          for (int counter = 0; counter <= argsCount; counter++)
          {
            if (strcmp(args[counter], ")") == 0)
            {
              command[0] = '\0';
              argsCount = prepareCommArgs(counter + 1, argsCount);
              goto A;
            }
            else command[0] = '\0';
          }
        }  
      }
    }

    if (strcmp(args[0], "&") == 0 || strcmp(args[1], "&") == 0)
    {
      runProcess(command, arg_ptr, DONT_WAIT);
      if (flag_process != 0)
      {
        for (int counter = 0; counter <= argsCount; counter++)
        {
          if (strcmp(args[counter], "&") == 0)
          {
            command[0] = '\0';
            argsCount = prepareCommArgs(counter + 1, argsCount);
            goto A;
          }
          else command[0] = '\0';
        }
      }
      else command[0] = '\0';
    }
    if (arg_ptr[0] == '\0') strcpy(arg_ptr, "./");
    runProcess(command, arg_ptr, WAIT);
    arg_ptr[0] = '\0';

    for (int counter = 0; counter <= 10; counter++)
    {
      args[counter][0] = '\0';
    }
  }
}

int main(int argc, char *argv[])
{
  cwd[0] = '/';

  printf("\n\%s\n", "SWEB-Pseudo-Shell starting...\n");
  do
  {
    printf("\n%s %s%s", "SWEB:", cwd, "> ");
    fgets(buffer, 255, stdin);
    buffer[255] = 0;
    handle_command(buffer, 256);
    for (size_t a = 0; a < 256; a++)
      buffer[a] = 0;

  } while (running);

  return exit_code;
}
