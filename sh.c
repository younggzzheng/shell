#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include "./jobs.c"
job_list_t *job_list;
int jid = 1;
int shell_pid;
int foreground_pid;

/*
returns an array of char pointers, each of which points to the first character in each token.
a token is separated by a space. Ends on a NULL pointer.
Input: a char* (string) message, and
an int, the size of the buffer that is storing the user input, size
a string array, tokens, that will be altered.
Output: void. sets tokens to an array of strings that are each of the tokens. a token is anything separated by a space.

Example: "  echo hello world!     >    out.txt abc "
=>  {echo, hello, world!, >, out.txt, abc, NULL}
*/

void tokenize(char message[], char** tokens) {
  char* word;
  word = strtok(message, " ");
  tokens[0] = word;
  int i = 1;
  while ((word = strtok(NULL, " ")) != NULL) {
    tokens[i] = word;
    i++;
  }
  tokens[i] = NULL;
}
/*
redirect will, given >, >>, or <, close and open the proper files so that the input/output
will be redirected to the correct place. redirect will throw and error message to stderr
if rdr_input = 1 and rdr_symbol = > or >>, and if rdr_output = 0 and rdr_symbol = <

Input: string rdr_symbol: must be >, >> or <.
int pointers rdr_input and rdr_output
string file_name: the name of the file that you want to redirect to.
Output: 0 if all went well, 1 if there was an error., but the file descriptors will be set to the proper numbers.
will set the two booleans to true once redirected.

*/
int redirect(char* input_file, char* output_file, char* output_rdr_type, int rdr_input, int rdr_output) {
  /* syntax error checking */
  if (rdr_input != 0 && input_file == NULL) {
    char errorMessage[] = "No redirection file specified.";
    fprintf(stderr, "%s\n", errorMessage);
  }

  if (rdr_output != 0 && output_file == NULL) {
    char errorMessage[] = "No redirection file specified.";
    fprintf(stderr, "%s\n", errorMessage);
  }

  if (rdr_input != 0 &&
    (strcmp(input_file, ">") == 0
    || (strcmp(input_file, ">>") == 0
    || (strcmp(input_file, "<") == 0)))) {
      char errorMessage[] = "syntax error: output file is a redirection symbol.";
      fprintf(stderr, "%s\n", errorMessage);
      return 1;
    }
  if (output_file != 0 &&
    (strcmp(output_file, ">") == 0
    || (strcmp(output_file, ">>") == 0
    || (strcmp(output_file, "<") == 0)))) {
      char errorMessage[] = "syntax error: output file is a redirection symbol.";
      fprintf(stderr, "%s\n", errorMessage);
      return 1;
    }

  /* FOR REDIRECTING OUTPUT */
  if (rdr_output) {
    if (strcmp(output_rdr_type, ">") == 0) {
      close(STDOUT_FILENO);
      if (open(output_file, O_WRONLY | O_TRUNC | O_CREAT, 0666) == -1) {
        char errorMessage[] = "Cannot write to: ";
        strcat(errorMessage, input_file);
        fprintf(stderr, "%s\n", errorMessage);
        return 1;
      }
    } else {
      if (strcmp(output_rdr_type, ">>") == 0) {
        close(STDOUT_FILENO);
        if (open(output_file, O_WRONLY | O_APPEND | O_CREAT , 0666) == -1) {
          char errorMessage[] = "Does not exist or could not write to: ";
          strcat(errorMessage, input_file);
          fprintf(stderr, "%s\n", errorMessage);
          return 1;
        }
      } else {
        perror("Redirection failed.");  // should never get here.
      }
    }
  }

  /* FOR REDIRECTING INPUT */
  if (rdr_input) {
    close(STDIN_FILENO);
    if (open(input_file, O_RDONLY, 0666) == -1) {
    char errorMessage[] = "Does not exist or cannot read from: ";
    strcat(errorMessage, input_file);
    fprintf(stderr, "%s\n", errorMessage);
    return 1;
  }
}
return 0;
}

