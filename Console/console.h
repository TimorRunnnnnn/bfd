#ifndef _CONSOLE_H
#define _CONSOLE_H

#define CURSOR_SIZE			(100)
#define INPUT_BUFFER_MAX_CHAR			(255)/*一行最大的字符个数*/

typedef struct _commandnode
{
	struct _commandnode *last;
	char *commond;
	struct _commandnode *next;
}CommandNode_t;

char* cliHistory(struct cli_def *cli, char c, INT32 *in_history, INT32 numOfHistory);

void Key_Left(void);
void Key_Right(INT32 inputBufferTail);
void refreshLine(char *buffer, INT32 cursorPos);
void newLine(char *buf, INT32 *tail);
#endif