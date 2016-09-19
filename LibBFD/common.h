/*****************************************************************
* Copyright (C) 2016 Maipu Communication Technology Co.,Ltd.*
******************************************************************
* common.h
*
* DESCRIPTION:
*	定义了一些基本宏
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
#ifndef _COMMON_H
#define _COMMON_H

#define EXIT() {\
				printf_s("\n程序发生致命错误,函数: %s",__FUNCTION__);\
				printf_s("\n在%s 的 %d 行",__FILE__,__LINE__);\
				printf_s("\n程序结束");\
				printf_s("\n");\
				system("Pause");\
				exit(42);\
					}

#define MAX(x,y)	(((x)>(y))?(x):(y))


#define BFD_PORT		(3784)

#define SET				(1)
#define RESET			(0)

/* free and zero (to avoid double-free) */
#define free_z(p) do { if (p) { free(p); (p) = 0; } } while (0)

#endif