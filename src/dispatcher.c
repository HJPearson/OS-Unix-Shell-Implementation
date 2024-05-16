#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "dispatcher.h"
#include "shell_builtins.h"
#include "parser.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>


static void pipe_command(struct command *pipeline, int write_fd, int read_fd, bool first) {
	if (pipeline->output_type == COMMAND_OUTPUT_PIPE) {
		if ((dup2(write_fd, STDOUT_FILENO)) == -1) {
			perror("dup2 failed write");
			exit(-1);
		}
	}
	if (!first) {
		if ((dup2(read_fd, STDIN_FILENO)) == -1) {
			perror("dup2 failed read");
			exit(-1);
		}
	}
}

/*
*
* file_redirection() - helper function run inside of dispatch_external_command() that handles file redirections
*
*/
static void file_input_redirection(struct command *pipeline) 
{
	if (pipeline->input_filename != NULL) {
		int new_fd;
		if ((new_fd = open(pipeline->input_filename, O_RDONLY)) == -1) {
			perror("failed to open file");
			exit(-1);
		}
		if ((dup2(new_fd, STDIN_FILENO)) == -1) {
			perror("dup2 failed");
			exit(-1);
		}
	}
}

static void file_output_redirection(struct command *pipeline) 
{	
	if (pipeline->output_type == COMMAND_OUTPUT_FILE_TRUNCATE) {
		// Opens a file with permisions to write and creates the file if it doesn't exist. Truncates
		int new_fd;
		if ((new_fd = open(pipeline->output_filename, O_WRONLY | O_CREAT | O_TRUNC)) == -1) {
			perror("failed to open file");
			exit(-1);
		}
		if ((dup2(new_fd, STDOUT_FILENO)) == -1) {
			perror("dup2 failed");
			exit(-1);
		}
	} else if (pipeline->output_type == COMMAND_OUTPUT_FILE_APPEND) {
		// Opens a file with permisions to write and creates the file if it doesn't exist. Appends
		int new_fd;
		if ((new_fd = open(pipeline->output_filename, O_WRONLY | O_CREAT | O_APPEND)) == -1) {
			perror("failed to open file");
			exit(-1);
		}
		if ((dup2(new_fd, STDOUT_FILENO)) == -1) {
			perror("dup2 failed");
			exit(-1);
		}
	}
}

/**
 * dispatch_external_command() - run a pipeline of commands
 *
 * @pipeline:   A "struct command" pointer representing one or more
 *              commands chained together in a pipeline.  See the
 *              documentation in parser.h for the layout of this data
 *              structure.  It is also recommended that you use the
 *              "parseview" demo program included in this project to
 *              observe the layout of this structure for a variety of
 *              inputs.
 *
 *
 * Return: The return status of the last command executed in the
 * pipeline.
 */
static int dispatch_external_command(struct command *pipeline)
{
	pid_t child_id, id;
	int status;
	int fd[2] = {-1, -1};
	int prev_fd[2] = {-1, -1};
	bool isFirstCommand = true;
	while (true) {
		if (pipe(fd) == -1) {
			perror("pipe failed");
			exit(-1);
		}
		id = fork();
		if (id == -1) {
			fprintf(stderr, "error: fork failed.\n");
			return -1;
		} 
		if (id == 0) {
			// Child
			file_input_redirection(pipeline);
			file_output_redirection(pipeline);
			pipe_command(pipeline, fd[1], prev_fd[0], isFirstCommand);
			if (execvp(pipeline->argv[0], pipeline->argv) == -1) {
				perror("execvp");
				exit(-1);
			}
		}
		// Parent
		if ((child_id = wait(&status)) == -1) {
			fprintf(stderr, "error: wait failed.\n");
			return -1;
		}
		if (close(fd[1]) == -1) {
			perror("close failed");
			exit(-1);
		}
		if (pipeline->output_type != COMMAND_OUTPUT_PIPE) {
			if (close(fd[0]) == -1) {
				perror("close failed");
				exit(-1);
			}
			return WEXITSTATUS(status);
		}
		pipeline = pipeline->pipe_to;
		isFirstCommand = false;
		prev_fd[0] = fd[0];
		prev_fd[1] = fd[1];
	}
	return -1;
}

/**
 * dispatch_parsed_command() - run a command after it has been parsed
 *
 * @cmd:                The parsed command.
 * @last_rv:            The return code of the previously executed
 *                      command.
 * @shell_should_exit:  Output parameter which is set to true when the
 *                      shell is intended to exit.
 *
 * Return: the return status of the command.
 */
static int dispatch_parsed_command(struct command *cmd, int last_rv,
				   bool *shell_should_exit)
{
	/* First, try to see if it's a builtin. */
	for (size_t i = 0; builtin_commands[i].name; i++) {
		if (!strcmp(builtin_commands[i].name, cmd->argv[0])) {
			/* We found a match!  Run it. */
			return builtin_commands[i].handler(
				(const char *const *)cmd->argv, last_rv,
				shell_should_exit);
		}
	}

	/* Otherwise, it's an external command. */
	return dispatch_external_command(cmd);
}

int shell_command_dispatcher(const char *input, int last_rv,
			     bool *shell_should_exit)
{
	int rv;
	struct command *parse_result;
	enum parse_error parse_error = parse_input(input, &parse_result);

	if (parse_error) {
		fprintf(stderr, "Input parse error: %s\n",
			parse_error_str[parse_error]);
		return -1;
	}

	/* Empty line */
	if (!parse_result)
		return last_rv;

	rv = dispatch_parsed_command(parse_result, last_rv, shell_should_exit);
	free_parse_result(parse_result);
	return rv;
}
