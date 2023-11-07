#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#ifndef ____STATUS_H
#define ____STATUS_H

#include<string>
#include<cstring>
#include <utility>

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

static std::ostream& operator<<(std::ostream& os, StatusCode code) {
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
    Status(const Status & other);                // copy-ctor
    Status& operator=(const Status & other);     // copy-assign
    Status(Status && other) noexcept;            // move-ctor
    Status &operator=(Status && other) noexcept; // move-assign
    ~Status();

    bool ok() const { return data_->code_ == StatusCode::kOk; }

    StatusCode code() const { return data_->code_; }

    // 因为data_->message_只读，所以可以返回string_view
    std::string_view message() const { return data_->message_; }

    bool operator==(StatusCode code) const { return data_->code_ == code; }

    // 返回code+message的格式化的错误信息
    std::string ToString() const;
private:
    StatusBase * data_;
};

static std::ostream& operator<<(std::ostream& os, const Status& x) {
    return os << x.ToString();
}



inline Status OkStatus() { return {}; }








}









#endif //____STATUS_H

#pragma clang diagnostic pop