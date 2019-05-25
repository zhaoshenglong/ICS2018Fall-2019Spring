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
int read_req_header(char *req_header, rio_t *client_rio, int fd, int *bodylen);
void build_req_header(char *req, char *method, char *uri, char *version, char *header);
int send_req(char *req, size_t len, int fd);
int read_resp_status(char *resp_status, rio_t *server_rio, int fd);
int read_resp_header(char *resp_header, rio_t *server_rio, int fd, int *bodylen);
void build_resp_header(char *resp, char *status, char *header);
int send_resp(char *resp, size_t len, int fd);
void doit(int fd, struct sockaddr_in *clientaddr);

/* Wrapper Function */
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxline);
ssize_t Rio_writen_w(int fd, void *usrbuf, size_t n);

void *thread(void *vargp);

struct th_arg
{
    int connfd;
    struct sockaddr_in clientaddr;
};

sem_t sem;
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
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    struct th_arg *vargp;
    pthread_t tid;
    Sem_init(&sem, 0, 1);

    listenfd = Open_listenfd(argv[1]);

    /* Start listen for the connection */
    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        vargp = Malloc(sizeof(struct th_arg));
        vargp->connfd = connfd;
        vargp->clientaddr = *(struct sockaddr_in *)&clientaddr;
        Pthread_create(&tid, NULL, thread, vargp);
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

void *thread(void *vargp)
{
    int connfd = ((struct th_arg *)vargp)->connfd;
    struct sockaddr_in clientaddr = ((struct th_arg *)vargp)->clientaddr;
    Pthread_detach(Pthread_self());
    Free(vargp);
    doit(connfd, &clientaddr);
    Close(connfd);
    return NULL;
}

void doit(int fd, struct sockaddr_in *clientaddr)
{
    int proxyfd;
    int req_bodylen = 0, resp_bodylen = 0, resp_headerlen = 0;
    char req_path[MAXLINE], req_header[MAXLINE], method[MAXLINE],
        url[MAXLINE], version[MAXLINE], resp_status[MAXLINE],
        resp_header[MAXLINE], logstr[MAXLINE], req[MAXLINE], resp[MAXLINE];
    char server_ip[MAXLINE], server_port[MAXLINE], server_uri[MAXLINE];
    rio_t client_rio, server_rio;

    Rio_readinitb(&client_rio, fd);
    if (read_req_path(req_path, &client_rio, fd) == 0)
    {
        /* Read none path */
        return;
    }
    sscanf(req_path, "%s %s %s", method, url, version);
    if (parse_uri(url, server_ip, server_uri, server_port) != 0)
    {
        return;
    }

    /* Read header */
    if (read_req_header(req_header, &client_rio, fd, &req_bodylen) == 0)
    {
        /* Read none header */
        return;
    }

    build_req_header(req, method, server_uri, version, req_header);

    proxyfd = Open_clientfd(server_ip, server_port);

    Rio_readinitb(&server_rio, proxyfd);
    if (send_req(req, strlen(req), proxyfd) == 0)
    {
        Close(proxyfd);
        return;
    }

    if (req_bodylen > 0)
    {
        int tmp = req_bodylen, n;
        while (tmp > MAXLINE)
        {
            n = Rio_readnb_w(&client_rio, req, MAXLINE);
            if (n == 0 || n < MAXLINE)
            {
                Close(proxyfd);
                return;
            }
            send_req(req, n, proxyfd);
            tmp -= n;
        }
        n = Rio_readnb_w(&client_rio, req, tmp);
        if (n == 0 || n < tmp)
        {
            Close(proxyfd);
            return;
        }
        send_req(req, n, proxyfd);
    }
    if (read_resp_status(resp_status, &server_rio, proxyfd) == 0)
    {
        Close(proxyfd);
        return;
    }
    if (read_resp_header(resp_header, &server_rio, proxyfd, &resp_bodylen) == 0)
    {
        Close(proxyfd);
        return;
    }
    build_resp_header(resp, resp_status, resp_header);
    resp_headerlen = strlen(resp);
    if (send_resp(resp, resp_headerlen, fd) == 0)
    {
        /* Send none */
        return;
    }

    if (resp_bodylen > 0)
    {

        int tmp = resp_bodylen, n;

        /*
         * NOTE: This is a tricky way.
         * The response will be cut by server, so you can not predict the 
         * response length each time.
         * So the following way is a tricky way which always receive 1 byte.
         * It seems if you Rio_readnb to long, it enter into dead loop
         * 
         */
        while (tmp > 0)
        {
            n = Rio_readnb_w(&server_rio, resp, 1);
            if (n <= 0 && tmp != 0)
            {
                Close(proxyfd);
                return;
            }
            n = send_resp(resp, n, fd);
            if (n <= 0)
            {
                Close(proxyfd);
                return;
            }
            tmp -= 1;
        }
    }

    format_log_entry(logstr, clientaddr, url, resp_headerlen + resp_bodylen);
    P(&sem);
    printf("%s\n", logstr);
    V(&sem);
    Close(proxyfd);
}

