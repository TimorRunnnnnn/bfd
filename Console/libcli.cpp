/*****************************************************************
* Copyright (C) 2016 Maipu Communication Technology Co.,Ltd.*
******************************************************************
* libcli.cpp
*
* DESCRIPTION:
*	实现了命令行接口的注册、解析、补全等功能。
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
#include <winsock2.h>
#include <windows.h>

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>

#include "libcli.h"
#include "bfd_cli.h"
#include "console.h"
#include "common.h"
# define UNUSED(d) d

#define INVALID_COMMAND		"\nInvalid Command"
#define INVALID_ARGUMENT	"\nInvalid argument @%s"

static char *cliCommandName(struct cli_def *cli, struct cli_command *command);
static CLI_ARGUMENT_STATE checkIPAddress(char *ip);
static CLI_ARGUMENT_STATE checkNumber(char *num);
static INT32 cliFindCommand(struct cli_def *cli, struct cli_command *commands, INT32 num_words, char *words[],
	INT32 start_word);
static INT32 vasprintf(char **strp, const char *fmt, va_list args);
static INT32 asprintf(char **strp, const char *fmt, ...);
static INT32 cliShowHelp(struct cli_def *cli, struct cli_command *c);
static INT32 cliInitHelp(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(INT32 argc));
static INT32 cliIntHistory(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(INT32 argc));
static INT32 cliParseLine(const char *line, char *words[], INT32 max_words);

/*****************************************************************
* DESCRIPTION:
*	获得命令的名字
* INPUTS:
*	cli - 命令行接口
*	command - 需要获得名字的命令结构体指针
* OUTPUTS:
*	outtime - 输出的当前时间
* RETURNS:
*	命令的名字的字符串
*****************************************************************/
static char *cliCommandName(struct cli_def *cli, struct cli_command *command)
{
	char *name = cli->commandname;
	char *o;

	if (name)
	{
		free(name);
	}
	if (!(name = (char*)calloc(1, 1)))
	{
		return NULL;
	}
	while (command)
	{
		o = name;
		if (asprintf(&name, "%s%s%s", command->command, *o ? " " : "", o) == -1)
		{
			fprintf(stderr, "Couldn't allocate memory for command_name: %s", strerror(errno));
			free(o);
			return NULL;
		}
		command = command->parent;
		free(o);
	}
	cli->commandname = name;
	return name;
}


/*****************************************************************
* DESCRIPTION:
*	检查ip地址是否正确
* INPUTS:
*	ip - 需要检查的字符串
* OUTPUTS:
*	none 
* RETURNS:
*	CLI_ARGUMENT_STATE_FALSE - 不是ip地址
*	CLI_ARGUMENT_STATE_INCOMPLETE - ip地址不完整
*	CLI_ARGUMENT_STATE_TRUE - 是完整的ip地址
*****************************************************************/
static CLI_ARGUMENT_STATE checkIPAddress(char *ip)
{
	INT32 len = strlen(ip);

	/*255.255.255.255最长15个字节加一个'\0'最大16字节*/
	if (len > 16)
	{
		return CLI_ARGUMENT_STATE_FALSE;
	}

	INT32 countOfNumber = 0;
	INT32 countOfDot = 0;

	for (INT32 i = 0; i < len; i++)
	{
		if ((ip[i] >= '0') && (ip[i] <= '9'))
		{
			countOfNumber++;
			if (countOfNumber == 3)
			{
				/*如果是3位数,检查是否小于255*/
				INT32 num = (ip[i - 2] - '0') * 100 + (ip[i - 1] - '0') * 10 + (ip[i] - '0');
				if (num > 255)
				{
					return CLI_ARGUMENT_STATE_FALSE;
				}
			}
			else if (countOfNumber > 3)
			{
				/*如果某一段数字大于3位*/
				return CLI_ARGUMENT_STATE_FALSE;
			}
		}
		else if (ip[i] == '.')
		{
			/*如果出现连续两个'.',或者以'.'开头*/
			if (countOfNumber == 0)
			{
				return CLI_ARGUMENT_STATE_FALSE;
			}
			else
			{
				countOfDot++;
				countOfNumber = 0;
			}
		}
		else
		{
			/*如果出现其他字符*/
			return CLI_ARGUMENT_STATE_FALSE;
		}
	}

	/*在遍历完整个字符串以后*/
	if (countOfDot > 3)
	{
		/*如果有大于3个'.'*/
		return CLI_ARGUMENT_STATE_FALSE;
	}
	else if ((countOfDot < 3) || (countOfDot == 3 && countOfNumber == 0))
	{
		/*如果小于3个'.',或者最后一位为'.',说明不完整*/
		return CLI_ARGUMENT_STATE_INCOMPLETE;
	}

	/*如果'.'等于3个*/
	return CLI_ARGUMENT_STATE_TRUE;
}

