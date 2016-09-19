/*****************************************************************
* Copyright (C) 2016 Maipu Communication Technology Co.,Ltd.*
******************************************************************
* console.cpp
*
* DESCRIPTION:
*	实现了用户界面，响应键盘按键。
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
#include "stdio.h"
#include "windows.h"
#include "conio.h"
#include "console.h"
#include "libcli.h"
#include "bfd_cli.h"
#include "libbfd.h"
#include "common.h"


static void Key_Left(void);
static void Key_Right(INT32 inputBufferTail);
static void refreshLine(char *buffer, INT32 cursorPos);
static void newLine(char *buf, INT32 *tail);
static INT32 socketInit(void);

/*****************************************************************
* DESCRIPTION:
*	初始化整个系统，注册一些列默认的命令，然后只用getch循环获取键盘输入
* INPUTS:
*	none
* OUTPUTS:
*	none
* RETURNS:
*	进程结束代码
*****************************************************************/
int main(void)
{
	char inputBuffer[INPUT_BUFFER_MAX_CHAR];
	CONSOLE_CURSOR_INFO cci;         //定义光标信息结构体  
	CONSOLE_SCREEN_BUFFER_INFO bInfo;
	HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
	cci.bVisible = TRUE;
	cci.dwSize = CURSOR_SIZE;
	SetConsoleCursorInfo(handle_out, &cci);
	SetConsoleTitle(L"BFD测试");

	socketInit();
	HANDLE tem = bfdInit();
	CloseHandle(tem);

	struct cli_def *cli = cliInit();
	struct cli_command *com = NULL;
	char argHelp[INPUT_BUFFER_MAX_CHAR];
	memset(argHelp, '\0', sizeof(argHelp));

	com = cliRegisterCommand(cli, NULL, "bfd", NULL, "BFD configuration commands", CLI_ARGUMENT_TYPE_NONE, NULL);
	com = cliRegisterCommand(cli, com, "session", cliBfdCreateSession, "BFD Session configuration commands", CLI_ARGUMENT_TYPE_IP, \
		"<A.B.C.D>	Destination IP Address of BFD Session");
	sprintf_s(argHelp, "<%d-%d>		Milliseconds", MIN_RX_INTERVAL / 1000, MAX_RX_INTERVAL / 1000);
	com = cliRegisterCommand(cli, com, "min-receive-interval", cliBfdSetRxInterval, "    Minimum receive interval capability", CLI_ARGUMENT_TYPE_NUMBER, \
		argHelp);
	sprintf_s(argHelp, "<%d-%d>		Milliseconds", MIN_TX_INTERVAL / 1000, MAX_TX_INTERVAL / 1000);
	com = cliRegisterCommand(cli, com, "min-transmit-interval", cliBfdSetTxInterval, "    Transmit interval between BFD packets", CLI_ARGUMENT_TYPE_NUMBER, \
		argHelp);
	sprintf_s(argHelp, "<%d-%d>		value used to multiply the interval", MIN_MULT, MAX_MULT);
	cliRegisterCommand(cli, com, "multiplier", cliBfdSetMultiplier, " Multiplier value used to compute holddown", CLI_ARGUMENT_TYPE_NUMBER, \
		argHelp);

	com = cliRegisterCommand(cli, NULL, "no", NULL, "Negate a command or set its defaults", CLI_ARGUMENT_TYPE_NONE, NULL);
	com = cliRegisterCommand(cli, com, "bfd", NULL, "BFD configuration commands", CLI_ARGUMENT_TYPE_NONE, NULL);
	cliRegisterCommand(cli, com, "session", cliBfdDeleteSession, "BFD Session configuration commands", CLI_ARGUMENT_TYPE_IP, "<A.B.C.D>	Destination IP Address of BFD Session");

	com = cliRegisterCommand(cli, NULL, "show", NULL, "Show running system information", CLI_ARGUMENT_TYPE_NONE, NULL);
	com = cliRegisterCommand(cli, com, "bfd", NULL, "BFD configuration commands", CLI_ARGUMENT_TYPE_NONE, NULL);
	cliRegisterCommand(cli, com, "session", cliBfdShowSession, "BFD protocol info", CLI_ARGUMENT_TYPE_NONE, NULL);
	cliBuildShortest(cli, cli->commands);

	memset(inputBuffer, '\0', sizeof(inputBuffer));
	inputBuffer[0] = '>';
	printf_s(">");

	INT32 inputBufferTail = 1;
	INT32 historyPos = 0;
	INT32 NumberOfHistory = 0;
	char c = 0;
	char lastChar = 0;

	/*测试用*/
	//char test[] = "bfd s 192.168.80.100 m 500 m 500 m 40";
	//cliRunCommand(cli, test);
	
	while (1)
	{
		lastChar = c;
		c = _getch();
		if ((c >= '0'&&c <= '9') || (c >= 'a'&&c <= 'z') || (c >= 'A'&&c <= 'Z') || (c == ' ')\
			|| (c == '/') || (c == '\\') || (c == '.'))
		{
			/*字符*/
			GetConsoleScreenBufferInfo(handle_out, &bInfo);
			if ((inputBufferTail + 1) >= INPUT_BUFFER_MAX_CHAR)
			{
				newLine(inputBuffer, &inputBufferTail);
				continue;
			}
			if (bInfo.dwCursorPosition.X == (inputBufferTail))
			{
				inputBuffer[bInfo.dwCursorPosition.X] = c;
				printf_s("%c", c);
			}
			else
			{
				for (INT32 i = inputBufferTail; i > (bInfo.dwCursorPosition.X - 1); i--)
				{
					inputBuffer[i] = inputBuffer[i - 1];
				}
				inputBuffer[bInfo.dwCursorPosition.X] = c;
				refreshLine(inputBuffer, bInfo.dwCursorPosition.X + 1);
			}
			inputBufferTail++;
		}
		else if (c == '\t')
		{
			/*TAB completion*/
			char *completions[CLI_MAX_LINE_WORDS];
			INT32 num_completions = 0;

			num_completions = cliGetCompletions(cli, &inputBuffer[1], completions, CLI_MAX_LINE_WORDS);
			if (num_completions == 0)
			{
				printf_s("\a");
			}
			else if (num_completions == 1)
			{
				//Single completion
				if (inputBufferTail == 1)
				{
					continue;
				}
				INT32 charNumOfWord = 0;
				INT32 i = inputBufferTail - 1;//Tail指向的是最后一个字母后面的位置,-1是最后一个字母
				while ((inputBuffer[i] != ' ') && (i >= 1))
				{
					inputBuffer[i] = ' ';//清空
					i--;
					charNumOfWord++;
					inputBufferTail--;
				}
				INT32 len = strlen(completions[0]);
				memcpy(&inputBuffer[inputBufferTail], completions[0], len);
				inputBufferTail += len;
				inputBuffer[inputBufferTail] = ' ';
				inputBufferTail++;/*补全了以后在*/
				refreshLine(inputBuffer, inputBufferTail);
			}
			else if (lastChar == '\t')
			{
				// double tab
				int i;
				printf_s("\n");
				for (i = 0; i < num_completions; i++)
				{
					printf_s("%s", completions[i]);
					if (i % 4 == 3)
						printf_s("\n");
					else
						printf_s("\t");
				}
				//if (i % 4 != 3)
				{
					printf_s("\n\n");
				}
				//cli->showprompt = 1;
				refreshLine(inputBuffer, inputBufferTail);
			}
			continue;
		}
		else if (c == '\r')
		{
			/*回车*/
			if (!(inputBuffer[1] == ' ' || inputBuffer[1] == '\0'))
			{
				if (cliAddHistory(cli, inputBuffer) == CLI_OK)
				{
					NumberOfHistory++;
				}
			}
			historyPos = NumberOfHistory;

			cliRunCommand(cli, &inputBuffer[1]);
			memset(inputBuffer, '\0', sizeof(inputBuffer));
			inputBuffer[0] = '>';
			inputBufferTail = 1;
			printf_s("\n>");
		}
		else if (c == '?')
		{
			/* ? 显示帮助*/
			inputBuffer[inputBufferTail] = c;
			printf_s("%c", c);
			cliRunCommand(cli, &inputBuffer[1]);
			printf_s("\n\n");
			inputBuffer[inputBufferTail] = '\0';//去掉'?'
			refreshLine(inputBuffer, inputBufferTail);

		}
		else if (c == '\b')
		{
			/*退格键*/
			GetConsoleScreenBufferInfo(handle_out, &bInfo);
			if (bInfo.dwCursorPosition.X == 1)
			{
				continue;
			}
			for (INT32 i = bInfo.dwCursorPosition.X - 1; i < inputBufferTail; i++)
			{
				inputBuffer[i] = inputBuffer[i + 1];
			}
			inputBufferTail--;
			inputBuffer[inputBufferTail] = ' ';/*替换掉以前最后一个字符*/
			refreshLine(inputBuffer, bInfo.dwCursorPosition.X - 1);
			inputBuffer[inputBufferTail] = '\0';/*替换回来*/
		}
		else if (c == (char)0xe0 || c == 0)
		{
			/*方向键(←)： 0xe04b
			方向键(↑)： 0xe048
			方向键(→)： 0xe04d
			方向键(↓)： 0xe050*/
			c = _getch();
			switch (c)
			{
			case 0x50:
			case 0x48:
			{
				char *his = cliHistory(cli, c, &historyPos, NumberOfHistory);
				if (his != NULL)
				{
					INT32 len = strlen(his);
					memset(&inputBuffer[1], '\0', sizeof(INPUT_BUFFER_MAX_CHAR));
					memcpy(inputBuffer, his, len + 1);
					refreshLine(inputBuffer, len);
					inputBufferTail = len;
				}

			}break;

			case 0x4d:Key_Right(inputBufferTail); break;
			case 0x4b:Key_Left(); break;
			case 83:
			{
				/*delete键*/
				GetConsoleScreenBufferInfo(handle_out, &bInfo);
				if (bInfo.dwCursorPosition.X == inputBufferTail)
				{
					continue;
				}
				for (INT32 i = bInfo.dwCursorPosition.X; i < inputBufferTail; i++)
				{
					inputBuffer[i] = inputBuffer[i + 1];
				}
				inputBufferTail--;
				inputBuffer[inputBufferTail] = ' ';/*替换掉以前最后一个字符*/
				refreshLine(inputBuffer, bInfo.dwCursorPosition.X);
				inputBuffer[inputBufferTail] = '\0';/*替换回来*/
				break;
			}
			default:
				break;
			}
		}
	}
}

