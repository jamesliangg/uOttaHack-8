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
#define PATH_TEMPLATE "/collections/swob-realtime/items/%s-0000-%s-%s-swob.xml?lang=en"
#define TIMEOUT_SECS 10
#define MAX_DAYS 7
#define MAX_STATIONS 150

typedef struct {
    char code[16];      // ICAO code
    char name[64];      // Station name
    char mode[16];      // AUTO or MAN
} Station;

typedef struct {
    char station[256];
    float temperature;
    float dew_point;
    int humidity;
    float wind_speed;
    int wind_direction;
    float visibility;
    int snow_depth;
    char datetime[64];
    char date[16];
} WeatherData;

typedef struct {
    WeatherData data[MAX_DAYS];
    int count;
} WeatherHistory;

// Ontario weather stations
Station ontario_stations[] = {
    {"CYAT", "Attawapiskat", "AUTO"},
    {"CYTL", "Big Trout Lake", "AUTO"},
    {"COTL", "Big Trout Lake", "AUTO"},
    {"CXEA", "Ear Falls", "AUTO"},
    {"CYER", "Fort Severn", "AUTO"},
    {"CYLH", "Lansdowne House", "AUTO"},
    {"CWLF", "Lansdowne House", "AUTO"},
    {"CZMD", "Muskrat Dam", "AUTO"},
    {"CWWN", "Peawanuck", "AUTO"},
    {"CYPO", "Peawanuck", "AUTO"},
    {"CWPL", "Pickle Lake", "AUTO"},
    {"CYPL", "Pickle Lake", "AUTO"},
    {"CYRL", "Red Lake", "MAN"},
    {"CYRL", "Red Lake", "AUTO"},
    {"CZSJ", "Sandy Lake", "AUTO"},
    {"COWW", "Weagamow Lake", "AUTO"},
    {"CWCH", "Atikokan", "AUTO"},
    {"CTAG", "Fort Frances RCS", "AUTO"},
    {"CYHD", "Dryden Regional", "AUTO"},
    {"CTKR", "Kenora RCS", "AUTO"},
    {"CYQK", "Kenora", "MAN"},
    {"CTRA", "Rawson Lake", "AUTO"},
    {"CWTX", "Royal Island", "AUTO"},
    {"CYXL", "Sioux Lookout", "MAN"},
    {"COSL", "Sioux Lookout Airport", "AUTO"},
    {"CWYW", "Armstrong", "AUTO"},
    {"CYYW", "Armstrong", "AUTO"},
    {"CXCA", "Cameron Falls", "AUTO"},
    {"CWCI", "Caribou Island", "AUTO"},
    {"CYGQ", "Geraldton (Greenstone Regional)", "AUTO"},
    {"COGE", "Geraldton Airport", "AUTO"},
    {"CYSP", "Marathon", "MAN"},
    {"CYSP", "Marathon", "AUTO"},
    {"CWCJ", "Pukaskwa", "AUTO"},
    {"COTR", "Terrace Bay Airport", "AUTO"},
    {"CYQT", "Thunder Bay", "AUTO"},
    {"CYQT", "Thunder Bay", "MAN"},
    {"CZTB", "Thunder Bay CS", "AUTO"},
    {"CWDV", "Upsala", "AUTO"},
    {"CWEC", "Welcome Island", "AUTO"},
    {"CWKK", "Little Flatland Island", "AUTO"},
    {"CTLS", "Lake Superior Provincial Park", "AUTO"},
    {"CYAM", "Sault Ste Marie", "MAN"},
    {"COSM", "Sault Ste. Marie Airport", "AUTO"},
    {"CYXZ", "Wawa", "MAN"},
    {"CYXZ", "Wawa", "AUTO"},
    {"CYLD", "Chapleau", "MAN"},
    {"CYLD", "Chapleau", "AUTO"},
    {"COCP", "Chapleau Airport", "AUTO"},
    {"CTSB", "Sudbury Climate", "AUTO"},
    {"CYSB", "Sudbury", "MAN"},
    {"CYXR", "Earlton (Timiskaming Regional)", "AUTO"},
    {"CTXR", "Earlton Climate", "AUTO"},
    {"CYYU", "Kapuskasing", "MAN"},
    {"CYYU", "Kapuskasing", "AUTO"},
    {"CXKA", "Kapuskasing CDA ON", "AUTO"},
    {"CXKI", "Kirkland Lake", "AUTO"},
    {"CYMO", "Moosonee", "AUTO"},
    {"CXZC", "Moosonee RCS", "AUTO"},
    {"CWNZ", "Nagagami", "AUTO"},
    {"CYKP", "Ogoki Post", "AUTO"},
    {"COGP", "Ogoki Post", "AUTO"},
    {"CTMS", "Timmins Climate", "AUTO"},
    {"CYTS", "Timmins (Victor M. Power)", "MAN"},
    {"CTNK", "Algonquin Park East Gate", "AUTO"},
    {"CYYB", "North Bay", "AUTO"},
    {"CTZN", "North Bay Airport", "AUTO"},
    {"CYYB", "North Bay", "MAN"},
    {"CYZE", "Gore Bay-Manitoulin", "AUTO"},
    {"CTZE", "Gore Bay Climate", "AUTO"},
    {"CTBO", "Brockville Climate", "AUTO"},
    {"CWGH", "Grenadier Island", "AUTO"},
    {"CXKE", "Kemptville CS", "AUTO"},
    {"CTKG", "Kingston Climate", "AUTO"},
    {"CYGK", "Kingston", "MAN"},
    {"CYGK", "Kingston", "AUTO"},
    {"CTCK", "Moose Creek Wells", "AUTO"},
    {"CXOA", "Ottawa CDA RCS", "AUTO"},
    {"CYOW", "Ottawa/Macdonald-Cartier International", "MAN"},
    {"CTPM", "Pembroke", "AUTO"},
    {"CYWA", "Petawawa", "MAN"},
    {"CTBT", "Beatrice Climate", "AUTO"},
    {"CWGL", "Lagoon City", "AUTO"},
    {"CYQA", "Muskoka", "AUTO"},
    {"CXPC", "Parry Sound CCG", "AUTO"},
    {"CXBI", "Barrie-Oro", "AUTO"},
    {"CYVV", "Wiarton", "AUTO"},
    {"CYVV", "Wiarton", "MAN"},
    {"CWMZ", "Western Island", "AUTO"},
    {"CXET", "Egbert CS", "AUTO"},
    {"CWGD", "Goderich", "AUTO"},
    {"CYZR", "Sarnia (Chris Hadfield)", "AUTO"},
    {"CTZR", "Sarnia Climate", "AUTO"},
    {"CTTR", "Tobermory RCS", "AUTO"},
    {"CYCK", "Chatham-Kent", "AUTO"},
    {"COCE", "Cedar Springs", "AUTO"},
    {"CXDI", "Delhi CS", "AUTO"},
    {"CXHA", "Harrow CDA Auto", "AUTO"},
    {"CWPS", "Long Point", "AUTO"},
    {"CWWZ", "Port Weller", "AUTO"},
    {"CXRG", "Ridgetown RCS", "AUTO"},
    {"CYSN", "St. Catharines/Niagara District", "MAN"},
    {"CYSN", "St. Catharines/Niagara District", "AUTO"},
    {"CXVN", "Vineland Station RCS", "AUTO"},
    {"CTWL", "Welland-Pelham", "AUTO"},
    {"CYQG", "Windsor", "AUTO"},
    {"CWPC", "Port Colborne", "AUTO"},
    {"CXPT", "Point Pelee CS", "AUTO"},
    {"CTBF", "Brantford Airport", "AUTO"},
    {"CZEL", "Elora RCS", "AUTO"},
    {"COGI", "Guelph Turfgrass Institute", "AUTO"},
    {"CYKF", "Kitchener/Waterloo", "AUTO"},
    {"CYXU", "London", "MAN"},
    {"CWSN", "London CS", "AUTO"},
    {"CWLS", "Mount Forest", "AUTO"},
    {"COBQ", "Belleville Quinte", "AUTO"},
    {"CWWB", "Burlington Pier", "AUTO"},
    {"CWNC", "Cobourg", "AUTO"},
    {"CYHM", "Hamilton", "MAN"},
    {"CXHM", "Hamilton RBG CS", "AUTO"},
    {"COKN", "King City North", "AUTO"},
    {"CYOO", "Oshawa Executive Airport", "AUTO"},
    {"CWQP", "Point Petre", "AUTO"},
    {"CXTO", "Toronto City", "AUTO"},
    {"CYTZ", "Billy Bishop Toronto City Airport", "AUTO"},
    {"CYYZ", "Toronto/Pearson International", "MAN"},
    {"CYTR", "Trenton", "MAN"},
    {"CTUX", "Uxbridge West", "AUTO"},
    {"CYPQ", "Peterborough", "AUTO"},
    {"COTU", "Peterborough Trent U Experimental Farm", "AUTO"},
    {"CWRK", "Bancroft Auto", "AUTO"}
};