/*****************************************************************
* DESCRIPTION:
*	检查字符串是否为数字
* INPUTS:
*	num - 需要用来检查的字符串
* OUTPUTS:
*	none
* RETURNS:
*	CLI_ARGUMENT_STATE_FALSE - 不是数字
*	CLI_ARGUMENT_STATE_TRUE - 是数字
*****************************************************************/
static CLI_ARGUMENT_STATE checkNumber(char *num)
{
	INT32 len = strlen(num);
	while (len--)
	{
		if (num[len]<'0' || num[len]>'9')
		{
			return CLI_ARGUMENT_STATE_FALSE;
		}
	}
	return CLI_ARGUMENT_STATE_TRUE;
}

/*****************************************************************
* DESCRIPTION:
*	将输入的命令解析成一个二维数组字符串数组后，迭代解析每个字段，并找到对面的
*	命令结构体
* INPUTS:
*	cli - 命令行接口
*	commands - 起始的命令结构体
*	num_words - 字段数目
*	words[] - 字段的指针数组
* OUTPUTS:
*	none
* RETURNS:
*	TRUE - 解析成功，并成功执行所有命令
*	FALSE - 命令有错误
*****************************************************************/
static INT32 cliFindCommand(struct cli_def *cli, struct cli_command *commands, INT32 num_words, char *words[],
	INT32 start_word)
{
	/*start_word 是从0开始数*/
	struct cli_command *c, *again_config = NULL, *again_any = NULL;
	INT32 c_words = num_words;

	if ((words[start_word] == NULL) || (commands == NULL) || (cli == NULL) || ((num_words - 1) < start_word) || ((num_words - 1) < start_word))
	{
		printf_s("\n函数参数错误:");
		printf_s("\n &words[start_word]=%d", (INT32)words[start_word]);
		printf_s("\n &commands=%d", (INT32)commands);
		printf_s("\n &cli=%d", (INT32)cli);
		printf_s("\n num_words=%d,start_word=%d", num_words, start_word);
		system("pause");
		exit(42);
	}

	/*这里解析命令的'?',参数的在解析完命令后解析*/
	INT32 cmdLen = strlen(words[start_word]);
	/*如果是命令后面有 '?' 显示命令帮助*/
	if (words[start_word][cmdLen - 1] == '?')
	{
		/*如果只有一个 '?',显示下面所有的命令帮助*/
		BOOL findCmd = FALSE;
		for (c = commands; c; c = c->next)
		{
			if (_strnicmp(c->command, words[start_word], cmdLen - 1) == 0
				&& (c->callback || c->children))
			{
				printf_s("\n  %-20s %s", c->command, (c->help != NULL ? c->help : ""));
				findCmd = TRUE;
			}
		}
		if (findCmd == TRUE)
		{
			return CLI_QUIT;
		}
		/*如果在上面没有退出,则说明输入的命令是无法识别的,*/
		printf_s(INVALID_COMMAND);
		return CLI_ERROR;
	}

	for (c = commands; c; c = c->next)
	{
		/*先比较命令的最短匹配长度*/
		if (_strnicmp(c->command, words[start_word], c->unique_len))
		{
			continue;
		}
		/*如果和最短匹配长度相同,再匹配输入字符串长度的字符*/
		if (_strnicmp(c->command, words[start_word], strlen(words[start_word])))
		{
			continue;
		}
		/*找到了命令行*/
		INT32 rc = CLI_OK;
		/*如果这条命令下面还有子命令,首先判断这条命令是否需要参数,再解析后面的参数*/
		if (start_word == c_words - 1)
		{
			/*如果当前解析的是最后一个命令,但是需要参数*/
			if (c->argType != CLI_ARGUMENT_TYPE_NONE)
			{
				printf_s("\nCommand need argument @%s", cliCommandName(cli, c));
				return CLI_ERROR;
			}
			else
			{
				/*没有输入参数*/
				if (c->callback != NULL)
				{
					rc = c->callback(cli, cliCommandName(cli, c), NULL, 0);
				}
				return rc;
			}
		}

		INT32 commandOffset = 1;
		if (c->argType == CLI_ARGUMENT_TYPE_NONE)
		{
			commandOffset = 0;
		}

		/*在找下一个命令前,首先应该检查当前命令的参数是否正确*/
		if (!(((num_words - 1) == (start_word + 1)) && words[start_word + 1][strlen(words[start_word + 1]) - 1] == '?'))
		{
			enum CLI_ARGUMENT_STATE argState = checkArgument(c, *(words + start_word + 1));
			if (argState == CLI_ARGUMENT_STATE_FALSE)
			{
				printf_s(INVALID_ARGUMENT, cliCommandName(cli, c));
				return CLI_ERROR;
			}
			else if (argState == CLI_ARGUMENT_STATE_INCOMPLETE)
			{
				printf_s("\nIncomplete argument @%s", cliCommandName(cli, c));
				return CLI_ERROR;
			}
			else if (argState == CLI_ARGUMENT_STATE_TRUE)
			{
				rc = CLI_OK;
			}
		}
		/*如果本条命令有参数,并且当前命令是倒数第二个,解析参数最后一位是否为'?'*/
		if ((commandOffset == 1) && ((num_words - 1) == (start_word + 1)))
		{
			INT32 argLen = strlen(words[start_word + 1]);
			/*如果是参数后面有 '?' 显示参数帮助*/
			if (words[start_word + 1][argLen - 1] == '?')
			{
				/*如果不是只有一个'?',则检查前面是否满足参数要求*/
				if (argLen != 1)
				{
					char *argcheck = (char *)malloc(argLen);
					if (argcheck == NULL)
					{
						printf_s("\nInternal error");
					}
					memcpy(argcheck, words[start_word + 1], argLen);
					argcheck[argLen - 1] = '\0';
					if (checkArgument(c, argcheck) == CLI_ARGUMENT_STATE_FALSE)
					{
						printf_s("\nIncomplete argument @%s", cliCommandName(cli, c));
						return CLI_ERROR;
					}
					free(argcheck);
				}
				//printf_s("\n  %-20s %s", c->command, (c->help != NULL ? c->help : ""));
				if (c->argHelp != NULL)
				{
					printf_s("\n   %s", c->argHelp);
				}

				return CLI_QUIT;
			}
		}

		/*如果参数的个数大于本条命令+1(参数),则继续解析下一个命令,如果本条命令没有参数,如果有下一个,则解析下一个*/
		if ((num_words - 1) > (start_word + commandOffset))
		{
			if (c->children == NULL)
			{
				/*如果没有子命令,且最后是'?',显示clear或者不正确的命令*/

				INT32 offset = (c->argType == CLI_ARGUMENT_TYPE_NONE) ? (0) : (1);
				INT32 argLen = strlen(words[start_word + 1 + offset]);
				if (words[start_word + 1 + offset][argLen - 1] == '?')
				{
					/*如果不是只有一个'?',则检查前面是否满足参数要求*/
					if (argLen != 1)
					{
						printf_s(INVALID_COMMAND);
					}
					else
					{
						printf_s("\n  <clear>");
					}
					return CLI_QUIT;
				}
				else
				{
					printf_s(INVALID_ARGUMENT, cliCommandName(cli, c));
					return CLI_ERROR;
				}
			}
			/*+1+1是跳过参数,解析下一个命令*/
			rc = cliFindCommand(cli, c->children, num_words, words, start_word + 1 + commandOffset);
		}

		if (rc == CLI_OK)
		{
			/*从最后一个命令开始返回,先返回值再执行callbac,防止最后一个命令出错,但是前面的callback先执行了*/
			if (c->callback != NULL)
			{
				if (c->argType == CLI_ARGUMENT_TYPE_NONE)
				{
					rc = c->callback(cli, cliCommandName(cli, c), NULL, 0);
				}
				else
				{
					rc = c->callback(cli, cliCommandName(cli, c), words + start_word + 1, 1);
				}
			}
		}
		return rc;
	}
	printf_s(INVALID_COMMAND);
	return CLI_ERROR;
}

