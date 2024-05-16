# OS-Unix-Shell-Implementation
Implementation of a Unix shell

Significant starter code was given for this project. If you wish to see my code, view the dispatcher.c file in src.

My code focuses on the following:
- Creating and handling new processes via fork()
- Handling shell input via execvp()
- Input/output redirection
- Piping

If you plan on running this code, not that it may not work unless run in the Docker container specified by the provided file in .devcontainer.

To run the program, first run 'make' to compile, then run './shell' to run the shell.
Typical Unix commands may be run. Here are some example formats of commands using piping:

command1 | command2 | command3 | command4
command1 arg1 arg2 | command2 | command3 arg1 arg2
command1 < inputfile | command2 | command3 > outputfile