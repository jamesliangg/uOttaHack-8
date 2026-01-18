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
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUF_SIZE 4096
#define HOST "api.weather.gc.ca"
#define PORT 443
#define PATH_BASE "/collections/swob-realtime/items/%s-0000-CYYZ-MAN-swob.xml?lang=en"
#define TIMEOUT_SECS 10

int main() {
    int sock;
    struct sockaddr_in server;
    struct hostent *hp;
    char buf[BUF_SIZE];
    char path[BUF_SIZE];
    char request[BUF_SIZE];
    int total = 0;

    // Generate today's date in YYYY-MM-DD format
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char date_str[16];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", timeinfo);
    snprintf(path, sizeof(path), PATH_BASE, date_str);
    // printf("[LOG] Generated path: %s\n", path);

    // printf("[LOG] Creating socket...\n");
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    // printf("[LOG] Socket created successfully\n");

    // printf("[LOG] Resolving hostname: %s\n", HOST);
    hp = gethostbyname(HOST);
    if (!hp) { fprintf(stderr, "Unknown host\n"); return 1; }
    // printf("[LOG] Hostname resolved\n");

    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
    server.sin_port = htons(PORT);

    // Print resolved IP address
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &server.sin_addr, ip_str, sizeof(ip_str));
    // printf("[LOG] Resolved IP: %s\n", ip_str);

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

    // printf("[LOG] Connecting to %s:%d (%s)...\n", HOST, PORT, ip_str);
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect"); 
        printf("[ERROR] Connection failed. errno=%d\n", errno);
        close(sock); 
        return 1;
    }
    // printf("[LOG] Connected successfully\n");

    // Initialize OpenSSL
    // printf("[LOG] Initializing SSL/TLS...\n");
    SSL_library_init();
    SSL_load_error_strings();
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        fprintf(stderr, "[ERROR] Failed to create SSL context\n");
        close(sock);
        return 1;
    }

    // Create SSL connection
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    
    // printf("[LOG] Performing SSL/TLS handshake...\n");
    if (SSL_connect(ssl) <= 0) {
        printf("[ERROR] SSL handshake failed\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        return 1;
    }
    // printf("[LOG] SSL/TLS handshake successful\n");

    // Send HTTPS request
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: QNX-Weather/1.0\r\n"
        "Accept: application/xml\r\n"
        "Connection: close\r\n"
        "\r\n", path, HOST);

    // printf("[LOG] Sending HTTPS request...\n");
    if (SSL_write(ssl, request, strlen(request)) <= 0) {
        printf("[ERROR] Failed to send request\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        return 1;
    }
    // printf("[LOG] Request sent, waiting for response...\n");

    // Read response over SSL and collect full response
    int chunk_count = 0;
    char full_response[32768] = {0};
    int response_len = 0;
    
    while ((total = SSL_read(ssl, buf, BUF_SIZE - 1)) > 0) {
        chunk_count++;
        // printf("[LOG] Received chunk %d: %d bytes\n", chunk_count, total);
        buf[total] = '\0';
        
        // Append to full response (skip HTTP headers)
        if (response_len == 0 && strstr(buf, "\r\n\r\n")) {
            // Skip HTTP headers
            char *json_start = strstr(buf, "\r\n\r\n");
            if (json_start) json_start += 4;
            strncat(full_response, json_start, sizeof(full_response) - response_len - 1);
            response_len = strlen(full_response);
        } else if (response_len > 0) {
            strncat(full_response, buf, sizeof(full_response) - response_len - 1);
            response_len = strlen(full_response);
        }
    }
    // printf("[LOG] Read complete (received %d chunks). Closing connection...\n", chunk_count);
    
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sock);

    printf("\n=== Toronto Weather Data (Latest) ===\n");
    
    // Simple JSON extraction using string search
    char *json = full_response;
    
    // Extract station name
    char *stn_nam = strstr(json, "\"stn_nam-value\":\"");
    if (stn_nam) {
        stn_nam += 17; // Skip past the key
        char station[256] = {0};
        sscanf(stn_nam, "%255[^\"]", station);
        printf("Station: %s\n", station);
    }
    
    // Extract date/time
    char *date_tm = strstr(json, "\"date_tm-value\":\"");
    if (date_tm) {
        date_tm += 17;
        char datetime[64] = {0};
        sscanf(date_tm, "%63[^\"]", datetime);
        printf("Time: %s\n", datetime);
    }
    
    // Extract temperature
    char *air_temp = strstr(json, "\"air_temp\":");
    if (air_temp) {
        float temp;
        sscanf(air_temp, "\"air_temp\":%f", &temp);
        printf("Temperature: %.1f°C\n", temp);
    }
    
    // Extract dew point
    char *dwpt = strstr(json, "\"dwpt_temp\":");
    if (dwpt) {
        float dew;
        sscanf(dwpt, "\"dwpt_temp\":%f", &dew);
        printf("Dew Point: %.1f°C\n", dew);
    }
    
    // Extract humidity
    char *humidity = strstr(json, "\"rel_hum\":");
    if (humidity) {
        int rh;
        sscanf(humidity, "\"rel_hum\":%d", &rh);
        printf("Humidity: %d%%\n", rh);
    }
    
    // Extract wind speed
    char *wind_spd = strstr(json, "\"avg_wnd_spd_10m_pst10mts\":");
    if (wind_spd) {
        float wspd;
        sscanf(wind_spd, "\"avg_wnd_spd_10m_pst10mts\":%f", &wspd);
        printf("Wind Speed: %.1f km/h\n", wspd);
    }
    
    // Extract wind direction
    char *wind_dir = strstr(json, "\"avg_wnd_dir_10m_pst10mts\":");
    if (wind_dir) {
        int wdir;
        sscanf(wind_dir, "\"avg_wnd_dir_10m_pst10mts\":%d", &wdir);
        printf("Wind Direction: %d°\n", wdir);
    }
    
    // Extract visibility
    char *visibility = strstr(json, "\"vis\":");
    if (visibility) {
        float vis;
        sscanf(visibility, "\"vis\":%f", &vis);
        printf("Visibility: %.2f km\n", vis);
    }
    
    // Extract snow depth
    char *snow = strstr(json, "\"snw_dpth\":");
    if (snow) {
        int snw;
        sscanf(snow, "\"snw_dpth\":%d", &snw);
        printf("Snow Depth: %d cm\n", snw);
    }
    
    return 0;
}