/*****************************************************************
* DESCRIPTION:
*	可变长度的字符串格式化，返回长度，字符串存放在malloc分配的堆里
* INPUTS:
*	fmt - 输出格式（%d %s %f 类似)
*	args - 由调用函数提供的可变参数列表
* OUTPUTS:
*	strp - 输出字符串指针
* RETURNS:
*	字符串长度
*****************************************************************/
static INT32 vasprintf(char **strp, const char *fmt, va_list args)
{
	INT32 size;

	size = vsnprintf(NULL, 0, fmt, args);
	if ((*strp = (char*)malloc(size + 1)) == NULL)
	{
		return -1;
	}

	size = vsnprintf(*strp, size + 1, fmt, args);
	return size;
}

/*****************************************************************
* DESCRIPTION:
*	根据格式化的字符串长度，申请足够的内存空间，返回长度，字符串存放
*	在malloc分配的堆里，增强sprintf
* INPUTS:
*	fmt - 输出格式（%d %s %f 类似)
*	... - 对应格式的输出内容
* OUTPUTS:
*	strp - 输出字符串指针
* RETURNS:
*	字符串长度
*****************************************************************/
static INT32 asprintf(char **strp, const char *fmt, ...)
{
	va_list args;
	INT32 size;

	va_start(args, fmt);
	size = vasprintf(strp, fmt, args);

	va_end(args);
	return size;
}

