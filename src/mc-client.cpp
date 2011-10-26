/* multicast_client.c
 * multicast relay from one interface to the other
 * writen by Øivind Kure october 2011
 * Assume Linux IPv6 not portable to Win or Ipv4
 * Usage mc-client ff02::1 6888 eth01
 * Example based on example code with the following header:
 * This sample demonstrates a Windows multicast client that works with either
 * IPv4 or IPv6, depending on the multicast address given.
 * Requires Windows XP+/Use MSVC and platform SDK to compile.
 * Troubleshoot: Make sure you have the IPv6 stack installed by running
 *     >ipv6 install
 *
 * Usage:
 *     multicast_client multicastip port
 *
 * Examples:
 *     >multicast_client 224.0.22.1 9210
 *     >multicast_client ff15::1 2001
 *
 * Written by tmouse, July 2005
 * http://cboard.cprogramming.com/showthread.php?t=67469
 */
 
#include <stdio.h>      /* for printf() and fprintf() */
//#include <winsock2.h>   /* for socket(), connect(), sendto(), and recvfrom() */
//#include <ws2tcpip.h>   /* for ip_mreq */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <time.h>       /* for timestamps */
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<netdb.h>
#include <unistd.h>
#include<net/if.h>
#include<sys/ioctl.h>


#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#endif
#define INVALID_SOCKET -1

void DieWithError(const char* errorMessage)
{
    fprintf(stderr, "%s\n", errorMessage);
    exit(EXIT_FAILURE);
}


int main(int argc, char* argv[])
{
//    SOCKET     sock;                     /* Socket */
	int sock ;                             // socket
	int r;                                 // result from calls to system functions
	int max_fd = 0;								// max index fd used by select
 //   WSADATA    wsaData;                  /* For WSAStartup */
    char*      multicastIP;              /* Arg: IP Multicast Address */
    char*      multicastPort;            /* Arg: Port */
    char*      interface;                // interfacename for outgoing data
    addrinfo* multicastAddr;            /* Multicast Address */
 //   u_char mcttl = 1;					//sets TTL for multicast
//    struct sockaddr_in6  mc_addr_listen ;
    addrinfo*  localAddr;                /* Local address to bind to */
    addrinfo   hints          = { 0 };   /* Hints for name lookup */
    fd_set rs_set;                    // structure to indicate which socket has data
    sockaddr srcaddr ;			  // to hold src addr of mc datagram
    socklen_t size_srcaddr = sizeof(srcaddr);
    struct ifreq ifr;                 // name of interface

//    if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
//   {
//        DieWithError("WSAStartup() failed");
//    }

    FD_ZERO(&rs_set);

    if ( argc != 4 )
    {
        fprintf(stderr,"Usage: %s <Multicast IP> <Multicast Port> < interface>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
	;
    multicastIP   = argv[1];      /* First arg:  Multicast IP address */
    multicastPort = argv[2];      /* Second arg: Multicast port */
    interface     = argv[3];      // third arg: Interface name

    /* Resolve the multicast group address */
    hints.ai_family = PF_INET6;
    hints.ai_flags  = AI_NUMERICHOST;
    if ( getaddrinfo(multicastIP, NULL, &hints, &multicastAddr) != 0 )
    {
        DieWithError("getaddrinfo() failed");
    }

  //  printf("Using %s\n", multicastAddr->ai_family == PF_INET6 ? "IPv6" : "IPv4");

    /* Get a local address with the same family (IPv4 or IPv6) as our multicast group */
    hints.ai_family   = multicastAddr->ai_family;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags    = AI_PASSIVE; /* Return an address we can bind to */
    if ( getaddrinfo(NULL, multicastPort, &hints, &localAddr) != 0 )
    {
        DieWithError("getaddrinfo() failed");
    }
   
    /* Create socket for receiving datagrams */
    if ( (sock = socket(localAddr->ai_family, localAddr->ai_socktype, 0)) == INVALID_SOCKET )
    {
        DieWithError("socket() failed");
    }

    /* Bind to the multicast port */
    if ( bind(sock, localAddr->ai_addr, localAddr->ai_addrlen) != 0 )
    {
        DieWithError("bind() failed");
    }
    FD_SET(sock,&rs_set);
    if(sock > max_fd) max_fd = sock;
    
   /* bind socket to a particular interface */
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,interface,sizeof(interface));
    if(ioctl(sock,SIOCGIFINDEX,&ifr) <0 ) //Interface index of interface{
    {
    	DieWithError("ioctl() failed to find index  to interface");
    }
    if(setsockopt(sock,SOL_SOCKET,SO_BINDTODEVICE,&ifr,sizeof(ifr)) < 0)
    {
    	DieWithError("setsockopt() failed to bind to interface");
    }

    /* Join the multicast group.  */

        struct ipv6_mreq multicastRequest;  /* Multicast address join structure */

        /* Specify the multicast group */
        memcpy(&multicastRequest.ipv6mr_multiaddr,
               &((struct sockaddr_in6*)(multicastAddr->ai_addr))->sin6_addr,
               sizeof(multicastRequest.ipv6mr_multiaddr));

        /* Accept multicast from any interface */
        multicastRequest.ipv6mr_interface = ifr.ifr_ifindex;

        /* Join the multicast address */
        if ( setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char*) &multicastRequest, sizeof(multicastRequest)) != 0 )
        {
            DieWithError("setsockopt() join group failed");
        }
   //     if ( setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char*) &mcttl, sizeof(mcttl)) != 0 )
   //             {
   //                 DieWithError("setsockopt() ttl failed");
   //             }
//    }
    

 //   freeaddrinfo(localAddr);
 //   freeaddrinfo(multicastAddr);

    while(true) /* Run forever */
    {
        time_t timer;
        char   recvString[1500];      /* Buffer for received string */
        int    recvStringLen;        /* Length of received string */
        if(select(max_fd+1,&rs_set,NULL,NULL,NULL) <0){
        	DieWithError("select failed");
        }
        if(FD_ISSET(sock,&rs_set)){


			/* Receive a single datagram from the server */
			if ( (recvStringLen = recvfrom(sock, recvString, sizeof(recvString) - 1, 0, &srcaddr, &size_srcaddr)) < 0 )
			{
				DieWithError("recvfrom() failed");
			}

			recvString[recvStringLen] = '\0';

			/* Print the received string */
			time(&timer);  /* get time stamp to print with recieved data */
			printf("Time Received: %.*s : %s\n", strlen(ctime(&timer)) - 1, ctime(&timer), recvString);
        }
    }

    /* NOT REACHED */
    r= close(sock);
 //   closesocket(sock);
    exit(EXIT_SUCCESS);
}