/*
  input: takes in a char** tokens that is the tokens that comes out form tokenize.
  output: an int 1 if all the fg bg direction has been done correctly, 0 otherwise.
*/
int foreground_or_background (char* tokens[]) {
    if (strcmp(tokens[0], "fg") == 0 || strcmp(tokens[0], "bg") == 0) {
      if (tokens[1] == NULL) {
        if (strcmp(tokens[0], "fg") == 0){
        fprintf(stderr, "%s\n", "fg: syntax error" );
      } else {
        fprintf(stderr, "%s\n", "bg: syntax error" );
      }
      return 0;
      }
      if ((char)((tokens[1])[0]) != '%') {
        if (strcmp(tokens[0], "fg") == 0){
        fprintf(stderr, "%s\n", "fg: job input does not begin with %%" );
      } else {
        fprintf(stderr, "%s\n", "bg: job input does not begin with %%" );

      }
        return 0;
      }
      char *percent = strchr(tokens[1], '%');
      percent ++;
      int jidfb = atoi(percent);
      if (jidfb < 1) {
        fprintf(stderr, "%s\n", "job not found" );
        return 0;
      }

      int pidfb = get_job_pid(job_list, jidfb);
      if (pidfb == -1) {
        fprintf(stderr, "%s\n", "job not found" );
        return 0;
      }
      if (strcmp(tokens[0], "fg") == 0){
       // control should be given to pid of fg'd process.
       if (tcsetpgrp(STDIN_FILENO, pidfb) == -1) {
         perror("");
       }
      foreground_pid = pidfb;
      } else {
        if (tcsetpgrp(STDIN_FILENO, shell_pid) == -1) {
          perror("");
        }
      }
      if (kill(-pidfb, SIGCONT) == -1) {
        perror("");
      }
      int status;
      if (strcmp(tokens[0], "fg") == 0) {
      if (waitpid(pidfb, &status, WUNTRACED) == -1) {
        perror("");
      }
    }
    if (strcmp(tokens[0], "fg") == 0) {
      if (WIFSIGNALED(status)) {
        int s = WTERMSIG(status);
        printf("[%d] (%d) terminated by signal %d\n", jidfb, pidfb, s);
        if(remove_job_pid(job_list, pidfb) == -1) {
          fprintf(stderr, "%s\n", "3Could not remove job from joblist.");
        }
      }
      if (WIFSTOPPED(status)) {
        int s = WSTOPSIG(status);
        printf("[%d] (%d) suspended by signal %d\n", jidfb, pidfb, s);
        if (update_job_jid(job_list, jidfb, _STATE_STOPPED) == -1) {
          fprintf(stderr, "%s\n", "Could not update job");
        }
      }
    }
      // give control back to shell
      if (tcsetpgrp(STDIN_FILENO, shell_pid) == -1) {
        perror("");
      }
      return 0;
    }
    return 1;
}