/*****************************************************************
* DESCRIPTION:
*	按下左键的时候，光标向左移动一位
* INPUTS:
*	none
* OUTPUTS:
*	none
* RETURNS:
*	none
*****************************************************************/
static void Key_Left(void)
{
	HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO bInfo;
	COORD coursePos;

	GetConsoleScreenBufferInfo(handle_out, &bInfo);
	coursePos.X = ((bInfo.dwCursorPosition.X - 1) < 1 ? 1 : (bInfo.dwCursorPosition.X - 1));
	coursePos.Y = bInfo.dwCursorPosition.Y;
	SetConsoleCursorPosition(handle_out, coursePos);
}

/*****************************************************************
* DESCRIPTION:
*	按下左键的时候，光标向右移动一位
* INPUTS:
*	none
* OUTPUTS:
*	none
* RETURNS:
*	none
*****************************************************************/
static void Key_Right(INT32 inputBufferTail)
{
	HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO bInfo;
	COORD coursePos;

	GetConsoleScreenBufferInfo(handle_out, &bInfo);
	coursePos.X = (bInfo.dwCursorPosition.X + 1) > inputBufferTail ? inputBufferTail : (bInfo.dwCursorPosition.X + 1);
	coursePos.Y = bInfo.dwCursorPosition.Y;
	SetConsoleCursorPosition(handle_out, coursePos);
}