int read_req_path(char *req_path, rio_t *client_rio, int fd)
{
    return Rio_readlineb_w(client_rio, req_path, MAXLINE);
}

int read_req_header(char *req_header, rio_t *client_rio, int fd, int *bodylen)
{
    int n, len = 0;
    char buf[MAXLINE], *ptr = req_header;
    n = Rio_readlineb_w(client_rio, buf, MAXLINE);
    if (n == 0)
    {
        return 0;
    }
    while (strcmp(buf, "\r\n"))
    {
        strcpy(ptr, buf);
        if (strncasecmp(ptr, "Content-Length", 14) == 0)
        {
            *bodylen = atoi(ptr + 16);
        }
        ptr += n;
        len += n;
        n = Rio_readlineb_w(client_rio, buf, MAXLINE);
        if (n == 0)
        {
            return 0;
        }
    }
    strcpy(ptr, buf);
    ptr += 2;
    *ptr = '\0';
    len += 2;
    return len;
}

void build_req_header(char *req, char *method, char *uri, char *version, char *header)
{
    sprintf(req, "%s /%s %s\r\n%s", method, uri, version, header);
}

int send_req(char *req, size_t len, int fd)
{
    return Rio_writen_w(fd, req, len);
}

int read_resp_status(char *resp_status, rio_t *server_rio, int fd)
{
    return Rio_readlineb_w(server_rio, resp_status, MAXLINE);
}

int read_resp_header(char *resp_header, rio_t *server_rio, int fd, int *bodylen)
{
    int n, len = 0;
    char buf[MAXLINE], *ptr = resp_header;
    n = Rio_readlineb_w(server_rio, buf, MAXLINE);
    if (n == 0)
    {
        return 0;
    }
    while (strcmp(buf, "\r\n"))
    {
        //printf("resp buf:%s\n", buf);
        strcpy(ptr, buf);
        if (strncasecmp(ptr, "Content-Length", 14) == 0)
        {
            *bodylen = atoi(ptr + 16);
            //printf("bodylen:%d\n", *bodylen);
        }
        ptr += n;
        len += n;
        n = Rio_readlineb_w(server_rio, buf, MAXLINE);
        if (n == 0)
        {
            return 0;
        }
    }
    strcpy(ptr, buf);
    ptr += 2;
    *ptr = '\0';
    len += 2;
    return len;
}

void build_resp_header(char *resp, char *status, char *header)
{
    sprintf(resp, "%s%s", status, header);
}

int send_resp(char *resp, size_t len, int fd)
{
    return Rio_writen_w(fd, resp, len);
}

ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n)
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
    {
        fprintf(stderr, "WARNING: %s\n", "Readnb error");
        rc = 0;
    }

    return rc;
}
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxline)
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxline)) < 0)
    {
        fprintf(stderr, "WARNING: %s\n", "Readlineb error");
        rc = 0;
    }
    return rc;
}
ssize_t Rio_writen_w(int fd, void *usrbuf, size_t n)
{
    int rc;
    if ((rc = rio_writen(fd, usrbuf, n)) != n)
    {
        fprintf(stderr, "WARNING: %s\n", "Writen error");
        rc = 0;
    }
    return rc;
}
