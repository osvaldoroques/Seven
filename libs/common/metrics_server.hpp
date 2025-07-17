#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <sstream>
#include <map>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace PrometheusMetrics {

// Simple HTTP server for metrics endpoint
class MetricsServer {
public:
    MetricsServer(int port = 8080) : port_(port), running_(false) {}
    
    ~MetricsServer() {
        stop();
    }
    
    void start() {
        if (running_.load()) return;
        
        running_.store(true);
        server_thread_ = std::thread([this]() {
            run_server();
        });
    }
    
    void stop() {
        if (!running_.load()) return;
        
        running_.store(false);
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
    
    void set_metrics_handler(std::function<std::string()> handler) {
        metrics_handler_ = handler;
    }

private:
    void run_server() {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            return;
        }
        
        // Set socket options
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);
        
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            close(server_fd);
            return;
        }
        
        if (listen(server_fd, 10) < 0) {
            close(server_fd);
            return;
        }
        
        // Set non-blocking
        fcntl(server_fd, F_SETFL, O_NONBLOCK);
        
        while (running_.load()) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd == -1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            // Handle request in a separate thread
            std::thread([this, client_fd]() {
                handle_client(client_fd);
            }).detach();
        }
        
        close(server_fd);
    }
    
    void handle_client(int client_fd) {
        char buffer[1024] = {0};
        int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read <= 0) {
            close(client_fd);
            return;
        }
        
        std::string request(buffer);
        std::string response;
        
        if (request.find("GET /metrics") == 0) {
            // Serve metrics
            std::string metrics_content = metrics_handler_ ? metrics_handler_() : "# No metrics available\n";
            
            response = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
                      "Content-Length: " + std::to_string(metrics_content.length()) + "\r\n"
                      "\r\n" + metrics_content;
        } else if (request.find("GET /health") == 0) {
            // Health check endpoint
            std::string health_content = "OK\n";
            response = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: " + std::to_string(health_content.length()) + "\r\n"
                      "\r\n" + health_content;
        } else {
            // 404 for other paths
            std::string not_found = "Not Found\n";
            response = "HTTP/1.1 404 Not Found\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: " + std::to_string(not_found.length()) + "\r\n"
                      "\r\n" + not_found;
        }
        
        write(client_fd, response.c_str(), response.length());
        close(client_fd);
    }
    
    int port_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    std::function<std::string()> metrics_handler_;
};

} // namespace PrometheusMetrics
