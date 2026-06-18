#pragma once
#include <stdexcept>
#include <string>

namespace services {

class ServiceException : public std::runtime_error {
private:
    int status_code_;
public:
    ServiceException(int status, const std::string& msg) : std::runtime_error(msg), status_code_(status) {}
    int status_code() const { return status_code_; }
};

} // namespace services