/*
Processes the user input. Main calls this, and only this, in its REPL.
intput:
char* input is the user input. example "/bin/echo hello"
int   size is the size of input.

output: nothing, but it will execute what the user wants to execute.
*/
void process(char* input, int size, job_list_t *job_list) {
  char *tokens[size];
  char *argv[size];
  int i = 0;
  int arg_index = 0;
  // these strings will be used for redirecting.
  char *output_file = NULL;
  char *input_file = NULL;
  char *output_rdr_type = NULL;  // either NULL, >, or >>.
  int rdr_input = 0;  // set to 1 if you want to redirect input.
  int rdr_output = 0;  // set to 1 if you want to redirect output.
  tokenize(input, tokens);
  // populate argv and input/output_file.
  while (tokens[i] != NULL) {
    if (strcmp(tokens[i], ">") == 0 || strcmp(tokens[i], ">>") == 0 || strcmp(tokens[i], "<") == 0) {
      if (strcmp(tokens[i], ">") == 0) {
        if (output_file != NULL) {
          fprintf(stderr, "%s\n", "> found: Cannot redirect output more than once.");
          return;
        }
        rdr_output = 1;
        output_rdr_type = tokens[i];
        output_file = tokens[i+1];
        if (output_file == NULL) {
          fprintf(stderr, "%s\n", "No output file specified.");
          return;
        }
      }

      if (strcmp(tokens[i], ">>") == 0) {
        if (output_file != NULL) {
          fprintf(stderr, "%s\n", ">> found: Cannot redirect output more than once.");
          return;
        }
        rdr_output = 1;
        output_rdr_type = tokens[i];
        output_file = tokens[i+1];
        if (output_file == NULL) {
          fprintf(stderr, "%s\n", "No output file specified.");
          return;
        }
      }

      if (strcmp(tokens[i], "<") == 0) {
        if (input_file != NULL) {
          fprintf(stderr, "%s\n", "< found: Cannot redirect input more than once.");
          return;
        }
        rdr_input = 1;
        input_file = tokens[i+1];
        if (input_file == NULL) {
          fprintf(stderr, "%s\n", "No input_file file specified.");
          return;
        }
      }
      i++;
    } else {
      argv[arg_index] = tokens[i];
      arg_index++;
    }
    i++;
  }


  if (arg_index == 0) {
    fprintf(stderr, "%s\n", "No command specified.");
    return;
  }

  if (strcmp(argv[0], "") == 0) {
    fprintf(stderr, "%s\n", "redirects with no command");
    return;
  }
  argv[arg_index] = NULL;
  char *path_name = argv[0];

  // changes argv[0] to the binary name rather than full path.
  if (strrchr(argv[0], '/') != NULL) {
    char *last_backslash = strrchr(argv[0], '/');
    last_backslash++;
    argv[0] = last_backslash;
  }


  /* ampersand stuff*/
    int bg_flag = 0;
    char *ampersand = argv[arg_index - 1];
    if (ampersand != NULL && strcmp(ampersand, "&") == 0) {
      //printf("%s\n", "WE FOUND AN AMPERSAND!!"); //TODO: END ME PLEASE.
      argv[arg_index - 1] = NULL;
      bg_flag = 1;
    }
  /*        BUILT-INS        */
      /* cd */
      if (strcmp(path_name, "cd") == 0) {
        // chdir will return -1 if there was an error, 0 if all good.
        if (argv[1] == NULL) {
          fprintf(stderr, "%s\n", "cd: syntax error");
          return;
        }
        if (chdir(argv[1]) != 0) {
          perror(argv[1]);
        }
        return;
      }

      /* ln */
      if (strcmp(path_name, "ln") == 0) {
        if (argv[1] == NULL || argv[2] == NULL) {
          fprintf(stderr, "%s\n", "syntax error: ln must be given two arguments <old_name> <new_name>.");
          return;
        }
        if (link(argv[1], argv[2]) != 0) {
          perror("");
        }
        return;
      }

      /* rm */
      if (strcmp(path_name, "rm") == 0) {
        if (argv[1] == NULL) {
          fprintf(stderr, "%s\n", "syntax error: rm must be given one argument.");
          return;
        }
        if (unlink(argv[1]) != 0) {
          perror(argv[1]);
        }
        return;
      }

      /* exit */
      if (strcmp(tokens[0], "exit") == 0) {
        cleanup_job_list(job_list);
        exit(0);
      }

      if (strcmp(tokens[0], "jobs") == 0) {
      //  printf("%s\n", "we found a job");
      if(tokens[1] != NULL) {
        fprintf(stderr, "%s\n", "jobs: syntax error" );
        return;
      }
        jobs(job_list);
        return;
      }

      if (foreground_or_background(tokens) == 0) {
        return;
      }


        /* CHILD PROCESS FORKING*/
  int pid;
  int parent_pid = getpid();
  if ((pid = fork()) == 0) {
    pid = getpid();
    if (setpgid(pid, pid) == -1) {
      perror("could not set pgid.");
    }
    if (bg_flag != 1) {
      if (tcsetpgrp(STDIN_FILENO, pid) == -1) {
        perror("could not change control of terminal.");
      }
    }

  /* CHILD signal handling */
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);

    int success = redirect(input_file, output_file, output_rdr_type, rdr_input, rdr_output);

    if (success != 0) {
      cleanup_job_list(job_list);
      exit(1);
    }
    execv(path_name, argv);
    perror("execv");
    cleanup_job_list(job_list);
    exit(1);
  }
  /* parent process continues here */
 if (bg_flag == 1) { // this was a background call.

   if (add_job(job_list, jid, pid,_STATE_RUNNING, path_name) == -1) {
     fprintf(stderr, "%s\n", "Could not add job.");
   }
   printf("[%d] (%d)\n", jid, pid);
   jid++;

 } else { // foreground call
   int status;
   if (waitpid(pid, &status, WUNTRACED) == -1) {
     perror("waitpid failed.");
   }
   if (WIFSIGNALED(status)) {
     int s = WTERMSIG(status);
     printf("[%d] (%d) terminated by signal %d\n", jid, pid, s);
   }
   if (WIFSTOPPED(status)) {
     int s = WSTOPSIG(status);
     printf("[%d] (%d) suspended by signal %d\n", jid, pid, s);

     if (add_job(job_list, jid, pid,_STATE_STOPPED, path_name) == -1) {
       perror("Could not add job.");
     }
     jid++;
   }
   if (tcsetpgrp(STDIN_FILENO, parent_pid) == -1) {
     perror("could not change control of terminal.");
   }
   signal(SIGINT, SIG_IGN);
   signal(SIGTSTP, SIG_IGN);
   signal(SIGQUIT, SIG_IGN);
   signal(SIGTTOU, SIG_IGN);
 }
}

