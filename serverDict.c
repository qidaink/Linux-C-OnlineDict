/** =====================================================
 * Copyright © hk. 2022-2022. All rights reserved.
 * File name: serverDict.c
 * Author   : fanhua
 * Description: 在线词典服务器端——TCP,多进程
 * ======================================================
 */
/* 头文件 */
#include <stdio.h>     /* perror scanf printf */
#include <stdlib.h>    /* exit   atoi  system */
#include <unistd.h>    /* sleep  read  close getpid fork*/
#include <errno.h>      /* errno号 */

#include <sys/types.h>  /* socket getpid    bind listen accept send fork */
#include <sys/socket.h> /* socket inet_addr bind listen accept send */
#include <arpa/inet.h>  /* htons  inet_addr inet_pton */
#include <netinet/in.h> /* ntohs  inet_addr inet_ntop*/

#include <string.h>    /* bzero strncasecmp strlen */

#include <signal.h>    /* signal */
#include <sys/wait.h>  /* waitpid */

#include <sqlite3.h>   /* sqlite3_open sqlite3_errmsg */
#include <time.h>      /* time localtime_r */
#define N 32

#define R 1   /* user - register */
#define U 2   /* user - update   */
#define D 3   /* user - delect   */
#define S 4   /* user - show     */
#define L 5   /* user - login    */
#define Q 6   /* user - query    */
#define H 7   /* user - history  */

#define  DATABASE  "user.db"

/* 定义通信双方的信息结构体 */
#define N 32
typedef struct
{
	int flag;/* 标记是否为root用户 */
	int type;
	char name[N];
	char data[256];
} MSG;
/* 客户端信息的结构体 */
typedef struct
{
	char ipv4_addr[16];/* 转换后的IP地址 */
	int port;          /* 转换后的端口 */
} CLIENT_INFO;
/* 服务器启动 */
int serverStart(void);/* 启动服务器 */
int serverStartMenu(void);/* 服务器启动菜单 */
int serverEnterListen(char *serverIP, int serverPort);/* 服务器进入监听状态 */
/* 客户端数据处理(多进程) */
int clientDataHandle(void *arg, sqlite3 * db, CLIENT_INFO client_info);/* 进程处理客户端数据函数 */
void sigChildHandle(int signo);  /* 信号回收子进程，避免出现僵尸进程 */
/* 用户功能实现与数据响应 */
int userShowCallBack(void* arg, int f_num, char ** f_value, char ** f_name);
int serverToClientUserShow(int accept_fd, MSG * msg, sqlite3 * db);
int serverUserShow(sqlite3 *db);/* 数据库中用户名和密码显示 */
int serverRegister(int accept_fd, MSG *msg, sqlite3 *db); /* 处理客户端用户注册请求，进行用户注册 */
int serverLogin(int accept_fd, MSG * msg, sqlite3 * db);
int serverGetDate(char * date);/* 获取当前时间字符串 */
int serverSearchWord(int accept_fd, MSG * msg, char word[]);/* 查询指定单词 */
int serverQuery(int accept_fd, MSG * msg, sqlite3 * db);
int serverHistory(int accept_fd, MSG * msg, sqlite3 * db);
int serverUpdate(int accept_fd, MSG * msg, sqlite3 * db);/* 用户密码修改 */
int serverDelete(int accept_fd, MSG * msg, sqlite3 * db);/* 用户账户删除 */


