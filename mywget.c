#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include "arraylist.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>

#define BUFSIZE 1024
#define IPSIZE 46

struct myargs {
    char* url;
    char domain[BUFSIZE]; // Domain of URL
    char path[BUFSIZE]; // Path of URL
    char* port; // 80 by default
    char* target;
    struct timeval timeout; // 10 (second) by default
};

struct IpNode
{
    char ipAddr[IPSIZE];
    struct IpNode *next;
};


/**
 * @brief Separate a URL out into the domain part and the path part
 * 
 * @param url Pointer to a URL string
 * @param domain Pointer to a domain string to which to write result, 
 *               assumed to be BUFSIZE large
 * @param path Pointer to path string to which to write result,
 *               assumed to be BUFSIZE large.  If there is no path, put
 *               a "/"
 */
void parseURL(char* url, char* domain, char* path) {
    char* httpStr = "http://";
    int len = strlen(url);
    memset(domain, '\0', BUFSIZE);
    memset(path, '\0', BUFSIZE);
    if (strncmp(url, httpStr, strlen(httpStr)) == 0) {
        // Skip over any http:// at the front
        url += strlen(httpStr);
    }
    int idxSep = 0;
    while (idxSep < len && url[idxSep] != '/') {
        idxSep++;
    }

    if (idxSep+1 > BUFSIZE) {
        fprintf(stderr, "ERROR: Domain part of URL exceeds %i bytes", BUFSIZE);
        exit(0);
    }
    strncpy(domain, url, idxSep);

    if (len-idxSep+1 > BUFSIZE) {
        fprintf(stderr, "ERROR: Path part of URL exceeds %i bytes", BUFSIZE);
        exit(0);
    }
    if (idxSep == len) {
        // No path specified; default to "/"
        path[0] = '/';
    }
    else {
        strncpy(path, url+idxSep, len-idxSep+1);
    }
}

/**
 * @brief Parse command line arguments for the HTTP client
 */
struct myargs parseArgs(int argc, char** argv) {
    struct myargs ret;
    // Step 1: Setup default values
    ret.url = "";
    ret.port = "80";
    ret.timeout.tv_sec = 10;
    ret.timeout.tv_usec = 0;
    ret.target = "";

    // Step 2: Parse user specified values
    // Advance to the next element
    char* programName = argv[0];
    argv++; 
    argc--; 
    while (argc > 0) {
        if((*argv)[0] == '-') {
            if (strcmp(*argv, "--help") == 0) {
                printf("Usage: %s --url <url of file>", programName);
                printf(" --target <target filename to save>");
                printf(" [--port <port number>] [--timeout <timeout>]\n");
                exit(0);
            }
            else if (strcmp(*argv, "--url") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.url = *argv;
                    parseURL(*argv, ret.domain, ret.path);
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --url\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--port") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.port = *argv;
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --port\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--target") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.target = *argv;
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --path\n");
                    exit(0);
                }
            }
            else if (strcmp(*argv, "--timeout") == 0) {
                argv++; argc--;
                if (argc > 0) {
                    ret.timeout.tv_sec = atol(*argv);
                }
                else {
                    fprintf(stderr, "Error: Expecting field after --timeout\n");
                    exit(0);
                }
            }

        }
        else {
            fprintf(stderr, "Warning: Unrecognized field %s\n", *argv);
        }
        argv++; argc--;
    }

    // Step 3: Check for required values
    if (strcmp(ret.url, "") == 0) {
        fprintf(stderr, "Error: Require a --url to be specified\n");
        exit(0);
    }
    if (strcmp(ret.target, "") == 0) {
        fprintf(stderr, "Error: Require a --target to be specified\n");
        exit(0);
    }

    return ret;
}



struct IpNode* getIpAdress(char* domainName, int ai_family)
{
    struct addrinfo hints;
    struct addrinfo* nodes;
    struct IpNode* head = NULL;

    head = (struct IpNode*) malloc(sizeof(struct IpNode));

    memset(&hints, 0, sizeof(struct  addrinfo));
  
    hints.ai_family = AF_UNSPEC; //use either ip4 or ip6
    hints.ai_socktype = SOCK_STREAM;
    int addr_ret = getaddrinfo(domainName, "80", &hints, &nodes);
    if(addr_ret != 0)
    {
        fprintf(stderr, "Cannot get addrinfo, error code: %d", addr_ret);
    }


    while (nodes != NULL)
    {
        char ip[IPSIZE];
        ip[0] = 0;

        if (nodes->ai_family == AF_INET && ai_family == AF_INET)
        {
            struct sockaddr_in* ipdata = (struct sockaddr_in*)nodes->ai_addr;
            inet_ntop(nodes->ai_family, &ipdata->sin_addr, ip, IPSIZE);
        }
        else if (nodes->ai_family == AF_INET6 && ai_family == AF_INET6)
        {
            struct sockaddr_in6* ipdata = (struct sockaddr_in6*)  nodes->ai_addr;
            inet_ntop(nodes->ai_family, &ipdata->sin6_addr, ip, IPSIZE);
        }

        if(ip[0] != 0){
            struct IpNode* newNode = (struct IpNode*) malloc(sizeof(struct IpNode));
            strcpy(newNode->ipAddr, ip);
            newNode->next = head;
            head = newNode;
        }
        nodes = nodes->ai_next;
    }
        freeaddrinfo(nodes);
        return head;
    }


int main(int argc, char** argv) {
    struct myargs args = parseArgs(argc, argv);

    //create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    
    char name[] = "www.drudgereport.com";

    struct IpNode* ip4_list = getIpAdress(args.domain, AF_INET);
    struct IpNode* ip6_list = getIpAdress(args.domain, AF_INET6);

    while(ip4_list->next != NULL)
    {
        printf("%s\n", ip4_list->ipAddr);
        ip4_list = ip4_list->next;
    }

    while(ip6_list->next != NULL)
    {
        printf("%s\n", ip6_list->ipAddr);
        ip6_list = ip6_list->next;
    }
    
}
