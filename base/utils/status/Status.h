#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#ifndef ____STATUS_H
#define ____STATUS_H

#include <string>
#include <cstring>
#include <utility>
#include <memory>
#include <string_view>
#include <ostream>

namespace yy::util {

extern std::string StrError(int errnum);

// errno -> StatusCode
class StatusCode ErrnoToStatusCode(int error_number);

// errno -> Status：最终Status内的错误信息会是 $"{message}: {strerror(error_number)}" 的形式
class Status ErrnoToStatus(int error_number, const std::string& message);


class StatusCode
{
    friend class StatusBase;
    static const int kUserErrorNumStart = 2048;
public:
    enum __StatusCode: int
    {
        kOk = 0,
        kCancelled = kUserErrorNumStart,
        kUnknown,
        kInvalidArgument,
        kDeadlineExceeded,
        kNotFound,
        kAlreadyExists,
        kPermissionDenied,
        kResourceExhausted,
        kFailedPrecondition,
        kAborted,
        kOutOfRange,
        kUnimplemented,
        kInternal,
        kUnavailable,
        kDataLoss,
        kUnauthenticated
    };

    StatusCode(int errnum): code_{errnum} {} //NOLINT
    StatusCode(__StatusCode code): code_{code} {} //NOLINT

    operator std::string() { return ToString(); }

    bool operator==(StatusCode::__StatusCode rhs) const { return code_ == (int)rhs; }
    bool operator==(StatusCode rhs) const { return code_ == rhs.code_; }

    std::string ToString() const;

    bool isErrno() const { return code_ < kUserErrorNumStart;  }
    bool isUserError() const { return code_ >= kUserErrorNumStart;  }

private:
    int code_;
};

inline bool operator==(StatusCode::__StatusCode lhs, StatusCode rhs) { return rhs == lhs; }

inline std::ostream& operator<<(std::ostream& os, StatusCode code) {
    return os << code.ToString();
}




class StatusBase final
{
    friend class Status;
public:
    StatusBase(StatusCode code, std::string message)
            : code_{code}, message_{std::move( message )} {}
    ~StatusBase() = default;
private:
    StatusCode  code_;
    std::string message_;
};


class Status final
{
public:
    Status();
    Status(StatusCode code, const std::string& message);
    //! 拷贝/移动/析构全部交由 std::shared_ptr 自动管理：
    //! 共享的是不可变数据（StatusBase），因此线程安全、零拷贝；移动后源对象为nullptr，所有访问器均已判空防护。
    Status(const Status &) = default;
    Status& operator=(const Status &) = default;
    Status(Status &&) noexcept = default;
    Status &operator=(Status &&) noexcept = default;
    ~Status() = default;

    bool ok() const { return data_ && data_->code_ == StatusCode::kOk; }

    StatusCode code() const { return data_ ? data_->code_ : StatusCode::kOk; }

    // 因为data_->message_只读，所以可以返回string_view
    std::string_view message() const { return data_ ? std::string_view{data_->message_} : std::string_view{}; }

    bool operator==(StatusCode code) const { return this->code() == code; }

    bool operator==(const Status& other) const {
        return this->code() == other.code() && this->message() == other.message();
    }

    bool operator!=(const Status& other) const { return !(*this == other); }

    // 返回code+message的格式化的错误信息
    std::string ToString() const;
private:
    std::shared_ptr<const StatusBase> data_;
};

inline std::ostream& operator<<(std::ostream& os, const Status& x) {
    return os << x.ToString();
}



inline Status OkStatus() { return {}; }








}









#endif //____STATUS_H

#pragma clang diagnostic pop