/*****************************************************************
* DESCRIPTION:
*	迭代显示一级命令的帮助
* INPUTS:
*	cli - 命令行接口
*	c - 要显示帮助的命令树的根节点
* OUTPUTS:
*	none
* RETURNS:
*	CLI_OK - 显示成功
*****************************************************************/
static INT32 cliShowHelp(struct cli_def *cli, struct cli_command *c)
{
	struct cli_command *p;

	for (p = c; p; p = p->next)
	{
		if (p->command)
		{
			printf_s("\n  %-20s %s", cliCommandName(cli, p), (p->help != NULL ? p->help : ""));
		}
	}

	return CLI_OK;
}

/*****************************************************************
* DESCRIPTION:
*	迭代显示一级命令的帮助，"help"命令的回调函数
* INPUTS:
*	cli - 命令行接口
*	其余参数未使用
* OUTPUTS:
*	none
* RETURNS:
*	CLI_OK - 显示成功
*****************************************************************/
static INT32 cliInitHelp(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(INT32 argc))
{
	if (argc != 0)
	{
		printf_s(INVALID_COMMAND);
		return CLI_ERROR;
	}
	printf_s("\nCommands available:");
	cliShowHelp(cli, cli->commands);
	return CLI_OK;
}

/*****************************************************************
* DESCRIPTION:
*	显示已输入过的命令，对应"history"的回调函数
* INPUTS:
*	cli - 命令行接口
*	其余参数未使用
* OUTPUTS:
*	none
* RETURNS:
*	CLI_OK - 显示成功
*****************************************************************/
static INT32 cliIntHistory(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(INT32 argc))
{
	INT32 i;

	printf_s("\nCommand history:");
	for (i = 0; i < MAX_HISTORY; i++)
	{
		if (cli->history[i])
		{
			printf_s("\n%3d. %s", i, cli->history[i]);
		}
	}

	return CLI_OK;
}