/*****************************************************************
* DESCRIPTION:
*	刷新一整行,把光标置位到cursorpos
* INPUTS:
*	buffer - 输入缓冲区
*	cursorPos - 移动到的光标位置
* OUTPUTS:
*	none
* RETURNS:
*	none
*****************************************************************/
static void refreshLine(char *buffer, INT32 cursorPos)
{
	COORD pos, oldpos;
	CONSOLE_CURSOR_INFO cci;         //定义光标信息结构体  
	CONSOLE_SCREEN_BUFFER_INFO bInfo;
	HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);

	GetConsoleScreenBufferInfo(handle_out, &bInfo);
	oldpos.X = cursorPos;
	oldpos.Y = bInfo.dwCursorPosition.Y;
	pos.X = 0;
	pos.Y = bInfo.dwCursorPosition.Y;
	cci.dwSize = CURSOR_SIZE;
	cci.bVisible = FALSE;/*先关闭光标,防止闪烁*/
	SetConsoleCursorInfo(handle_out, &cci);
	SetConsoleCursorPosition(handle_out, pos);
	for (INT32 i = 0; i < INPUT_BUFFER_MAX_CHAR;i++)
	{
		/*清空整行*/
		printf_s(" ");
	}
	SetConsoleCursorPosition(handle_out, pos);
	puts(buffer);
	cci.bVisible = TRUE;
	SetConsoleCursorPosition(handle_out, oldpos);
	SetConsoleCursorInfo(handle_out, &cci);
}

/*****************************************************************
* DESCRIPTION:
*	创建一个新的行
* INPUTS:
*	buf - 输入缓冲区
*	tail - 输入缓冲区结尾的序号的指针
* OUTPUTS:
*	none
* RETURNS:
*	none
*****************************************************************/
static void newLine(char *buf,INT32 *tail)
{
	memset(buf, '\0', INPUT_BUFFER_MAX_CHAR);
	buf[0] = '>';
	*tail = 1;
	printf_s("\n>");
}

/*****************************************************************
* DESCRIPTION:
*	初始化socket
* INPUTS:
*	none
* OUTPUTS:
*	none
* RETURNS:
*	TRUE - 配置成功
*****************************************************************/
static INT32 socketInit(void)
{
	INT16 wVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	INT32 err = WSAStartup(wVersion, &wsaData);

	if (err != 0)
	{
		printf("\nWSAStartup failed");
		EXIT();
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("\ncould not find a usable version of Winsock.dll");
		WSACleanup();
		EXIT();
	}
	return TRUE;
}

