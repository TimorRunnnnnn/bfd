#pragma comment(lib, "Ws2_32.lib")
#include "bfd_cli.h"
#include "windows.h"
#include "libcli.h"
#include "bfd.h"
#include "common.h"
#include "console.h"
#include "winsock.h"

static char *inputReceiveInterval;
static char *inputTransmitInterval;
static char *inputMult;

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

CLI_ARGUMENT_STATE checkArgument(cli_command *c, char *cmd)
{
	CLI_ARGUMENT_STATE ret = CLI_ARGUMENT_STATE_TRUE;
	if (c->argType==CLI_ARGUMENT_TYPE_NONE)
	{
		return CLI_ARGUMENT_STATE_TRUE;
	}
	if (cmd == NULL)
	{
		return CLI_ARGUMENT_STATE_FALSE;
	}
	if (c->argType == CLI_ARGUMENT_TYPE_IP)
	{
		ret = checkIPAddress(cmd);
	}
	else if (c->argType == CLI_ARGUMENT_TYPE_NUMBER)
	{
		ret = checkNumber(cmd);
	}
	else if (c->argType == CLI_ARGUMENT_TYPE_STRING)
	{
		/*这里需要检查字符串参数是否正确*/
		//argState = checkString(*(words + start_word + 1));
	}
	return ret;
}

static BOOL setArg(char **arg, char *input)
{
	if (input == NULL)
	{
		return FALSE;
	}
	if (checkNumber(input) != CLI_ARGUMENT_STATE_TRUE)
	{
		return FALSE;
	}
	char *tem = (char *)malloc(strlen(input) + 1);
	if (tem == NULL)
	{
		return FALSE;
	}
	memcpy(tem, input, strlen(input) + 1);
	*arg = tem;
	return TRUE;
}

INT32 cliBfdSetMultiplier(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	return setArg(&inputMult, *argv);
}
INT32 cliBfdSetTxInterval(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	return setArg(&inputTransmitInterval, *argv);
}
INT32 cliBfdSetRxInterval(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	return setArg(&inputReceiveInterval, *argv);
}
INT32 cliBfdCreateSession(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	INT32 ret = FALSE;
	if (inputMult == NULL || inputTransmitInterval == NULL || inputReceiveInterval == NULL \
		|| checkIPAddress(*argv) != CLI_ARGUMENT_STATE_TRUE)
	{
		printf_s("\nCreate BFD Session Failed : Argument Error.");
	}
	else
	{
		ret = bfdCreatBFDSession(*argv, inputReceiveInterval, inputTransmitInterval, inputMult);
	}
	free_z(inputTransmitInterval);
	free_z(inputReceiveInterval);
	free_z(inputMult);
	return ret;
}
INT32 cliBfdDeleteSession(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	if (checkIPAddress(*argv)!=CLI_ARGUMENT_STATE_TRUE)
	{
		printf_s("\nDelete BFD Session Failed : Argument Error.");
	}
	else
	{
		bfdDeleteBFDSession(*argv);
	}
	return TRUE;
}

INT32 cliBfdShowSession(struct cli_def *cli, const char *command, char *argv[], int argc)
{
	const char *state[4] = {
		"ADMINDOWN",
		"DOWN",
		"INIT",
		"UP",
	};
	printf_s("\n\nBFD Sessions");
	SessionNode_t *walk = bfdGetSessionList();
	if (walk!=NULL)
	{
		printf_s("\nNeighAddr			LD/RD			State");
	}
	char str[INPUT_BUFFER_MAX_CHAR];
	memset(str, '\0', sizeof(str));
	while (walk != NULL)
	{
		sprintf_s(str, "\n%s			%d/%d			%s", inet_ntoa(walk->destinationIP), walk->localDiscreaminator, walk->remoteDiscreaminator, state[walk->sessionState]);
		printf_s("%s", str);
		walk = walk->next;
	}
	printf_s("\n");


	return TRUE;
}