/*****************************************************************
* DESCRIPTION:
*	解析一行命令，分离出每个字段，只支持空格分离，
* INPUTS:
*	cli - 命令行接口
*	其余参数未使用
* OUTPUTS:
*	none
* RETURNS:
*	CLI_OK - 显示成功
*****************************************************************/
static INT32 cliParseLine(const char *line, char *words[], INT32 max_words)
{
	INT32 nwords = 0;
	const char *p = line;
	const char *word_start = 0;
	INT32 inquote = 0;

	while (*p)
	{
		if (!isspace(*p))
		{
			word_start = p;
			break;
		}
		p++;
	}

	while (nwords < max_words - 1)
	{
		if (!*p || *p == inquote || (word_start && !inquote && (isspace(*p) || *p == '|')))
		{
			if (word_start)
			{
				INT32 len = p - word_start;
				words[nwords] = (char*)malloc(len + 1);
				memcpy(words[nwords], word_start, len);
				words[nwords++][len] = '\0';
			}

			if (!*p)
			{
				break;
			}
			if (inquote)
			{
				p++; /* skip over trailing quote */
			}
			inquote = 0;
			word_start = 0;
		}
		else if (*p == '"' || *p == '\'')
		{
			inquote = *p++;
			word_start = p;
		}
		else
		{
			if (!word_start)
			{
				if (*p == '|')
				{
					if (!(words[nwords++] = _strdup("|")))
						return 0;
				}
				else if (!isspace(*p))
				{
					word_start = p;
				}
			}

			p++;
		}
	}

	return nwords;
}

/*****************************************************************
* DESCRIPTION:
*	注册一条命令
* INPUTS:
*	cli - 命令行接口指针
*	parent - 这条命令的父命令
*	command - 需要注册的命令的全名
*	callback - 命令的回调函数，为NULL表示这条命令不能执行
*	help - 这条命令的帮助信息，字符串格式
*	argType - 参数类型
* OUTPUTS:
*	none
* RETURNS:
*	这条命令创建完成的struct cli_command指针
*****************************************************************/
__declspec(dllexport) struct cli_command *cliRegisterCommand(struct cli_def *cli, struct cli_command *parent, const char *command,
	INT32(*callback)(struct cli_def *, const char *, char **, INT32), const char *help, CLI_ARGUMENT_TYPE argType, const char *argHelp)
{
	struct cli_command *c, *p;

	if (!command)
	{
		return NULL;
	}
	if (!(c = (cli_command*)calloc(sizeof(struct cli_command), 1)))
	{
		return NULL;
	}
	c->callback = callback;
	c->next = NULL;
	if (!(c->command = _strdup(command)))
	{
		return NULL;
	}
	c->parent = parent;
	if (help && !(c->help = _strdup(help)))
	{
		return NULL;
	}
	c->argType = argType;
	c->argHelp = _strdup(argHelp);
	if ((c->argType != CLI_ARGUMENT_TYPE_NONE) && c->argHelp == NULL)
	{
		return NULL;
	}

	if (parent)
	{
		if (!parent->children)
		{
			parent->children = c;
		}
		else
		{
			for (p = parent->children; p && p->next; p = p->next);
			if (p) p->next = c;
		}
	}
	else
	{
		if (!cli->commands)
		{
			cli->commands = c;
		}
		else
		{
			for (p = cli->commands; p && p->next; p = p->next);

			if (p)
			{
				p->next = c;
			}
		}
	}
	return c;
}

/*****************************************************************
* DESCRIPTION:
*	根据已经注册的命令，为每个命令创建一个最短唯一匹配字符串
* INPUTS:
*	cli - 命令行接口指针
*	commands - 命令树的根节点
* OUTPUTS:
*	none
* RETURNS:
*	CLI_OK - 创建成功
*****************************************************************/
__declspec(dllexport) INT32 cliBuildShortest(struct cli_def *cli, struct cli_command *commands)
{
	struct cli_command *c, *p;
	char *cp, *pp;
	UINT32 len;

	for (c = commands; c; c = c->next)
	{
		c->unique_len = strlen(c->command);

		c->unique_len = 1;
		for (p = commands; p; p = p->next)
		{
			if (c == p)
				continue;

			cp = c->command;
			pp = p->command;
			len = 1;

			while (*cp && *pp && *cp++ == *pp++)
			{
				len++;
			}
			if (len > c->unique_len)
			{
				c->unique_len = len;
			}
		}

		if (c->children)
		{
			cliBuildShortest(cli, c->children);
		}
	}

	return CLI_OK;
}

