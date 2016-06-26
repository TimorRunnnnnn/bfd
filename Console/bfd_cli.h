#ifndef _BFD_CLI_H_
#define _BFD_CLI_H_
#include "libcli.h"
#include "windows.h"
enum CLI_ARGUMENT_STATE
{
	CLI_ARGUMENT_STATE_FALSE=0,
	CLI_ARGUMENT_STATE_TRUE,
	CLI_ARGUMENT_STATE_INCOMPLETE,
};

CLI_ARGUMENT_STATE checkArgument(cli_command *c, char *cmd);
INT32 cliBfdSetMultiplier(struct cli_def *cli, const char *command, char *argv[], int argc);
INT32 cliBfdSetTxInterval(struct cli_def *cli, const char *command, char *argv[], int argc);
INT32 cliBfdSetRxInterval(struct cli_def *cli, const char *command, char *argv[], int argc);
INT32 cliBfdCreateSession(struct cli_def *cli, const char *command, char *argv[], int argc);
INT32 cliBfdDeleteSession(struct cli_def *cli, const char *command, char *argv[], int argc);
INT32 cliBfdShowSession(struct cli_def *cli, const char *command, char *argv[], int argc);
#endif