/*
This contains a REPL that reads the user input. This input
is then passed to process, which does the necessary operations on the user
input.
*/
int main() {
  job_list = init_job_list();
  shell_pid = getpid();
  while (1) {
    /* SHELL signal handling */
      signal(SIGINT, SIG_IGN);
      signal(SIGTSTP, SIG_IGN);
      signal(SIGQUIT, SIG_IGN);
      signal(SIGTTOU, SIG_IGN);
    int BUFSIZE = 1048;
    char buf[BUFSIZE];
    int input_bytes;

    /* REAPING */
    int pid;
    int status;
    while ((pid = get_next_pid(job_list)) != -1) {
      int reap_jid = get_job_jid(job_list, pid);
      if (reap_jid == -1) {
        fprintf(stderr, "%s\n", "Could not reap next job.");
      }
      if (waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED)==0) {
        continue;
      }

      if (WIFSIGNALED(status)) {
        int s = WTERMSIG(status);
        printf("[%d] (%d) terminated by signal %d\n", reap_jid, pid, s); 
        if(remove_job_pid(job_list, pid) == -1) {
          fprintf(stderr, "%s\n", "Could not remove job from joblist.");
        }
      }

      if (WIFEXITED(status)) {
        int s = WEXITSTATUS(status);
        if (foreground_pid != pid) {
        printf("[%d] (%d) terminated with exit status %d\n", reap_jid, pid, s);
        }
        if (remove_job_pid(job_list, pid) == -1) {
          fprintf(stderr, "%s\n", "Could not remove job from joblist.");
        }
      }

      if (WIFSTOPPED(status)) {
        int s = WSTOPSIG(status);
        printf("[%d] (%d) suspended by signal %d\n", reap_jid, pid, s);
        update_job_jid(job_list, reap_jid, _STATE_STOPPED);
      }
      if (WIFCONTINUED(status)) {

        printf("[%d] (%d) resumed\n", reap_jid, pid);
        if (update_job_jid(job_list, reap_jid, _STATE_RUNNING) == -1) {
          fprintf(stderr, "%s\n", "Could not update current job's state.");
        }
      }

    }

    #ifdef PROMPT
    if (printf("SHELL> ") < 0) {
      fprintf(stderr, "%s\n", "Unable to write to window.\n");
    }
    fflush(stdout);
    #endif
    // reading from user input.
    input_bytes = (int) read(0, buf, sizeof(buf));
    if (input_bytes < 0) {
      fprintf(stderr, "Unable to read.");
    }
    if (input_bytes == 0) {
      cleanup_job_list(job_list);
      exit(0);
    }
    if (input_bytes == 1) {
      continue;
    }
    if (input_bytes > 1) {
      buf[input_bytes-1] = '\0';
    }
    char *tab;
    while ((tab = strchr(buf, '\t')) != NULL) {
      *tab = ' ';
    }
    process(buf, input_bytes, job_list);
  }
  cleanup_job_list(job_list);
  return 0;
}
