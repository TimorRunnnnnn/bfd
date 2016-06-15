#ifndef __LIBCLI_H__
#define __LIBCLI_H__


#include <stdio.h>
#include <stdarg.h>
#include <time.h>


#define CLI_OK                  TRUE
#define CLI_ERROR               FALSE
#define CLI_QUIT                -2
#define CLI_ERROR_ARG           -3
#define CLI_REPEAT				-4

#define MAX_HISTORY             256

#define LIBCLI_HAS_ENABLE       1

#define PRINT_PLAIN             0
#define PRINT_FILTERED          0x01
#define PRINT_BUFFERED          0x02

#define CLI_MAX_LINE_LENGTH     4096
#define CLI_MAX_LINE_WORDS      128

enum CLI_ARGUMENT_TYPE
{
	CLI_ARGUMENT_TYPE_NONE,
	CLI_ARGUMENT_TYPE_IP,
	CLI_ARGUMENT_TYPE_NUMBER,
	CLI_ARGUMENT_TYPE_STRING,
	CLI_ARGUMENT_TYPE_MULTIPLE,
};

struct cli_def
{
	struct cli_command *commands;
	char *history[MAX_HISTORY];
	char *commandname;  // temporary buffer for cli_command_name() to prevent leak
	char *buffer;
	unsigned buf_size;
};
struct cli_command
{
	char *command;
	int(*callback)(struct cli_def *, const char *, char **, int);
	enum CLI_ARGUMENT_TYPE argType;/*命令参数类型*/
	char *argHelp;/*参数说明*/

	unsigned int unique_len;
	char *help;
	struct cli_command *next;
	struct cli_command *children;
	struct cli_command *parent;
};

struct cli_def *cliInit(void);
int cliGetCompletions(struct cli_def *cli, const char *command, char **completions, int max_completions);
struct cli_command *cliRegisterCommand(struct cli_def *cli, struct cli_command *parent, const char *command,
	int(*callback)(struct cli_def *, const char *, char **, int), const char *help,CLI_ARGUMENT_TYPE argType,const char *argHelp);
int cliRunCommand(struct cli_def *cli, const char *command);
int cliBuildShortest(struct cli_def *cli, struct cli_command *commands);
int cliAddHistory(struct cli_def *cli, const char *cmd);


#endif
