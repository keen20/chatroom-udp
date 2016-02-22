/*
 * pub.h
 * 公共头文件
 */

#ifndef _PUB_H_
#define _PUB_H_

#include<list>
#include<algorithm>
using namespace std;
//C2S
#define C2S_LOGIN       0x01
#define C2S_LOGOUT      0x02
#define C2S_ONLINE_USE  0X03
#define MSG_LEN         512
//S2C
#define S2C_LOGIN_OK          0x04
#define S2C_ALREADY_LOGINED   0x05
#define S2C_SOMEONE_LOGIN     0x06
#define S2C_SOMEONE_LOGOUT    0x07
#define S2C_ONLINE_USER       0x08
//C2C
#define C2C_CHAT 0x09
//消息结构，客户端与服务端之间
typedef struct message
{
		int cmd;
		char body[MSG_LEN];//用户名，或者USER_INFO/CHAT_MSG
}MESSAGE;

//用户信息
typedef struct user_info
{
		char username[16];
		unsigned int ip;
		unsigned short port;
}USER_INFO;

//聊天消息
typedef struct chat_msg
{
		char username[16];
		char msg[100];
}CHAT_MSG;

typedef list<USER_INFO> USER_LIST;

#endif /* PUB_H_ */
