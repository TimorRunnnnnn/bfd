#ifndef _BFD_H_
#define _BFD_H_
#include "windows.h"
#include "winsock.h"

#define BFD_VERSION						(1)
#define BFD_PACKAGE_LENTH				(24)

#define MAX_TX_INTERVAL					(1000*1000*10)/*10s*/
#define MIN_TX_INTERVAL					(1000*50)/*50ms*/
#define MAX_RX_INTERVAL					(1000*1000*10)/*10s*/
#define MIN_RX_INTERVAL					(1000*50)/*50ms*/
#define MAX_MULT						(50)/*最大检测倍数5*/
#define MIN_MULT						(2)/*最小检测倍数2*/

enum BFDSessionState
{
	SESSION_STATE_ADMIN_DOWN = 0,
	SESSION_STATE_DOWN,
	SESSION_STATE_INIT,
	SESSION_STATE_UP,
};
/*	
	0 --No Diagnostic
	1 --Control Detection Time Expired
	2 --Echo Function Failed
	3 --Neighbor Signaled Session Down
	4 --Forwarding Plane Reset
	5 --Path Down
	6 --Concatenated Path Down
	7 --Administratively Down
	8 --Reverse Concatenated Path Down
*/
enum BFDDiag
{
	DIAGNOSTIC_NO_DIAGNOSTIC = 0,
	DIAGNOSTIC_CONTROL_DETECTION_TIME_EXPIRED,
	DIAGNOSTIC_ECHO_FUNCTION_FAILED,
	DIAGNOSTIC_NEIGHBOR_SIGNALED_SESSION_DOWN,
	DIAGNOSTIC_FORWARDING_PLANE_RESET,
	DIAGNOSTIC_PATH_DOWN,
	DIAGNOSTIC_CONCATENATED_PATH_DOWN,
	DIAGNOSTIC_ADMINISTRATIVELY_DOWN,
	DIAGNOSTIC_REVERSE_CONCATENATED_PATH_DOWN,
};
/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+ -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Vers | Diag | Sta | P | F | C | A | D | M | Detect Mult | Length |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| My Discriminator |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Your Discriminator |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Desired Min TX Interval |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Required Min RX Interval |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Required Min Echo RX Interval |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
typedef struct _bfdpackage
{
	BFDDiag diagnostic : 5;/*诊断*/
	UINT32 version : 3;/*版本号*/

UINT32:1;/*保留*/
	UINT32 flagDemand : 1;
	UINT32 flagAuthenticationPresent : 1;
	UINT32 flagControlPlaneIndependent : 1;
	UINT32 flagFinal : 1;
	UINT32 flagPoll : 1;
	UINT32 sessionState : 2;/*状态*/

	UINT32 detectMult : 8;/*检测倍数*/
	UINT32 length : 8;/*报文长度*/

	INT32 myDiscreaminator;/*本地标识符*/
	INT32 yourDiscreaminator;/*远端标识符*/
	INT32 desiredMinTxInterval;/*本地支持的最小BFD报文发送间隔*/
	INT32 requiredMinRXInterval;/*本地支持的最小BFD接收间隔*/
	INT32 requiredMinEchoRXInterval;/*本地支持的最小Echo报文接收间隔*/
}BFDPackage_t;

typedef struct _udppacket
{
	SOCKADDR_IN peerAddress;
	IN_ADDR localAddress;
	BFDPackage_t BFDPackage;
}UDPPackage_t;

typedef struct _peerdata
{
	IN_ADDR destinationIP;
	INT32 reciveInterval;
	INT32 transmitInterval;
	INT32 multiplier;
}SessionArg_t;

typedef struct _neighbour_node
{
	IN_ADDR destinationIP;
	IN_ADDR localIP;
	INT32 localDiscreaminator;
	INT32 remoteDiscreaminator;
	INT32 rawTxTime;/*输入的发送时间*/
	INT32 rawRxTime;/*输入的接收时间*/
	INT32 remoteTxTime;
	INT32 remoteRxTime;
	INT32 remoteMult;
	INT32 remainTxTime;/*剩余的时间*/
	INT32 remainRxTime;
	DWORD threadID;/*线程ID*/
	BFDSessionState sessionState;
	struct _neighbour_node *next;
}SessionNode_t;

__declspec(dllexport)INT32 bfdDeleteBFDSession(char *peerip_str);
__declspec(dllexport) INT32 socketInit(void);
__declspec(dllexport) HANDLE bfdInit(void);
__declspec(dllexport) INT32 bfdCreatBFDSession(char *peerip_str, char *minReciveInterval_str, char *minTransmitInterval_str, char *multiplier_str);
__declspec(dllexport) SessionNode_t* bfdGetSessionList(void);
#endif