/*****************************************************************
* DESCRIPTION:
*	命令行接口的创建
* INPUTS:
*	none
* OUTPUTS:
*	none
* RETURNS:
*	创建的命令接口指针
*****************************************************************/
__declspec(dllexport) struct cli_def *cliInit(void)
{
	struct cli_def *cli;

	if (!(cli = (cli_def*)calloc(sizeof(struct cli_def), 1)))
	{
		return 0;
	}
	cli->buf_size = 1024;
	if (!(cli->buffer = (char*)calloc(cli->buf_size, 1)))
	{
		free_z(cli);
		return 0;
	}
	cliRegisterCommand(cli, 0, "help", cliInitHelp, "Show available commands", CLI_ARGUMENT_TYPE_NONE, NULL);
	cliRegisterCommand(cli, 0, "history", cliIntHistory,
		"Show a list of previously run commands", CLI_ARGUMENT_TYPE_NONE, NULL);
	return cli;
}

/*****************************************************************
* DESCRIPTION:
*	增加一条历史命令，一般在按下回车运行命令的时候调用
* INPUTS:
*	cli - 命令行接口的指针
*	cmd - 增加的历史命令
* OUTPUTS:
*	none
* RETURNS:
*	CLI_ERROR - 命令历史缓冲区达到最大
*	CLI_OK - 增加命令历史成功
*****************************************************************/
__declspec(dllexport) INT32 cliAddHistory(struct cli_def *cli, const char *cmd)
{
	INT32 i;
	for (i = 0; i < MAX_HISTORY; i++)
	{
		if (!cli->history[i])
		{
			// if (i == 0 || _stricmp(cli->history[i-1], cmd))
			if ((i != 0) && (_stricmp(cli->history[i - 1], cmd) == 0))
			{
				/*如果和上次执行的命令相同,返回Error*/
				return CLI_REPEAT;
			}
			if (!(cli->history[i] = _strdup(cmd)))
			{
				return CLI_ERROR;
			}
			return CLI_OK;
		}
	}
	// No space found, drop one off the beginning of the list
	free(cli->history[0]);
	for (i = 0; i < MAX_HISTORY - 1; i++)
	{
		cli->history[i] = cli->history[i + 1];
	}
	if (!(cli->history[MAX_HISTORY - 1] = _strdup(cmd)))
	{
		return CLI_ERROR;
	}
	return CLI_OK;
}

/*****************************************************************
* DESCRIPTION:
*	解析并运行一条命令
* INPUTS:
*	cli - 命令行接口的指针
*	command - 输入的命令
* OUTPUTS:
*	none
* RETURNS:
*	CLI_ERROR - 运行失败
*	CLI_OK - 运行成功
*****************************************************************/
__declspec(dllexport) INT32 cliRunCommand(struct cli_def *cli, const char *command)
{
	INT32 r;
	UINT32 num_words, i;
	char *words[CLI_MAX_LINE_WORDS] = { 0 };

	if (!command)
	{
		return CLI_ERROR;
	}
	while (isspace(*command))
	{
		command++;
	}
	if (!*command)
	{
		return CLI_OK;
	}
	num_words = cliParseLine(command, words, CLI_MAX_LINE_WORDS);

	if (num_words)
	{
		r = cliFindCommand(cli, cli->commands, num_words, words, 0);
	}
	else
	{
		r = CLI_ERROR;
	}
	for (i = 0; i < num_words; i++)
	{
		free(words[i]);
	}
	if (r == CLI_QUIT)
	{
		return r;
	}
	return CLI_OK;
}

