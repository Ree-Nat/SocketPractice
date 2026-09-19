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
#include <stdbool.h>
#include <unistd.h>


#define BUFSIZE 1024
#define IPSIZE 46
#define HTTP_PORT 80
#define HTTP_SUCCESS 301
//test

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

struct myargs args; //global args

int parseResponseStruct(ArrayListBuf *responseStruct);


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

        if (nodes->ai_family == AF_INET)
        {
            struct sockaddr_in* ipdata = (struct sockaddr_in*)nodes->ai_addr;
            inet_ntop(nodes->ai_family, &ipdata->sin_addr, ip, IPSIZE);
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

ArrayListBuf *getResponseStruct(int fd)
{
    ArrayListBuf response;
    ArrayListBuf_init(&response);
    char recv_buffer[512];
    int responseNumber;

    while((responseNumber = (recv(fd, recv_buffer, sizeof(recv_buffer), 0))) != 0)
    {
        if(responseNumber < 0)
        {
            if (errno == EINTR) continue;
            fprintf(stderr, "Error while parsing response");
            exit(0);
        }
        ArrayListBuf_push(&response, recv_buffer, responseNumber);
    }

    ArrayListBuf *responsePointer = &response;

    return responsePointer;
}


int inititateTCP(struct IpNode* ipList, struct myargs domainLink)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
    {
        fprintf(stderr, "Socket creation failed: %d", sockfd);
        exit(0);
    }
    bool connected = false;
    struct sockaddr_in serv_addr; //ip4
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(HTTP_PORT);

    /*goes through link list parsed from get addr, returns error if no connection
    initiated.*/
    while(ipList->next != NULL && connected == false)
    {
        if (inet_pton(AF_INET, ipList->ipAddr, &serv_addr.sin_addr) <= 0)
        {
            fprintf(stderr, "\n Invalid adress");
            exit(0);
        }
        
        int status;
        if((status = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0)
        {
            printf("\n Address %s connection failed, trying next ip4 adress", ipList->ipAddr);
            ipList = ipList->next;
        }
        else {connected = true; printf("connected to: %s", ipList->ipAddr);}
    }
    if(connected != true){fprintf(stderr, "Connecting socket failed"); exit(0);}

        char requestBuffer[200];
        int req = snprintf(requestBuffer, sizeof(requestBuffer), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", domainLink.path, domainLink.domain);
        if(req < 0)
        {
            fprintf(stderr, "Write failed, buffer is too long");
        }

        int msgLen = strlen(requestBuffer);

        size_t total_sent = 0; //using unsigned interger 
       

        while(total_sent < msgLen)
        {
             ssize_t msgSent_size = send(sockfd, &requestBuffer[total_sent], msgLen - total_sent, 0);
             if(msgSent_size < 0)
             {
                if (errno == EINTR) continue; //interrupt signal over socket, wait
                fprintf(stderr, "Failed to send GET request to msg");
             }
             total_sent += msgSent_size;

        }
        fprintf(stdout, "Connection Successful, now intepreting http get:\n");
        ArrayListBuf *responseStruct = getResponseStruct(sockfd);
        int code = parseResponseStruct(responseStruct);
        close(sockfd);
        return code; 
    }

int parseResponseStruct(ArrayListBuf *responseStruct)
{
    char *responseBuff = responseStruct->buff;
    int responseLength = responseStruct->N;
    int responseCode = 0;
    char *charPointer;

    //Get rid of leftover values from arraylist buffer
    char *parseResponse = malloc(sizeof(char) * responseStruct->N + 1);
    if(parseResponse == NULL){fprintf(stderr, "Error when malloc parse response");exit(0);}
    strncpy(parseResponse, responseBuff, responseStruct->N);
    parseResponse[responseLength] = '\0';

    char *parsePointer = parseResponse;
    sscanf(parsePointer, "HTTP/%*d.%*d %3d", &responseCode);
    parsePointer = strstr(parsePointer, "\r\n\r\n"); //go down to body
    parsePointer += 4; //go down a line

    char nameBuffer[50];
    int res = sprintf(nameBuffer, "%s/http_output.bin", args.target);
    if(res < 0)
    {
        fprintf(stderr, "path name to long");
    }

    if(responseCode == HTTP_SUCCESS)
    {
        FILE *binFile = fopen(nameBuffer, "w+");
        fwrite(parsePointer, sizeof(parsePointer), strlen(parsePointer), binFile);
        fclose(binFile);
    }
    return responseCode;
}




int main(int argc, char** argv) {
    args = parseArgs(argc, argv);
    //create socket
    struct IpNode* ip4_list = getIpAdress(args.domain, AF_INET);
    
    int result = inititateTCP(ip4_list, args);

    if(result == HTTP_SUCCESS)
    {
        printf("\n Connection and Parsing successful, now outputing file thi %s", args.path);
        
    }
    else
    {
        printf("HTTP request not OK. HTTP Response code: %d", result);
    }

    return 0;
    
}
