#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <sys/inotify.h>
#include <yaml-cpp/yaml.h>
#include <unistd.h>
#include <condition_variable>

class Configuration {
public:
    explicit Configuration(const std::string& yaml_path)
      : yaml_path_(yaml_path), stop_watch_(false)
    {
        loadAll();
    }

    template<typename T>
    T get(const std::string& key, T default_value) const {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = data_.find(key);
        if (it == data_.end()) return default_value;
        std::istringstream ss(it->second);
        T ret{};
        ss >> ret;
        return ss.fail() ? default_value : ret;
    }
    
    // Check if configuration is valid/loaded
    bool is_valid() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return !data_.empty();
    }
    
    // Conversion operator for boolean checks
    explicit operator bool() const {
        return is_valid();
    }

    void onReload(std::function<void()> cb) {
        std::lock_guard<std::mutex> lk(mtx_);
        callbacks_.push_back(std::move(cb));
    }

    void startWatch() {
        inotify_fd_ = inotify_init1(IN_NONBLOCK);
        if (inotify_fd_ < 0) {
            perror("inotify_init1");
            return;
        }
        wd_ = inotify_add_watch(inotify_fd_, yaml_path_.c_str(), IN_MODIFY);
        watch_thread_ = std::thread([this]{ watchLoop(); });
    }

    void stopWatch() {
        stop_watch_ = true;
        if (watch_thread_.joinable()) {
            watch_thread_.join();
        }
        if (wd_ >= 0) {
            inotify_rm_watch(inotify_fd_, wd_);
            wd_ = -1;
        }
        if (inotify_fd_ >= 0) {
            close(inotify_fd_);
            inotify_fd_ = -1;
        }
    }

    ~Configuration() {
        stopWatch();
    }

private:
    void loadAll() {
        std::lock_guard<std::mutex> lk(mtx_);
        data_.clear();
        loadDefaults();
        loadYaml();
        loadEnv();
    }

    void loadDefaults() {
        data_["nats.url"] = "nats://localhost:4222";
        data_["threads"]  = "4";
    }

    void loadYaml() {
        try {
            auto node = YAML::LoadFile(yaml_path_);
            for (auto it : node) {
                data_[it.first.as<std::string>()] = it.second.as<std::string>();
            }
        } catch (const std::exception& e) {
            std::cerr << "⚠️ YAML load failed: " << e.what() << "\n";
        }
    }

    void loadEnv() {
        for (auto& kv : data_) {
            auto env_key = kv.first;
            for (auto& c : env_key) if (c == '.') c = '_';
            if (auto* ev = std::getenv(env_key.c_str()))
                kv.second = ev;
        }
    }

    void watchLoop() {
        constexpr size_t BUF_LEN = 1024 * (sizeof(inotify_event) + 16);
        std::vector<char> buf(BUF_LEN);
        while (!stop_watch_) {
            int len = read(inotify_fd_, buf.data(), BUF_LEN);
            if (len > 0) {
                loadAll();
                std::lock_guard<std::mutex> lk(mtx_);
                for (auto& cb : callbacks_) cb();
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    std::string   yaml_path_;
    mutable std::mutex                               mtx_;
    std::unordered_map<std::string, std::string>     data_;
    std::vector<std::function<void()>>                callbacks_;
    int                                               inotify_fd_{-1}, wd_{-1};
    std::thread                                       watch_thread_;
    std::atomic<bool>                                 stop_watch_;
};