int num_stations = sizeof(ontario_stations) / sizeof(Station);

// Helper function to fetch weather data from a given path
void fetch_weather(const char *path, char *response) {
    int sock;
    struct sockaddr_in server;
    struct hostent *hp;
    char buf[BUF_SIZE];
    char request[BUF_SIZE];
    int total = 0;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return; }

    hp = gethostbyname(HOST);
    if (!hp) { fprintf(stderr, "Unknown host\n"); return; }

    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
    server.sin_port = htons(PORT);

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECS;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect");
        close(sock);
        return;
    }

    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        close(sock);
        return;
    }

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    
    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        return;
    }

    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: QNX-Weather/1.0\r\n"
        "Accept: application/xml\r\n"
        "Connection: close\r\n"
        "\r\n", path, HOST);

    if (SSL_write(ssl, request, strlen(request)) <= 0) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sock);
        return;
    }

    int response_len = 0;
    while ((total = SSL_read(ssl, buf, BUF_SIZE - 1)) > 0) {
        buf[total] = '\0';
        
        // Skip HTTP headers
        if (response_len == 0 && strstr(buf, "\r\n\r\n")) {
            char *json_start = strstr(buf, "\r\n\r\n");
            if (json_start) json_start += 4;
            strncat(response, json_start, 32768 - response_len - 1);
            response_len = strlen(response);
        } else if (response_len > 0) {
            strncat(response, buf, 32768 - response_len - 1);
            response_len = strlen(response);
        }
    }
    
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sock);
}