/*****************************************************************
* DESCRIPTION:
*	根据已输入的字符，补全命令
* INPUTS:
*	cli - 命令行接口的指针
*	command - 已输入的部分字符
*	completions - 补全的字符串数组的指针
*	max_completions - 最大补全个数
* OUTPUTS:
*	none
* RETURNS:
*	补全的命令条数
*****************************************************************/
__declspec(dllexport) INT32 cliGetCompletions(struct cli_def *cli, const char *command, char **completions, INT32 max_completions)
{
	struct cli_command *c;
	struct cli_command *n;
	INT32 num_words, save_words, i, k = 0;
	char *words[CLI_MAX_LINE_WORDS] = { 0 };

	if (!command)
	{
		return 0;
	}
	while (isspace(*command))
	{
		command++;
	}

	save_words = num_words = cliParseLine(command, words, sizeof(words) / sizeof(words[0]));
	if (!command[0] || command[strlen(command) - 1] == ' ')
	{
		num_words++;
	}

	if (num_words != 0)
	{
		for (c = cli->commands, i = 0; c && i < num_words && k < max_completions; c = n)
		{
			n = c->next;
			if (words[i] && _strnicmp(c->command, words[i], strlen(words[i])))
			{
				continue;
			}
			if (i < num_words - 1)
			{
				if (strlen(words[i]) < c->unique_len)
				{
					continue;
				}
				if ((i + 1) > (num_words - 1))
				{
					/*如果没有输入参数*/
					return 0;
				}
				if (checkArgument(c, words[i + 1]) != CLI_ARGUMENT_STATE_TRUE)
				{
					/*如果前面的参数不正确*/
					return 0;
				}
				if (c->argType != CLI_ARGUMENT_TYPE_NONE)
				{
					i++;
				}
				i++;
				n = c->children;
				continue;
			}
			completions[k++] = c->command;
		}
	}
	for (i = 0; i < save_words; i++)
	{
		free(words[i]);
	}
	return k;
}

/*****************************************************************
* DESCRIPTION:
*	检查参数是否正确和完整
* INPUTS:
*	c - 需要检查参数的命令指针
*	cmd - 要检查的参数的字符串
* OUTPUTS:
*	none
* RETURNS:
*	CLI_ARGUMENT_STATE_TRUE - 参数正确
*	CLI_ARGUMENT_STATE_FALSE - 参数错误
*	CLI_ARGUMENT_STATE_INCOMPLETE - 参数不完整
*****************************************************************/
__declspec(dllexport) CLI_ARGUMENT_STATE checkArgument(cli_command *c, char *arg)
{
	CLI_ARGUMENT_STATE ret = CLI_ARGUMENT_STATE_TRUE;
	if (c->argType == CLI_ARGUMENT_TYPE_NONE)
	{
		return CLI_ARGUMENT_STATE_TRUE;
	}
	if (arg == NULL)
	{
		return CLI_ARGUMENT_STATE_FALSE;
	}
	if (c->argType == CLI_ARGUMENT_TYPE_IP)
	{
		ret = checkIPAddress(arg);
	}
	else if (c->argType == CLI_ARGUMENT_TYPE_NUMBER)
	{
		ret = checkNumber(arg);
	}
	else if (c->argType == CLI_ARGUMENT_TYPE_STRING)
	{
		/*这里需要检查字符串参数是否正确*/
		//argState = checkString(*(words + start_word + 1));
	}
	return ret;
}


/*****************************************************************
* DESCRIPTION:
*	响应↑↓键，显示命令历史
* INPUTS:
*	cli - 要显示历史的命令行接口
*	c - 键盘输入，↑或者↓
*	in_history - 保存了输入历史的缓冲区指针
*	numOfHistory - 到目前为止的运行命令个数
* OUTPUTS:
*	none
* RETURNS:
*	历史命令，字符串格式
*****************************************************************/
__declspec(dllexport) char* cliHistory(struct cli_def *cli, char c, INT32 *in_history, INT32 numOfHistory)
{
	if (c == 0x48) // Up
	{
		(*in_history)--;
		if (*in_history < 0)
		{
			*in_history = 0;
		}
		if (cli->history[*in_history] != NULL)
		{
			return cli->history[*in_history];
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		(*in_history)++;
		if (*in_history > (numOfHistory - 1))
		{
			*in_history = numOfHistory - 1;
			return NULL;
		}
		else
		{
			if (cli->history[*in_history] != NULL)
			{
				return cli->history[*in_history];
			}
		}
	}
	return NULL;
}
