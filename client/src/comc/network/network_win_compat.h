#ifndef NETWORK_WIN_COMPAT_H
#define NETWORK_WIN_COMPAT_H

typedef unsigned long DWORD;
typedef unsigned long long ULONG_PTR;
typedef ULONG_PTR SOCKET;
typedef unsigned long long SIZE_T;

struct sockaddr;

typedef struct {
    unsigned short wVersion;
    unsigned short wHighVersion;
    unsigned char szDescription[257];
    unsigned char szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    unsigned char *lpVendorInfo;
} WSADATA;

typedef struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    SIZE_T ai_addrlen;
    char *ai_canonname;
    struct sockaddr *ai_addr;
    struct addrinfo *ai_next;
} addrinfo;

#define INVALID_SOCKET ((SOCKET)(~(SOCKET)0))
#define SOCKET_ERROR   (-1)

#define AF_UNSPEC      0
#define SOCK_STREAM    1
#define IPPROTO_TCP    6

#define SD_BOTH        2

#define MAKEWORD(a,b) \
    ((unsigned short)((((unsigned short)(a)) & 0xff) | \
    ((((unsigned short)(b)) & 0xff) << 8)))

int WSAStartup(unsigned short version, WSADATA *data);
int WSACleanup(void);
int WSAGetLastError(void);

SOCKET socket(int af, int type, int protocol);

int connect(SOCKET s, const struct sockaddr *name, int namelen);

int send(SOCKET s, const char *buf, int len, int flags);
int recv(SOCKET s, char *buf, int len, int flags);

int shutdown(SOCKET s, int how);
int closesocket(SOCKET s);

int getaddrinfo(
    const char *node,
    const char *service,
    const struct addrinfo *hints,
    struct addrinfo **result
);

void freeaddrinfo(struct addrinfo *res);

#endif
