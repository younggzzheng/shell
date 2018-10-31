# shell
Main code is in sh.c!

Terminal run bash shell clone that supports redirection, extensive and safe signal handling, foreground/background, and many bash builtins.

Use as you would with a normal bash shell.

# Method breakdowns:

main:
Contains a REPL that reads user input using read into a buffer of size 1048. Calls process.
cleans up the job list if you exit.

tokenize:
(1) uses strtok to read the string until there's nothing left.
(2) add a null at the end of the array.

process:
(1) tokenize the input into an array of strings, terminated with a null pointer.
(2) go through the array, picking out all of the redirect stuff and setting the proper field variables to those things.
    at the same time, be populating argv.
(3) fork off a child, do the executing and the redirecting.
(4) if the argument is fg or bg, then call foreground_or_background

redirect:
(1) check to see if the redirection syntax is correct
(2) check the redirection flags to see if we've redirected before already
(3) open and close the proper files to change the file descriptors so redirection happens.

foreground_or_background:
(1) parses the user input so that you get the jid of the thing you want to fg/bg.
(2) moves around terminal control if you want to fg a paused process so that that process gains control of terminal
(3) bg changes terminal control back to the terminal while adding the process to the job list, then increments the global jid counter.

compile by running make. That will, be default, do the prompt version.
make 33noprompt to do the non-prompt version.
