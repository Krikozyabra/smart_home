#include "cli/StreamCliSink.h"
#include "cli/StringCliSink.h"

#include <ostream>

namespace smart_home::cli {

StreamCliSink::StreamCliSink(std::ostream &out, std::ostream &err) noexcept
    : out_(out), err_(err) {}

void StreamCliSink::writeLine(std::string_view line) { out_ << line << '\n'; }

void StreamCliSink::writeError(std::string_view error) {
    err_ << error << '\n';
}

void StringCliSink::writeLine(std::string_view line) {
    out_.append(line).push_back('\n');
}

void StringCliSink::writeError(std::string_view error) {
    err_.append(error).push_back('\n');
}

std::string StringCliSink::output() const { return out_; }

std::string StringCliSink::error() const { return err_; }

void StringCliSink::clear() noexcept {
    out_.clear();
    err_.clear();
}

} // namespace smart_home::cli