// Parse weather data from JSON response
WeatherData parse_weather(const char *json) {
    WeatherData wd = {0};
    
    char *stn_nam = strstr(json, "\"stn_nam-value\":\"");
    if (stn_nam) {
        stn_nam += 17;
        sscanf(stn_nam, "%255[^\"]", wd.station);
    }
    
    char *date_tm = strstr(json, "\"date_tm-value\":\"");
    if (date_tm) {
        date_tm += 17;
        sscanf(date_tm, "%63[^\"]", wd.datetime);
        // Extract just the date part (YYYY-MM-DD)
        strncpy(wd.date, wd.datetime, 10);
        wd.date[10] = '\0';
    }
    
    char *air_temp = strstr(json, "\"air_temp\":");
    if (air_temp) {
        sscanf(air_temp, "\"air_temp\":%f", &wd.temperature);
    }
    
    char *dwpt = strstr(json, "\"dwpt_temp\":");
    if (dwpt) {
        sscanf(dwpt, "\"dwpt_temp\":%f", &wd.dew_point);
    }
    
    char *humidity = strstr(json, "\"rel_hum\":");
    if (humidity) {
        sscanf(humidity, "\"rel_hum\":%d", &wd.humidity);
    }
    
    char *wind_spd = strstr(json, "\"avg_wnd_spd_10m_pst10mts\":");
    if (wind_spd) {
        sscanf(wind_spd, "\"avg_wnd_spd_10m_pst10mts\":%f", &wd.wind_speed);
    }
    
    char *wind_dir = strstr(json, "\"avg_wnd_dir_10m_pst10mts\":");
    if (wind_dir) {
        sscanf(wind_dir, "\"avg_wnd_dir_10m_pst10mts\":%d", &wd.wind_direction);
    }
    
    char *visibility = strstr(json, "\"vis\":");
    if (visibility) {
        sscanf(visibility, "\"vis\":%f", &wd.visibility);
    }
    
    char *snow = strstr(json, "\"snw_dpth\":");
    if (snow) {
        sscanf(snow, "\"snw_dpth\":%d", &wd.snow_depth);
    }
    
    return wd;
}

// Fetch historical data for the past 7 days
WeatherHistory fetch_historical(const char *station_code, const char *station_mode) {
    WeatherHistory history = {0};
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char date_str[16];
    char path[BUF_SIZE];
    char response[32768];
    
    printf("Fetching 7-day historical data...\n");
    
    for (int i = 0; i < MAX_DAYS; i++) {
        // Go back i days
        time_t day = now - (i * 86400);
        struct tm *day_info = localtime(&day);
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", day_info);
        
        snprintf(path, sizeof(path), PATH_TEMPLATE, date_str, station_code, station_mode);
        memset(response, 0, sizeof(response));
        
        printf("  Fetching %s...\n", date_str);
        fetch_weather(path, response);
        
        if (strlen(response) > 0) {
            history.data[history.count] = parse_weather(response);
            history.count++;
        }
    }
    
    return history;
}

