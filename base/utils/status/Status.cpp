#pragma warning(disable:4068)
#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCode"

#include "Status.h"
#include "util_functions.h"
#include <iostream>
#include <sstream>
namespace yy::util {



/********************************* StatusCode *********************************/
std::string StatusCode::ToString() const
{
    if(isErrno()) {
        return  util::GetErrorInfo(code_);
    }

    switch(code_) {
        case StatusCode::kOk:return "OK";
        case StatusCode::kCancelled:return "CANCELLED";
        case StatusCode::kUnknown:return "eUNKNOWN";
        case StatusCode::kInvalidArgument:return "INVALID_ARGUMENT";
        case StatusCode::kDeadlineExceeded:return "DEADLINE_EXCEEDED";
        case StatusCode::kNotFound:return "NOT_FOUND";
        case StatusCode::kAlreadyExists:return "ALREADY_EXISTS";
        case StatusCode::kPermissionDenied:return "PERMISSION_DENIED";
        case StatusCode::kResourceExhausted:return "RESOURCE_EXHAUSTED";
        case StatusCode::kFailedPrecondition:return "FAILED_PRECONDITION";
        case StatusCode::kAborted:return "ABORTED";
        case StatusCode::kOutOfRange:return "OUT_OF_RANGE";
        case StatusCode::kUnimplemented:return "UNIMPLEMENTED";
        case StatusCode::kInternal:return "INTERNAL";
        case StatusCode::kUnavailable:return "UNAVAILABLE";
        case StatusCode::kDataLoss:return "DATA_LOSS";
        case StatusCode::kUnauthenticated:return "UNAUTHENTICATED";
        default:return "eUNKNOWN ERROR";
    }
}


StatusCode ErrnoToStatusCode(int error_number) {
    switch (error_number) {
        //! StatusCode::kOk
        case 0:
            return StatusCode::kOk;

        //! StatusCode::kInvalidArgument
        case EINVAL:        // Invalid argument
        case ENAMETOOLONG:  // Filename too long
        case E2BIG:         // Argument list too long
        case EDESTADDRREQ:  // Destination address required
        case EDOM:          // Mathematics argument out of domain of function
        case EFAULT:        // Bad address
        case EILSEQ:        // Illegal byte sequence
        case ENOPROTOOPT:   // Protocol not available
        case ENOSTR:        // Not a STREAM
        case ENOTSOCK:      // Not a socket
        case ENOTTY:        // Inappropriate I/O control operation
        case EPROTOTYPE:    // Protocol wrong type for socket
        case ESPIPE:        // Invalid seek
            return StatusCode::kInvalidArgument;

        //! StatusCode::kDeadlineExceeded
        case ETIMEDOUT:  // Connection timed out
        case ETIME:      // Timer expired
            return StatusCode::kDeadlineExceeded;

        //! StatusCode::kNotFound
        case ENODEV:  // No such device
        case ENOENT:  // No such file or directory
#ifdef ENOMEDIUM
            case ENOMEDIUM:  // No medium found
#endif
        case ENXIO:  // No such device or address
        case ESRCH:  // No such process
            return StatusCode::kNotFound;

        //! StatusCode::kAlreadyExists
        case EEXIST:         // File exists
        case EADDRNOTAVAIL:  // Address not available
        case EALREADY:       // Connection already in progress
#ifdef ENOTUNIQ
            case ENOTUNIQ:  // Name not unique on network
#endif
            return StatusCode::kAlreadyExists;

        //! StatusCode::kPermissionDenied
        case EPERM:   // Operation not permitted
        case EACCES:  // Permission denied
#ifdef ENOKEY
            case ENOKEY:  // Required key not available
#endif
        case EROFS:  // Read only file system
            return StatusCode::kPermissionDenied;

        //! StatusCode::kFailedPrecondition
        case ENOTEMPTY:   // Directory not empty
        case EISDIR:      // Is a directory
        case ENOTDIR:     // Not a directory
        case EADDRINUSE:  // Address already in use
        case EBADF:       // Invalid file descriptor
#ifdef EBADFD
        case EBADFD:      // File descriptor in bad state
#endif
        case EBUSY:       // Device or resource busy
        case ECHILD:      // No child processes
        case EISCONN:     // Socket is connected
#ifdef EISNAM
        case EISNAM:      // Is a named type file
#endif
#ifdef ENOTBLK
        case ENOTBLK:     // Block device required
#endif
        case ENOTCONN:    // The socket is not connected
        case EPIPE:       // Broken pipe
#ifdef ESHUTDOWN
        case ESHUTDOWN:   // Cannot send after transport endpoint shutdown
#endif
        case ETXTBSY:     // Text file busy
#ifdef EUNATCH
        case EUNATCH:     // Protocol driver not attached
#endif
            return StatusCode::kFailedPrecondition;

            //! StatusCode::kResourceExhausted
        case ENOSPC:   // No space left on device
#ifdef EDQUOT
        case EDQUOT:   // Disk quota exceeded
#endif
        case EMFILE:   // Too many open files
        case EMLINK:   // Too many links
        case ENFILE:   // Too many open files in system
        case ENOBUFS:  // No buffer space available
        case ENODATA:  // No message is available on the STREAM read queue
        case ENOMEM:   // Not enough space
        case ENOSR:    // No STREAM resources
#ifdef EUSERS
        case EUSERS:   // Too many users
#endif
            return StatusCode::kResourceExhausted;

            //! StatusCode::kOutOfRange
#ifdef ECHRNG
        case ECHRNG:     // Channel number out of range
#endif
        case EFBIG:      // File too large
        case EOVERFLOW:  // Value too large to be stored in data type
        case ERANGE:     // Result too large
            return StatusCode::kOutOfRange;

            //! StatusCode::kUnimplemented
#ifdef ENOPKG
            case ENOPKG:        // Package not installed
#endif
        case ENOSYS:        // Function not implemented
        case ENOTSUP:       // Operation not supported
        case EAFNOSUPPORT:  // Address family not supported
#ifdef EPFNOSUPPORT
            case EPFNOSUPPORT:  // Protocol family not supported
#endif
        case EPROTONOSUPPORT:  // Protocol not supported
#ifdef ESOCKTNOSUPPORT
        case ESOCKTNOSUPPORT:  // Socket type not supported
#endif
        case EXDEV:  // Improper link
            return StatusCode::kUnimplemented;

            //! StatusCode::kUnavailable
        case EAGAIN:  // Resource temporarily unavailable
#ifdef ECOMM
        case ECOMM:   // Communication error on send
#endif
        case ECONNREFUSED:  // Connection refused
        case ECONNABORTED:  // Connection aborted
        case ECONNRESET:    // Connection reset
        case EINTR:         // Interrupted function call
#ifdef EHOSTDOWN
        case EHOSTDOWN:  // Host is down
#endif
        case EHOSTUNREACH:  // Host is unreachable
        case ENETDOWN:      // Network is down
        case ENETRESET:     // Connection aborted by network
        case ENETUNREACH:   // Network unreachable
        case ENOLCK:        // No locks available
        case ENOLINK:       // Link has been severed
#ifdef ENONET
        case ENONET:        // Machine is not on the network
#endif
            return StatusCode::kUnavailable;

            //! StatusCode::kAborted
        case EDEADLK:  // Resource deadlock avoided
#ifdef ESTALE
        case ESTALE:   // Stale file handle
#endif
            return StatusCode::kAborted;

            //! StatusCode::kCancelled
        case ECANCELED:  // Operation cancelled
            return StatusCode::kCancelled;

            //! StatusCode::kCancelled
        default:
            return StatusCode::kUnknown;
    }
}

/********************************* Status *********************************/
Status::Status()
        : data_{ new StatusBase{StatusCode::kOk, ""} } {}

Status::Status(StatusCode code, const std::string& message) //NOLINT
        : data_{ new StatusBase{code, message} } {}

Status::Status(const Status& other)
        : data_{ new StatusBase{other.data_->code_, other.data_->message_} } {}

Status& Status::operator=(const Status& other) {
    if (&other != this)
        *data_ = *other.data_; // 调用StatusBase的copy-assign
    return *this;
}

Status::Status(Status&& other) noexcept {
    this->data_ = other.data_; // ①指向别人的资源
    other.data_ = nullptr;     // ②将别人的指针置空以移动所有权
}

Status& Status::operator=(Status&& other) noexcept {
    if(&other != this) {
        delete data_;              // ①删除自己的资源
        this->data_ = other.data_; // ②指向别人的资源
        other.data_ = nullptr;     // ③将别人的指针置空以移动所有权
    }
    return *this;
}

Status::~Status()
{
    delete data_;
    data_ = nullptr;
}

std::string Status::ToString() const {
    return this->ok() ? "OK" : (data_->code_.ToString() + ": " + data_->message_);
}

Status ErrnoToStatus(int errnum, const std::string& message)
{
    return Status{ErrnoToStatusCode(errnum),
                  std::string(message)+": "+ StrError(errnum)};
}







}
#pragma clang diagnostic pop
