/*****************************************************************
* Copyright (C) 2016 Maipu Communication Technology Co.,Ltd.*
******************************************************************
* libcli.h
*
* DESCRIPTION:
*	实现了命令行接口需要的宏和数据结构。
* AUTHOR:
*	谭帮成
* CREATED DATE:
*	2016 年 5 月 15 日
* REVISION:
*	1.0
*
* MODIFICATION HISTORY
* --------------------
* $Log:$
*
*****************************************************************/
#pragma once
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "bfd_cli.h"

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
	char *buffer; //临时存储用的buffer
	unsigned int buf_size;
};
struct cli_command
{
	char *command;
	INT32(*callback)(struct cli_def *, const char *, char **, INT32);
	enum CLI_ARGUMENT_TYPE argType;/*命令参数类型*/
	char *argHelp;/*参数说明*/
	UINT32 unique_len;
	char *help;
	struct cli_command *next;
	struct cli_command *children;
	struct cli_command *parent;
};

__declspec(dllexport)CLI_ARGUMENT_STATE checkArgument(cli_command *c, char *cmd);
__declspec(dllexport) struct cli_def *cliInit(void);
__declspec(dllexport) INT32 cliGetCompletions(struct cli_def *cli, const char *command, char **completions, INT32 max_completions);
__declspec(dllexport) struct cli_command *cliRegisterCommand(struct cli_def *cli, struct cli_command *parent, const char *command,
	INT32(*callback)(struct cli_def *, const char *, char **, INT32), const char *help,CLI_ARGUMENT_TYPE argType,const char *argHelp);
__declspec(dllexport) INT32 cliRunCommand(struct cli_def *cli, const char *command);
__declspec(dllexport) INT32 cliBuildShortest(struct cli_def *cli, struct cli_command *commands);
__declspec(dllexport) INT32 cliAddHistory(struct cli_def *cli, const char *cmd);
__declspec(dllexport) char* cliHistory(struct cli_def *cli, char c, INT32 *in_history, INT32 numOfHistory);