int main(int argc, char *argv[])
{
	int socket_fd = -1;/* 服务器的socket套接字 */
	/* 1.启动服务器 */
	if ( (socket_fd = serverStart()) < 0)
	{
		printf("server error!\n");
		exit(-1);
	}
	/* 2.注册信号处理函数，实现进程结束自动回收 */
	signal(SIGCHLD, sigChildHandle);
	/* 3.数据库操作 */
	sqlite3 * db;  /* 数据库操作句柄 */
	char * errmsg; /* 保存错误信息 */
	/* 3.1打开数据库 */
	if(sqlite3_open(DATABASE, &db) != SQLITE_OK)
	{
		printf("sqlite3_open: %s\n", sqlite3_errmsg(db));
		return -1;
	}
	else
	{
		printf("open DATABASE success.\n");
	}
	/* 3.2创建用户名称和密码数据表 */
	if(sqlite3_exec(db, "create table if not exists user(name char primary key, passwd char, flag int)",
	                NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("Create or open user table error:%s\n", errmsg);
	}
	else
	{
		printf("Create or open user table success!\n");
	}
	/* 3.3创建查询记录数据表 */
	if(sqlite3_exec(db, "create table if not exists history(name char, word char, timr char)",
	                NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("Create or open history table error:%s\n", errmsg);
	}
	else
	{
		printf("Create or open history table success!\n");
	}
	/* 4.阻塞等待客户端连接请求所需变量定义 */
	int accept_fd = -1;
	struct sockaddr_in cin;          /* 用于存储成功连接的客户端的IP信息 */
	socklen_t addrlen = sizeof(cin);
	/* 5.打印成功连接的客户端的信息相关变量定义 */
	CLIENT_INFO client_info;
	/* 6.多进程程相关变量定义 */
	pid_t pid = -1;
	while(1)
	{
		/* 等待连接 */
		if ((accept_fd = accept(socket_fd, (struct sockaddr *)&cin, &addrlen)) < 0)
		{
			perror("accept");
			exit(-1);
		}
		/* 创建进程处理客户端请求 */
		pid = fork();
		if(pid < 0)/* 进程创建失败 */
		{
			perror("fork");
			break;
		}
		else if(pid > 0)/* 父进程 */
		{
			/* 关闭正文件描述符的话可以节约资源，但是这样每个客户端连接过来的描述符都是一样的，但是通信并不受影响 */
			// close(accept_fd);/* 子进程会复制父进程的资源包括文件描述符，所以父进程这里不再需要这个新的socket描述符 */
		}
		else /* 子进程，这里会复制之前成功连接到服务器的客户端而产生的的新的accept_fd */
		{
			close(socket_fd);/* 子进程用于处理客户端请求，所以不需要用于监听的那个socket套接字了 */
			/* 获取连接成功的客户端信息 */
			if (!inet_ntop(AF_INET, (void *)&cin.sin_addr, client_info.ipv4_addr, sizeof(cin)))
			{
				perror ("inet_ntop");
				exit(-1);
			}
			client_info.port = ntohs(cin.sin_port);
			printf ("clinet(\033[1;32m%s:%d\033[0m) is connected successfully![accept_fd=%d]\n", client_info.ipv4_addr, client_info.port, accept_fd);
			clientDataHandle(&accept_fd, db, client_info);/* 处理客户端数据 */
			exit(0);
		}
	}
	/* 7.关闭文件描述符 */
	close(socket_fd);
	return 0;
}
/*=================================================*/
/**
 * @Function: serverStartMenu
 * @Description: 服务器启动菜单
 * @param   : none
 * @return  : 返回一个整数
 *            0,显示成功;
 *            -1,显示失败
 */
int serverStartMenu(void)
{
	printf("\n\033[1m---------------- Server Start Description -------------------\033[0m\n");
	printf("|\033[1;32m First, We should enter the IP and port of the server.\033[0m\n");
	printf("|\n");
	printf("|\033[1;33m serverIP: IP address of the local NIC(or 0.0.0.0).\033[0m\n");
	printf("|\n");
	printf("|\033[1;33m serverPort: >5000.\033[0m\n");
	printf("|\033[1;31m Attention: serverIP = 0.0.0.0 , It listens to all IP addresses.\033[0m\n");
	printf("\033[1m-----------------------------------------------------------\033[0m\n");
	return 0;
}

/**
 * @Function: serverStart
 * @Description: 启动服务器
 * @param   : none
 * @return  : 返回一个整数
 *            非负数,启动成功，且表示socket描述符;
 *            -1,启动失败
 */
int serverStart(void)
{
	char confirm;
	int ret = -1;
	char serverIP[16] = {0}; /* 服务器端IP地址 */
	int serverPort = -1; /* 服务器端口 */
	/* 1.打印提示信息 */
	serverStartMenu();
	printf("Whether to use the default settings(Y or N):");
	ret = scanf("%c%*[^\n]", &confirm);
	if(ret < 0)
	{
		perror("scanf");
		return -1;
	}
	if(confirm == 'Y' || confirm == 'y')
	{
		strncpy(serverIP, "0.0.0.0", 16);
		serverPort = 5001;
	}
	else
	{
		/* 2.输入服务器端IP地址和端口 */
		printf("serverIP serverPort: ");
		ret = scanf("%s %d%*[^\n]", serverIP, &serverPort);
		if(ret < 0)
		{
			perror("scanf");
			return -1;
		}
	}
	printf("server:\033[1;32m %s %d \033[0m\n", serverIP, serverPort);
	/* 3.建立TCP连接 */
	ret = serverEnterListen(serverIP, serverPort);
	return ret;
}

/**
 * @Function: serverEnterListen
 * @Description: 服务器进入监听状态
 * @param serverIP   : 服务器IP
 * @param serverPort : 服务器端口
 * @return  : 返回一个整数
 *            socket_fd,启动成功，并返回socket套接字;
 *            -1,启动失败
 */
int serverEnterListen(char *serverIP, int serverPort)
{
	/* 1.创建套接字，得到套接字描述符 */
	int socket_fd = -1; /* 接收服务器端socket描述符 */
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror ("socket");
		exit(-1);
	}
	/* 2.网络属性设置 */
	/* 2.1允许绑定地址快速重用 */
	int b_reuse = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &b_reuse, sizeof (int));
	/* 3.服务器绑定IP和端口 */
	/* 3.1填充struct sockaddr_in结构体变量 */
	struct sockaddr_in sin;
	bzero (&sin, sizeof(sin)); /* 将内存块（字符串）的前n个字节清零 */
	sin.sin_family = AF_INET;   /* 协议族 */
	sin.sin_port = htons(serverPort); /* 网络字节序的端口号 */
	if(inet_pton(AF_INET, serverIP, (void *)&sin.sin_addr) != 1)/* IP地址 */
	{
		perror("inet_pton");
		return -1;
	}
	/* 3.2绑定 */
	if(bind(socket_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		perror("bind");
		return -1;
	}
	/*4.调用listen()把主动套接字变成被动套接字 */
	if (listen(socket_fd, 5) < 0)
	{
		perror("listen");
		return -1;
	}
	printf("server(\033[1;32m%s:%d\033[0m) staring...OK![socket_fd=%d]\n", serverIP, serverPort, socket_fd);
	return socket_fd;
}

/*=================================================*/
/**
 * @Function: clientDataHandle
 * @Description: 子进程处理客户端数据函数
 * @param arg : 得到客户端连接产生新的套接字文件描述符
 * @param db  : 数据库句柄
 * @param client_info : 客户端IP和端口信息
 * @return  : 返回一个整数
 *            0,数据处理完毕;
 */
int clientDataHandle(void *arg, sqlite3 * db, CLIENT_INFO client_info)
{
	int accept_fd = *(int *) arg; /* 表示客户端通信的新的socket fd */
	MSG msg; /* 与客户端通信的数据信息结构体变量 */
	printf ("client(\033[1;32m%s:%d\033[0m) data handler process(PID:%d).[accept_fd =%d]\n", client_info.ipv4_addr, client_info.port, getpid(), accept_fd);
	/* 数据读写 */
	while( recv(accept_fd, &msg, sizeof(msg), 0) > 0)
	{
		printf("\033[1;35m[client(%s:%d)]:\033[0m msg type:%d\n", client_info.ipv4_addr, client_info.port, msg.type);
		switch(msg.type)
		{
		case R:/* 用户账户注册 */
			serverRegister(accept_fd, &msg, db);
			break;
		case U:/* 用户账户密码修改 */
			serverUpdate(accept_fd, &msg, db);
			break;
		case D:/* 用户账户删除 */
			serverDelete(accept_fd, &msg, db);
			break;
		case S:/* 用户账户显示 */
			serverToClientUserShow(accept_fd, &msg, db);
			break;
		case L:/* 用户账户登录 */
			serverLogin(accept_fd, &msg, db);
			break;
		case Q:/* 单词查询 */
			serverQuery(accept_fd, &msg, db);
			break;
		case H:/* 历史记录查询 */
			serverHistory(accept_fd, &msg, db);
			break;
		default:
			printf("Invalid data msg.\n");
			break;
		}
	}
	printf ("client(\033[1;32m%s:%d\033[0m) exit ... [accept_fd =%d]\n", client_info.ipv4_addr, client_info.port, accept_fd);

	/* 6.关闭文件描述符 */
	close(accept_fd);
	return 0;
}
/*=================================================*/
/**
 * @Function: sigChildHandle
 * @Description: 通过信号回收子进程
 * @param signo: 检测到的信号(子进程结束时会发出SIGCHLD信号)
 * @return  : none
 */
void sigChildHandle(int signo)
{
	if(signo == SIGCHLD)
	{
		waitpid(-1, NULL,  WNOHANG);
	}
}
/*=================================================*/

/**
 * @Function: userShowCallBack
 * @Description:  客户端root用户申请查看所有账户
 * @param arg1 : arg1
 * @param arg1 : arg2
 * @return  :
 *          value1:
 *          value2:
 */

// 得到查询结果，并且需要将历史记录发送给客户端
int userShowCallBack(void* arg, int f_num, char ** f_value, char ** f_name)
{
	/* record  , name  , date  , word */
	int accept_fd;
	MSG msg;

	accept_fd = *((int *)arg);

	sprintf(msg.data, "%-8s, %-11s , %-10s", f_value[0], f_value[1], f_value[2]);

	send(accept_fd, &msg, sizeof(MSG), 0);

	return 0;
}
/**
 * @Function: serverToClientUser
 * @Description: 历史记录查询
 * @param arg: 得到客户端连接产生新的套接字文件描述符
 * @param db : 数据库句柄
 * @return  : 返回一个整数
 *            0,显示完毕;
 *            -1,显示失败
 */
int serverToClientUserShow(int accept_fd, MSG * msg, sqlite3 * db)
{
	int ret = -1;
	char sql[128] = {};
	char *errmsg;
	sprintf(sql, "select * from user");
	//查询数据库
	if(sqlite3_exec(db, sql, userShowCallBack,(void *)&accept_fd, &errmsg)!= SQLITE_OK)
	{
		printf("%s\n", errmsg);
		ret = -1;
	}
	else
	{
		printf("send user information done.\n");
		ret = 0;
	}

	// 所有的记录查询发送完毕之后，给客户端发出一个结束信息
	msg->data[0] = '\0';
	send(accept_fd, msg, sizeof(MSG), 0);
	serverUserShow(db);
	return ret;
}

/**
 * @Function: serverUserShow
 * @Description: 数据库数据显示
 * @param db : 指向数据库的句柄
 * @return  : 返回一个整数
 *            0,数据显示完成;
 */
int serverUserShow(sqlite3 *db)
{
	char *errmsg;
	char ** resultp;
	int nrow;
	int ncolumn;

	if(sqlite3_get_table(db, "select * from user", &resultp, &nrow, &ncolumn, &errmsg) != SQLITE_OK)
	{
		printf("sqlite3_get_table: %s\n", errmsg);
		return -1;
	}
	else
	{
		printf("\n\033[1m------------------- user information ----------------------\033[0m\n");
	}

	int i = 0;
	int j = 0;
	int index = ncolumn;

	for(j = 0; j < ncolumn; j++)
	{
		printf("\033[1;32m%-10s  \033[0m", resultp[j]);
	}
	puts("");

	for(i = 0; i < nrow; i++)
	{
		for(j = 0; j < ncolumn; j++)
		{
			printf("%-11s ", resultp[index++]);
		}
		puts("");
	}
	printf("\033[1m-----------------------------------------------------------\033[0m\n");
	return 0;
}
/*=================================================*/
/**
 * @Function: serverRegister
 * @Description: 处理客户端用户注册请求，进行用户注册
 * @param accept_fd : 客户端连接产生新的套接字文件描述符
 * @param msg       : 服务器与客户端通信的数据结构体指针变量
 * @param db        : 数据库句柄
 * @return  : 返回一个整数
 *            0,用户名已存在;
 *            1,注册成功;
 *            -1发送信息失败
 */
int serverRegister(int accept_fd, MSG *msg, sqlite3 *db)
{
	int ret = -1;
	char * errmsg;/* 保存错误信息 */
	char sql[512];/* 数据库操作命令缓冲区 */
	/* 生成 sqlist3 命令 */
	sprintf(sql, "insert into user values('%s', '%s', %d);", msg->name, msg->data, msg->flag);
	// printf("sqlite3 command: %s\n", sql);
	/* 执行 sqlite3 命令 */
	if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)/* 用户名已存在会导致插入失败 */
	{
		printf("\033[1;31m[error]sqlite3_exec error:%s\033[0m\n", errmsg);
		strcpy(msg->data, "user name already exist.");
		ret = 0;
	}
	else /* 注册成功 */
	{
		printf("\033[1;32mclient register ok!\033[0m\n");
		strcpy(msg->data, "OK!");
		ret = 1;
	}
	/* 回应客户端 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror("send");
		ret = -1;
	}
	/* 显示当前所有的用户名和密码 */
	serverUserShow(db);
	return ret;
}
/*=================================================*/
/**
 * @Function: serverLogin
 * @Description: 客户端用户登录
 * @param accept_fd : 得到客户端连接产生新的套接字文件描述符
 * @param msg       : 与客户端通信的数据结构体指针变量与客户端通信的数据结构体指针变量
 * @param db        : 数据库句柄
 * @return  : 返回一个整数
 *            0,用户名或者密码错误，登陆失败;
 *            1,用户名和密码存在，登陆成功;
 *            -1,发送数据失败
 */
