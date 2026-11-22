/**
 * @file ConsoleInput.h
 * @brief Внутренний адаптер неблокирующего чтения строк из консоли.
 */
#pragma once

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <string>
#include <system_error>
#include <utility>

namespace smart_home::runtime {

/** @brief Заимствует дескриптор stdin и восстанавливает его флаги при
 * разрушении. */
class ConsoleInput {
  public:
    enum class Status { Waiting, Line, TooLong, End };

    struct Result {
        Status status;
        std::string line;
    };

    explicit ConsoleInput(int fd) : fd_(fd), flags_(::fcntl(fd, F_GETFL)) {
        if (flags_ < 0 || ::fcntl(fd_, F_SETFL, flags_ | O_NONBLOCK) < 0)
            throw std::system_error(errno, std::generic_category(),
                                    "stdin flags");
    }

    ~ConsoleInput() { ::fcntl(fd_, F_SETFL, flags_); }
    ConsoleInput(const ConsoleInput &) = delete;
    ConsoleInput &operator=(const ConsoleInput &) = delete;

    bool hasBufferedInput() const noexcept { return offset_ < size_; }

    // Один вызов обрабатывает не более одной строки и одного чтения из stdin.
    Result next() {
        if (!hasBufferedInput()) {
            const auto count = ::read(fd_, bytes_.data(), bytes_.size());
            if (count == 0)
                return {Status::End, std::exchange(line_, {})};
            if (count < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    return {Status::Waiting, {}};
                throw std::system_error(errno, std::generic_category(),
                                        "read stdin");
            }
            offset_ = 0;
            size_ = static_cast<std::size_t>(count);
        }
        while (hasBufferedInput()) {
            const char byte = bytes_[offset_++];
            if (discarding_) {
                if (byte == '\n')
                    discarding_ = false;
                continue;
            }
            if (byte == '\n')
                return {Status::Line, std::exchange(line_, {})};
            if (line_.size() == line_limit) {
                line_.clear();
                discarding_ = true;
                return {Status::TooLong, {}};
            }
            line_.push_back(byte);
        }
        return {Status::Waiting, {}};
    }

  private:
    static constexpr std::size_t line_limit = 65536;

    int fd_;
    int flags_;
    std::array<char, 4096> bytes_;
    std::size_t offset_ = 0;
    std::size_t size_ = 0;
    std::string line_;
    bool discarding_ = false;
};

} // namespace smart_home::runtime
