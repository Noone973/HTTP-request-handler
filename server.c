/**
 * @file server.c
 * @brief Mini Concurrent HTTP/1.1 Web Server
 * 
 * A lightweight web server supporting:
 * - Concurrent client handling (fork-based)
 * - HTTP/1.1 GET requests
 * - MIME type detection
 * - Basic error handling (404, 500)
 * - File serving with buffer optimization
 * 
 * @license MIT
 * @author Kutlwano Mokheseng
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 8192
#define BACKLOG 10

/**
 * @struct HTTPRequest
 * @brief Parsed HTTP request structure
 */
typedef struct {
    char method[16];    /**< HTTP method (GET, POST, etc.) */
    char path[256];     /**< Requested resource path */
    char version[16];   /**< HTTP version */
} HTTPRequest;

/**
 * @brief Parse HTTP request line
 * @param request_line The first line of HTTP request
 * @param req Pointer to HTTPRequest structure to populate
 * @return int 0 on success, -1 on error
 */
int parse_http_request(const char* request_line, HTTPRequest* req) {
    return sscanf(request_line, "%15s %255s %15s", 
                  req->method, req->path, req->version) == 3 ? 0 : -1;
}

/**
 * @brief Get MIME type based on file extension
 * @param filename File name to check
 * @return const char* MIME type string
 */
const char* get_mime_type(const char* filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return "text/plain";
    
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    
    return "text/plain";
}

/**
 * @brief Send HTTP response to client
 * @param client_sock Client socket descriptor
 * @param status_code HTTP status code
 * @param status_text HTTP status text
 * @param content_type Response content type
 * @param content Response body content
 * @param content_length Length of response body
 */
void send_response(int client_sock, int status_code, const char* status_text,
                   const char* content_type, const char* content, size_t content_length) {
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             status_code, status_text, content_type, content_length);
    
    write(client_sock, header, strlen(header));
    write(client_sock, content, content_length);
}

/**
 * @brief Send HTTP error response
 * @param client_sock Client socket descriptor
 * @param status_code HTTP status code
 * @param message Error message
 */
void send_error(int client_sock, int status_code, const char* message) {
    char body[512];
    snprintf(body, sizeof(body),
             "<html><body><h1>%d %s</h1><p>%s</p></body></html>",
             status_code, message, message);
    
    send_response(client_sock, status_code, message, "text/html", body, strlen(body));
}

/**
 * @brief Serve file to client
 * @param client_sock Client socket descriptor
 * @param filepath Path to file to serve
 */
void serve_file(int client_sock, const char* filepath) {
    // Security: Prevent directory traversal
    if (strstr(filepath, "..")) {
        send_error(client_sock, 403, "Forbidden");
        return;
    }
    
    // Default to index.html for root
    if (strcmp(filepath, "/") == 0) {
        filepath = "/index.html";
    }
    
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), ".%s", filepath);
    
    int fd = open(fullpath, O_RDONLY);
    if (fd == -1) {
        send_error(client_sock, 404, "Not Found");
        return;
    }
    
    // Get file size
    struct stat st;
    fstat(fd, &st);
    off_t file_size = st.st_size;
    
    // Send headers
    const char* mime_type = get_mime_type(fullpath);
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n",
             mime_type, file_size);
    
    write(client_sock, header, strlen(header));
    
    // Send file content efficiently
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        write(client_sock, buffer, bytes_read);
    }
    
    close(fd);
}

/**
 * @brief Handle individual client connection
 * @param client_sock Client socket descriptor
 * @param client_addr Client address information
 */
void handle_client(int client_sock, struct sockaddr_in* client_addr) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_sock, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        // Parse HTTP request
        HTTPRequest req;
        char* first_line = strtok(buffer, "\r\n");
        
        if (first_line && parse_http_request(first_line, &req) == 0) {
            printf("[%s] %s %s\n", inet_ntoa(client_addr->sin_addr), 
                   req.method, req.path);
            
            if (strcmp(req.method, "GET") == 0) {
                serve_file(client_sock, req.path);
            } else {
                send_error(client_sock, 501, "Not Implemented");
            }
        } else {
            send_error(client_sock, 400, "Bad Request");
        }
    }
    
    close(client_sock);
    exit(0); // Important for forked process
}

/**
 * @brief Signal handler for zombie processes
 * @param sig Signal number
 */
void zombie_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/**
 * @brief Main server function
 * @return int Exit status
 */
int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Setup signal handler for zombie processes
    signal(SIGCHLD, zombie_handler);
    
    // Create socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_sock, BACKLOG) < 0) {
        perror("listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }
    
    printf("Mini HTTP Server running on http://localhost:%d\n", PORT);
    printf("Serving files from: %s\n", getcwd(NULL, 0));
    printf("Press Ctrl+C to stop\n\n");
    
    // Main server loop
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept failed");
            continue;
        }
        
        // Fork to handle client concurrently
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(server_sock);
            handle_client(client_sock, &client_addr);
        } else if (pid > 0) {
            // Parent process
            close(client_sock);
        } else {
            perror("fork failed");
            close(client_sock);
        }
    }
    
    close(server_sock);
    return 0;
}