int serverLogin(int accept_fd, MSG * msg, sqlite3 * db)
{
	int ret = -1;
	char * errmsg;/* 保存错误信息 */
	char sql[512];/* 数据库操作命令缓冲区 */

	int nrow;     /* 行 */
	int ncloumn;  /* 列 */
	char **resultp;/* 查询结果 */
	/* 生成sqlite3查询命令 */
	sprintf(sql, "select * from user where name = '%s' and passwd = '%s';", msg->name, msg->data);
	printf("sqlite3 command: %s\n", sql);
	/* 查询数据库 */
	if(sqlite3_get_table(db, sql, &resultp, &nrow, &ncloumn, &errmsg)!= SQLITE_OK)
	{
		printf("sqlite3_get_table: %s\n", errmsg);
	}

	/* 查询成功，数据库中拥有此用户 */
	if(nrow == 1)/* 设置了主键，用户存在的话，行数只能是1 */
	{
		strcpy(msg->data, "OK!The user name or password is correct.");
		ret = 1;
	}
	/* 查询失败，没有符合条件的记录，密码或者用户名错误 */
	if(nrow == 0)
	{
		strcpy(msg->data,"Failed!The user name or password is incorrect.");
		ret = 0;
	}
	/* 发送回应信息 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror("send");
		ret = -1;
	}
	return ret;
}

/*=================================================*/
/**
 * @Function: serverGetData
 * @Description: 获取查询时间字符串
 * @param data : 指向获取到的当前时间所存放的字符串
 * @return  : 返回一个整数
 *            0,获取成功;
 */
int serverGetDate(char * date)
{
	time_t sec; /* 存储当前时间总秒数 */
	struct tm nowTime;/* 存储转换后的时间 */
	/* 获取日历时间 */
	sec = time(NULL);
	/* 将得到的秒数进行处理，获取时间字符串 */
	localtime_r(&sec, &nowTime);/* 得到本地时间 */
	sprintf(date, "%d-%d-%d %d:%d:%d", nowTime.tm_year + 1900, nowTime.tm_mon + 1, nowTime.tm_mday,
	        nowTime.tm_hour, nowTime.tm_min, nowTime.tm_sec);
	printf("\033[1;32mserver get date:[%s]\033[0m\n", date);
	return 0;
}
/**
 * @Function: serverSearchWord
 * @Description: 单词查询
 * @param accept_fd : 得到客户端连接产生新的套接字文件描述符
 * @param msg       : 与客户端通信的数据结构体指针变量
 * @param word      : 要查询的单词
 * @return  : 返回一个整数
 *            0,未找到单词;
 *            1,已找到单词;
 *            -1,函数出错
 */
int serverSearchWord(int accept_fd, MSG * msg, char word[])
{
	FILE * fp; /* 定义一个文件指针 */
	int word_len = 0; /* 单词长度 */
	char temp[512] = {};/* 单词释义缓冲区 */
	int ret = -1;
	char *p;
	/* 打开单词库文件 */
	if((fp = fopen("dict.txt", "r")) == NULL)
	{
		perror("fopen");
		strcpy(msg->data, "Failed to open the dict.txt");
		if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
		{
			perror("send");
		}
		return -1;
	}
	/* 打印一下我们要查询的单词 */
	word_len = strlen(word);
	printf("word: %s , word_len = %d\n", word, word_len);
	/* 按行读取文件，逐行比对查找单词 */
	while(fgets(temp, 512, fp) != NULL)
	{
		ret = strncmp(temp, word, word_len);
		if(ret > 0 || ((ret == 0) && (temp[word_len]!=' ')))
			break; /* 未找到单词 */
		if(ret < 0)
			continue;
		/* 查找到单词，开始进行处理 */
		p = temp + word_len;
		while(*p == ' ')/* 跳过空格 */
		{
			p++;
		}
		strcpy(msg->data, p);
		printf("server found word: %s\n", msg->data);
		fclose(fp);
		return 1;
	}
	fclose(fp);
	return 0;
}

/**
 * @Function: serverQuery
 * @Description: 单词查询
 * @param accept_fd : 得到客户端连接产生新的套接字文件描述符
 * @param msg       : 与客户端通信的数据结构体指针变量
 * @param db        : 数据库句柄
 * @return  : 返回一个整数
 *            0,;
 *            -1,
 */
int serverQuery(int accept_fd, MSG * msg, sqlite3 * db)
{
	int ret = -1;
	char date[128] = {};
	char sql[128] = {};
	char *errmsg;
	/* 获取要查询的单词 */
	char word[64];
	strcpy(word, msg->data);
	/* 查询单词 */
	int found = -1;
	found = serverSearchWord(accept_fd, msg, word);
	printf("A word query has been completed.\n");
	/* 找到了单词，那么此时应该将 用户名，单词，时间，插入到历史记录表中去 */
	if(found == 1)
	{
		/* 获取系统时间 */
		serverGetDate(date);
		/* 生成sqlite3插入命令 */
		sprintf(sql, "insert into history values('%s', '%s', '%s')", msg->name, word, date);

		if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
		{
			printf("%s\n", errmsg);
			ret = -1;
		}
		else
		{
			printf("Insert history done.\n");
			ret = 0;
		}

	}
	else  //表示没有找到
	{
		strcpy(msg->data, "Not found!");
	}
	if(send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror("send");
		ret = -1;
	}
	return ret;
}

/*=================================================*/
/**
 * @Function: historyCallBack
 * @Description:  历史记录显示回调函数
 * @param arg1 : arg1
 * @param arg1 : arg2
 * @return  :
 *          value1:
 *          value2:
 */

// 得到查询结果，并且需要将历史记录发送给客户端
int historyCallBack(void* arg, int f_num, char ** f_value, char ** f_name)
{
	/* record  , name  , date  , word */
	int accept_fd;
	MSG msg;

	accept_fd = *((int *)arg);

	sprintf(msg.data, "%-8s, %-11s , %-10s", f_value[0], f_value[1], f_value[2]);

	send(accept_fd, &msg, sizeof(MSG), 0);

	return 0;
}

/**
 * @Function: serverHistory
 * @Description: 历史记录查询
 * @param arg: 得到客户端连接产生新的套接字文件描述符
 * @param db : 数据库句柄
 * @return  : 返回一个整数
 *            0,数据处理完毕;
 *            -1,启动失败
 */
int serverHistory(int accept_fd, MSG * msg, sqlite3 * db)
{
	char sql[128] = {};
	char *errmsg;
	if(msg->flag == 1)/* root 用户	*/
		sprintf(sql, "select * from history");
	else
		sprintf(sql, "select * from history where name = '%s'", msg->name);
	//查询数据库
	if(sqlite3_exec(db, sql, historyCallBack,(void *)&accept_fd, &errmsg)!= SQLITE_OK)
	{
		printf("%s\n", errmsg);
	}
	else
	{
		printf("Query record done.\n");
	}

	// 所有的记录查询发送完毕之后，给客户端发出一个结束信息
	msg->data[0] = '\0';
	send(accept_fd, msg, sizeof(MSG), 0);

	return 0;
}

/**
 * @Function: serverUpdate
 * @Description: 用户密码修改
 * @param accept_fd : 得到客户端连接产生新的套接字文件描述符
 * @param msg       : 与客户端通信的数据结构体指针变量与客户端通信的数据结构体指针变量
 * @param db        : 数据库句柄
 * @return  : 返回一个整数
 *            0,修改成功;
 *            -1,修改失败;
 */
int serverUpdate(int accept_fd, MSG * msg, sqlite3 * db)
{
	int ret = -1;
	char * errmsg;/* 保存错误信息 */
	char sql[512];/* 数据库操作命令缓冲区 */
	/* 查询输入的用户名是否存在 */
	int nrow;     /* 行 */
	int ncloumn;  /* 列 */
	char **resultp;/* 查询结果 */
	/* 生成sqlite3查询命令 */
	sprintf(sql, "select * from user where name = '%s';", msg->name);
	/* 查询数据库 */
	if(sqlite3_get_table(db, sql, &resultp, &nrow, &ncloumn, &errmsg)!= SQLITE_OK)
	{
		printf("sqlite3_get_table: %s\n", errmsg);
	}
	/* 查询失败，没有符合条件的记录，密码或者用户名错误 */
	if(nrow == 0)
	{
		strcpy(msg->data,"The user name is incorrect!");
		ret = -1;
	}
	else
	{
		/* 若存在则更新数据 */
		sprintf(sql, "update user set passwd='%s' where name='%s'", msg->data, msg->name);
		printf("sqlite3 command: %s\n", sql);
		bzero(msg->data, strlen(msg->data));/* 清空msg->data中原来的数据 */
		if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
		{
			printf("\033[1;32m[error]sqlite3_exec: %s\033[0m\n", errmsg);
			ret = -1;
		}
		else
		{
			printf("\033[1;32m[ OK  ]Update done!\033[0m\n");
			strcpy(msg->data, "OK!");
		}
	}
	/* 发送回应信息 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror("send");
		ret = -1;
	}
	return ret;
}

/**
 * @Function: serverDelete
 * @Description: 用户账户删除
 * @param accept_fd : 得到客户端连接产生新的套接字文件描述符
 * @param msg       : 与客户端通信的数据结构体指针变量与客户端通信的数据结构体指针变量
 * @param db        : 数据库句柄
 * @return  : 返回一个整数
 *            0,删除成功;
 *            -1,删除失败;
 */
int serverDelete(int accept_fd, MSG * msg, sqlite3 * db)
{
	int ret = -1;
	char * errmsg;/* 保存错误信息 */
	char sql[512];/* 数据库操作命令缓冲区 */
	/* 查询输入的用户名是否存在 */
	int nrow;     /* 行 */
	int ncloumn;  /* 列 */
	char **resultp;/* 查询结果 */
	/* 生成sqlite3查询命令 */
	sprintf(sql, "select * from user where name = '%s';", msg->name);
	/* 查询数据库 */
	if(sqlite3_get_table(db, sql, &resultp, &nrow, &ncloumn, &errmsg)!= SQLITE_OK)
	{
		printf("sqlite3_get_table: %s\n", errmsg);
	}
	/* 查询失败，没有符合条件的记录，密码或者用户名错误 */
	if(nrow == 0)
	{
		strcpy(msg->data,"The user name is incorrect!");
		ret = -1;
	}
	else
	{
		/* 若存在则删除数据 */
		sprintf(sql, "delete from user where name='%s'", msg->name);
		printf("sqlite3 command: %s\n", sql);
		bzero(msg->data, strlen(msg->data));/* 清空msg->data中原来的数据 */
		if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK)
		{
			printf("\033[1;32m[error]sqlite3_exec: %s\033[0m\n", errmsg);
			ret = -1;
		}
		else
		{
			printf("\033[1;32m[ OK  ]Delete done!\033[0m\n");
			strcpy(msg->data, "OK!");
		}
	}
	/* 发送回应信息 */
	if( send(accept_fd, msg, sizeof(MSG), 0) < 0)
	{
		perror("send");
		ret = -1;
	}
	return ret;
}
