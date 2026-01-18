#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#define BUF_SIZE 4096
#define HOST "wttr.in"
#define PORT 80
#define PATH "/Toronto?format=3"
#define TIMEOUT_SECS 10

int main() {
    int sock;
    struct sockaddr_in server;
    struct hostent *hp;
    char buf[BUF_SIZE];
    char request[BUF_SIZE];
    int total = 0;

    printf("[LOG] Creating socket...\n");
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    printf("[LOG] Socket created successfully\n");

    printf("[LOG] Resolving hostname: %s\n", HOST);
    hp = gethostbyname(HOST);
    if (!hp) { fprintf(stderr, "Unknown host\n"); return 1; }
    printf("[LOG] Hostname resolved\n");

    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
    server.sin_port = htons(PORT);

    // Print resolved IP address
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &server.sin_addr, ip_str, sizeof(ip_str));
    printf("[LOG] Resolved IP: %s\n", ip_str);

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECS;
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        printf("[WARN] Failed to set receive timeout\n");
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        printf("[WARN] Failed to set send timeout\n");
    }

    printf("[LOG] Connecting to %s:%d (%s)...\n", HOST, PORT, ip_str);
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect"); 
        printf("[ERROR] Connection failed. errno=%d\n", errno);
        close(sock); 
        return 1;
    }
    printf("[LOG] Connected successfully\n");

    // TLS not implemented for QNX simplicity; assumes HTTP/443 works or use stunnel. For prod, add TLS.
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: QNX-Weather/1.0\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n"
        "\r\n", PATH, HOST);

    printf("[LOG] Sending HTTP request...\n");
    if (write(sock, request, strlen(request)) < 0) { perror("write"); close(sock); return 1; }
    printf("[LOG] Request sent, waiting for response...\n");

    // Read response
    int chunk_count = 0;
    while ((total = read(sock, buf, BUF_SIZE - 1)) > 0) {
        chunk_count++;
        printf("[LOG] Received chunk %d: %d bytes\n", chunk_count, total);
        buf[total] = '\0';
        printf("%s", buf);
        if (strstr(buf, "\"features\":[]")) break; // No data
    }
    printf("[LOG] Read complete (received %d chunks). Closing connection...\n", chunk_count);
    close(sock);

    printf("\n--- Toronto Weather Data ---\n");
    return 0;
}