// Print ASCII temperature graph
void print_temperature_graph(WeatherHistory history) {
    if (history.count == 0) {
        printf("No data available\n");
        return;
    }
    
    // Find min and max temperatures
    float min_temp = history.data[0].temperature;
    float max_temp = history.data[0].temperature;
    
    for (int i = 0; i < history.count; i++) {
        if (history.data[i].temperature < min_temp) min_temp = history.data[i].temperature;
        if (history.data[i].temperature > max_temp) max_temp = history.data[i].temperature;
    }
    
    // Add some padding
    float range = max_temp - min_temp;
    if (range < 2) range = 2;
    min_temp -= range * 0.1;
    max_temp += range * 0.1;
    
    int height = 15;
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║         7-DAY TEMPERATURE HISTORY                      ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    // Print graph
    for (int row = height; row >= 0; row--) {
        float temp_level = min_temp + (max_temp - min_temp) * row / height;
        printf("%6.1f°C │", temp_level);
        
        for (int i = history.count - 1; i >= 0; i--) {
            float normalized = (history.data[i].temperature - min_temp) / (max_temp - min_temp);
            int bar_row = (int)(normalized * height);
            
            if (bar_row >= row) {
                printf("  ██  ");
            } else {
                printf("      ");
            }
        }
        printf("│\n");
    }
    
    printf("        └");
    for (int i = 0; i < history.count; i++) {
        printf("─────");
    }
    printf("┘\n");
    
    // Print dates below
    printf("          ");
    for (int i = history.count - 1; i >= 0; i--) {
        printf(" %s ", history.data[i].date);
    }
    printf("\n");
}

// Display data table
void print_weather_table(WeatherHistory history) {
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║         DETAILED WEATHER DATA                          ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    printf("┌──────────────┬────────┬──────────┬──────────┬──────────┐\n");
    printf("│ Date         │ Temp   │ Humidity │ Wind     │ Visible  │\n");
    printf("├──────────────┼────────┼──────────┼──────────┼──────────┤\n");
    
    for (int i = history.count - 1; i >= 0; i--) {
        printf("│ %s │ %6.1f°│ %8d%% │ %8.1f │ %8.2f │\n",
            history.data[i].date,
            history.data[i].temperature,
            history.data[i].humidity,
            history.data[i].wind_speed,
            history.data[i].visibility);
    }
    
    printf("└──────────────┴────────┴──────────┴──────────┴──────────┘\n");
}

// Menu display and selection with pagination
int show_menu() {
    int page = 0;
    int stations_per_page = 15;
    int total_pages = (num_stations + stations_per_page - 1) / stations_per_page;
    char input[10];
    
    while (1) {
        printf("\n╔════════════════════════════════════════════════════════╗\n");
        printf("║       ONTARIO WEATHER STATION SELECTOR                 ║\n");
        printf("║                  Page %d of %d                            ║\n", page + 1, total_pages);
        printf("╚════════════════════════════════════════════════════════╝\n\n");
        
        int start = page * stations_per_page;
        int end = start + stations_per_page;
        if (end > num_stations) end = num_stations;
        
        for (int i = start; i < end; i++) {
            printf("  %3d. %-40s (%s - %s)\n", 
                i + 1, 
                ontario_stations[i].name, 
                ontario_stations[i].code,
                ontario_stations[i].mode);
        }
        
        printf("\n");
        if (page > 0) printf("  [p] Previous page    ");
        if (page < total_pages - 1) printf("  [n] Next page");
        printf("\n  [e/q] Exit\n\n");
        printf("Enter choice (1-%d) or command: ", num_stations);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            return -1; // Exit on EOF
        }
        
        // Trim newline
        input[strcspn(input, "\n")] = 0;
        
        // Check for commands
        if (strcmp(input, "e") == 0 || strcmp(input, "q") == 0 || 
            strcmp(input, "E") == 0 || strcmp(input, "Q") == 0) {
            return -1; // Exit
        } else if ((strcmp(input, "n") == 0 || strcmp(input, "N") == 0) && page < total_pages - 1) {
            page++;
            continue;
        } else if ((strcmp(input, "p") == 0 || strcmp(input, "P") == 0) && page > 0) {
            page--;
            continue;
        } else {
            int choice = atoi(input);
            if (choice >= 1 && choice <= num_stations) {
                return choice;
            } else {
                printf("Invalid choice. Please try again.\n");
                continue;
            }
        }
    }
}

int main() {
    int choice;
    
    while (1) {
        choice = show_menu();
        
        if (choice == -1) {
            printf("Goodbye!\n");
            return 0;
        }
        
        if (choice < 1 || choice > num_stations) {
            printf("Invalid choice. Please try again.\n");
            continue;
        }
        
        Station selected = ontario_stations[choice - 1];
        printf("\nFetching weather data for %s (%s)...\n", selected.name, selected.code);
        
        WeatherHistory history = fetch_historical(selected.code, selected.mode);
        
        if (history.count > 0) {
            printf("\n");
            print_temperature_graph(history);
            print_weather_table(history);
        } else {
            printf("Failed to fetch weather data. Please try again.\n");
        }
    }
    
    return 0;
}
