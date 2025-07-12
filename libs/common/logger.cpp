#include "logger.hpp"

// Static member definitions
std::atomic<Logger::Level> Logger::global_level_(Logger::Level::INFO);
std::shared_ptr<Logger> Logger::instance_ = nullptr;
std::mutex Logger::instance_mutex_;
