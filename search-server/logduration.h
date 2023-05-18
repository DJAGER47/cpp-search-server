#pragma once

#include <chrono>
#include <string>
#include <iostream>
#include <string_view>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)
#define LOG_DURATION_STREAM(x, out) LogDuration UNIQUE_VAR_NAME_PROFILE(x, out)

class LogDuration
{
public:
    // заменим имя типа std::chrono::steady_clock
    // с помощью using для удобства
    using Clock = std::chrono::steady_clock;

    LogDuration() = default;
    explicit LogDuration(const std::string &name) : m_name{name}
    {
    }

    LogDuration(const std::string_view &name, std::ostream &streamm = std::cerr) : m_name{name}, m_ostream{streamm}
    {
    }

    ~LogDuration()
    {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        m_ostream << m_name << ": "s << duration_cast<microseconds>(dur).count()
                  << " us"s << std::endl;
    }

private:
    const Clock::time_point start_time_ = Clock::now();
    std::string m_name;
    std::ostream &m_ostream = std::cerr;
};
