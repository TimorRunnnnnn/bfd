
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


int vasprintf(char **strp, const char *fmt, va_list args)
{
	int size;

	size = vsnprintf(NULL, 0, fmt, args);
	if ((*strp = (char*)malloc(size + 1)) == NULL)
	{
		return -1;
	}

	size = vsnprintf(*strp, size + 1, fmt, args);
	return size;
}

int asprintf(char **strp, const char *fmt, ...)
{
	va_list args;
	int size;

	va_start(args, fmt);
	size = vasprintf(strp, fmt, args);

	va_end(args);
	return size;
}


char* cliHistory(struct cli_def *cli, char c, INT32 *in_history, INT32 numOfHistory)
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


char *cliCommandName(struct cli_def *cli, struct cli_command *command)
{
	char *name = cli->commandname;
	char *o;

	if (name) free(name);
	if (!(name = (char*)calloc(1, 1)))
		return NULL;

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
int cliBuildShortest(struct cli_def *cli, struct cli_command *commands)
{
	struct cli_command *c, *p;
	char *cp, *pp;
	unsigned len;

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
				len++;

			if (len > c->unique_len)
				c->unique_len = len;
		}

		if (c->children)
			cliBuildShortest(cli, c->children);
	}

	return CLI_OK;
}


struct cli_command *cliRegisterCommand(struct cli_def *cli, struct cli_command *parent, const char *command,
	int(*callback)(struct cli_def *, const char *, char **, int), const char *help, CLI_ARGUMENT_TYPE argType, const char *argHelp)
{
	struct cli_command *c, *p;

	if (!command) return NULL;
	if (!(c = (cli_command*)calloc(sizeof(struct cli_command), 1))) return NULL;

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
			if (p) p->next = c;
		}
	}
	return c;
}

int cliShowHelp(struct cli_def *cli, struct cli_command *c)
{
	struct cli_command *p;

	for (p = c; p; p = p->next)
	{
		if (p->command)
		{
			printf_s("\n  %-20s %s", cliCommandName(cli, p), (p->help != NULL ? p->help : ""));
		}

// 		if (p->children)
// 			cliShowHelp(cli, p->children);
	}

	return CLI_OK;
}

int cliInitHelp(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc))
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

int cliIntHistory(struct cli_def *cli, UNUSED(const char *command), UNUSED(char *argv[]), UNUSED(int argc))
{
	int i;

	printf_s("\nCommand history:");
	for (i = 0; i < MAX_HISTORY; i++)
	{
		if (cli->history[i])
			printf_s("\n%3d. %s", i, cli->history[i]);
	}

	return CLI_OK;
}


struct cli_def *cliInit(void)
{
	struct cli_def *cli;

	if (!(cli = (cli_def*)calloc(sizeof(struct cli_def), 1)))
		return 0;

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


int cliAddHistory(struct cli_def *cli, const char *cmd)
{
	int i;
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
		return CLI_ERROR;
	return CLI_OK;
}

static int cliParseLine(const char *line, char *words[], int max_words)
{
	int nwords = 0;
	const char *p = line;
	const char *word_start = 0;
	int inquote = 0;

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
				int len = p - word_start;
				words[nwords] = (char*)malloc(len + 1);
				memcpy(words[nwords], word_start, len);
				words[nwords++][len] = '\0';
			}

			if (!*p)
				break;

			if (inquote)
				p++; /* skip over trailing quote */

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
					word_start = p;
			}

			p++;
		}
	}

	return nwords;
}

int cliFindCommand(struct cli_def *cli, struct cli_command *commands, int num_words, char *words[],
	int start_word)
{
	/*start_word 是从0开始数*/
	struct cli_command *c, *again_config = NULL, *again_any = NULL;
	int c_words = num_words;

	//     if (filters[0])
	//         c_words = filters[0];

		// Deal with ? for help
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


	// 	if (words[start_word][strlen(words[start_word]) - 1] == '?')
	// 	{
	// 		int l = strlen(words[start_word]) - 1;
	// 
	// 		if (commands->parent && commands->parent->callback)
	// 			cliPrint(cli, "%-20s %s", cliCommandName(cli, commands->parent),
	// 				(commands->parent->help != NULL ? commands->parent->help : ""));
	// 
	// 		for (c = commands; c; c = c->next)
	// 		{
	// 			if (_strnicmp(c->command, words[start_word], l) == 0
	// 				&& (c->callback || c->children))
	// 				cliPrint(cli, "  %-20s %s", c->command, (c->help != NULL ? c->help : ""));
	// 		}
	// 
	// 		return CLI_OK;
	// 	}

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
		int rc = CLI_OK;
		// Found a word!
// 		/*如果这个命令下面没有子命令了,则说明后面的都是参数,只有这种情况允许多参数,其实不应该允许有多个参数的*/
// 		if (c->children == NULL)
// 		{
// 			// Last word
// // 			if ((num_words - 1) != start_word)
// // 			{
// // 				/*如果不是最后一个单词,则不做任何操作*/
// // 			}
// 			if (!c->callback)
// 			{
// 				printf_s("\nNo callback for \"%s\"", cliCommandName(cli, c));
// 				return CLI_ERROR;
// 			}
// 			else
// 			{
// 				/*改到这里执行函数*/
// 				rc = c->callback(cli, cliCommandName(cli, c), words + start_word + 1, c_words - start_word - 1);
// 				return rc;
// 			}
// 		}
// 		else
// 		{
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
		if (!(((num_words - 1) == (start_word + 1))&&words[start_word+1][strlen(words[start_word + 1]) - 1] == '?'))
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
			if (words[start_word+1][argLen - 1] == '?')
			{
				/*如果不是只有一个'?',则检查前面是否满足参数要求*/
				if (argLen != 1)
				{
					char *argcheck = (char *)malloc(argLen);
					if (argcheck==NULL)
					{
						printf_s("\nInternal error");
					}
					memcpy(argcheck, words[start_word + 1],argLen);
					argcheck[argLen-1] = '\0';
					if (checkArgument(c, argcheck) == CLI_ARGUMENT_STATE_FALSE)
					{
						printf_s("\nIncomplete argument @%s", cliCommandName(cli, c));
						return CLI_ERROR;
					}
					free(argcheck);
				}
				//printf_s("\n  %-20s %s", c->command, (c->help != NULL ? c->help : ""));
				if (c->argHelp!=NULL)
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
				INT32 argLen = strlen(words[start_word + 1+offset]);
				if (words[start_word + 1+offset][argLen - 1] == '?')
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

int cliRunCommand(struct cli_def *cli, const char *command)
{
	int r;
	unsigned int num_words, i;
	char *words[CLI_MAX_LINE_WORDS] = { 0 };

	if (!command) return CLI_ERROR;
	while (isspace(*command))
		command++;

	if (!*command) return CLI_OK;

	num_words = cliParseLine(command, words, CLI_MAX_LINE_WORDS);

	if (num_words)
		r = cliFindCommand(cli, cli->commands, num_words, words, 0);
	else
		r = CLI_ERROR;

	for (i = 0; i < num_words; i++)
		free(words[i]);

	if (r == CLI_QUIT)
		return r;

	return CLI_OK;
}

int cliGetCompletions(struct cli_def *cli, const char *command, char **completions, int max_completions)
{
	struct cli_command *c;
	struct cli_command *n;
	int num_words, save_words, i, k = 0;
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
		free(words[i]);

	return k;
}