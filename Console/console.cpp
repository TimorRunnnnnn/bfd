#include "stdio.h"
#include "windows.h"
#include "conio.h"
#include "console.h"
#include "libcli.h"

static CommandNode_t *tail;

void Key_Left(void)
{
	HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO bInfo;
	COORD coursePos;

	GetConsoleScreenBufferInfo(handle_out, &bInfo);
	coursePos.X = ((bInfo.dwCursorPosition.X - 1) < 1 ? 1 : (bInfo.dwCursorPosition.X - 1));
	coursePos.Y = bInfo.dwCursorPosition.Y;
	SetConsoleCursorPosition(handle_out, coursePos);
}

void Key_Right(INT32 inputBufferTail)
{
	HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO bInfo;
	COORD coursePos;

	GetConsoleScreenBufferInfo(handle_out, &bInfo);
	coursePos.X = (bInfo.dwCursorPosition.X + 1) > inputBufferTail ? inputBufferTail : (bInfo.dwCursorPosition.X + 1);
	coursePos.Y = bInfo.dwCursorPosition.Y;
	SetConsoleCursorPosition(handle_out, coursePos);
}


/*刷新一整行,把光标置位到cursorpos*/
void refreshLine(char *buffer, INT32 cursorPos)
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

void commandParse(char *command)
{
	while (*command!='\0')
	{
		
	}
}

void newLine(char *buf,INT32 *tail)
{
	memset(buf, '\0', INPUT_BUFFER_MAX_CHAR);
	buf[0] = '>';
	*tail = 1;
	printf_s("\n>");
}