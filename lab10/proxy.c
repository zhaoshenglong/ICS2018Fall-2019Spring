/*
 * proxy.c - ICS Web proxy
 *
 *
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);

int read_req_path(char *req_path, rio_t *client_rio, int fd);
int read_req_header(char *req_header, rio_t *client_rio, int fd);
int read_req_body(char *req_body, rio_t *client_rio, int fd);
void build_req(char *req, char *path, char *header, char *body);
void send_req(char *req, size_t len, rio_t *server_rio, int fd);
int read_resp_status(char *resp_status, rio_t *server_rio, int fd);
int read_resp_header(char *resp_header, rio_t *server_rio, int fd);
int read_resp_body(char *resp_body, rio_t *server_rio, int fd);
void build_resp(char *resp, char *status, char *header, char *body);
void send_resp(char *resp, size_t len, rio_t *client_rio, int fd);
void doit(int fd);

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    /* Check arguments */
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    int listenfd, connfd;
    int clientlen;
    struct sockaddr_storage clientaddr;

    listenfd = Open_listenfd(argv[1]);

    /* Start listen for the connection */
    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        doit(connfd);
        Close(connfd);
    }
    exit(0);
}

/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, char *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0)
    {
        hostname[0] = '\0';
        return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    if (hostend == NULL)
        return -1;
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    if (*hostend == ':')
    {
        char *p = hostend + 1;
        while (isdigit(*p))
            *port++ = *p++;
        *port = '\0';
    }
    else
    {
        strcpy(port, "80");
    }

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL)
    {
        pathname[0] = '\0';
    }
    else
    {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), the number of bytes
 * from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, size_t size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /*
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 12, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %zu", time_str, a, b, c, d, uri, size);
}

void doit(int fd)
{
    int proxyfd;
    int reqlen, resplen, n;
    char req_path[MAXLINE], req_header[MAXLINE], method[MAXLINE],
        url[MAXLINE], version[MAXLINE], buf[MAXLINE];
    char server_ip[MAXLINE], server_port[MAXLINE], server_uri[MAXLINE];
    rio_t client_rio, server_rio;

    Rio_readinitb(&client_rio, fd);
    read_req_path(req_path, &client_rio, fd);
    sscanf(req_path, "%s %s %s", method, url, version);
    parse_uri(url, server_ip, server_uri, server_port);

    /* Read header */
    read_req_header();

    proxyfd = Open_clientfd(server_ip, server_port);
}