#include "wrap_epoll.h"
#include "log.h"
#include "status/Status.h"
#include <cerrno>

namespace yy::util {

#ifdef ____DEBUG
#define FATAL_ERROR_EPOLL(cond, funname, fd) \
if( cond ){ \
    YLOG_FATAL("GetFD<%d> "#funname"() error: %s", fd, StatusCode{errno}.ToString().c_str()); \
    std::__throw_system_error(errno); \
}
#else
#define FATAL_ERROR_EPOLL(cond, funname, fd) \
if( cond ){ \
    YLOG_FATAL("fd<%d> "#funname"() error: %s", fd, StatusCode{errno}.ToString().c_str()); \
}
#endif




EpollList::EpollList(size_t epoll_size)
        : epoll_fd_{::epoll_create1(EPOLL_CLOEXEC)},
          epoll_arr_{kEpollMaxSize}
{
    if (epoll_fd_ < 0) {
        fprintf(stderr,"epoll_create1() error");
    }
}

EpollList::~EpollList()
{
    close(epoll_fd_);
}

EpollPollerEvent &EpollList::operator[](size_t idx)
{
    if (idx >= epoll_arr_.size())
        throw std::range_error("EpollList::operator[] wrong index！");
    return epoll_arr_[idx];
}

void EpollList::add_fd(const FileDescriper & fd, EPOLL_EVENTS event)
{
    add_fd(fd, {event});
}

void EpollList::add_fd(const FileDescriper & fd, std::initializer_list<EPOLL_EVENTS> events)
{
    EpollPollerEvent epe{fd};
    for (auto event: events) {
        if (event == EPOLLET)
            fd.SetNonblocking();
        epe.add_event(event);
    }
    int ret = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd.get_fd(), &(epe.epoll_event_));
    FATAL_ERROR_EPOLL(ret < 0, epoll_ctl_add, fd.get_fd())
}

void EpollList::add_fd(EpollPollerEvent &event) {
    auto ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event.get_fd(), &event.epoll_event_);
    FATAL_ERROR_EPOLL(ret < 0, EPOLL_CTL_ADD, event.get_fd())
}

void EpollList::del_fd(const FileDescriper & fd)
{
    int ret = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd.get_fd(), nullptr);
    FATAL_ERROR_EPOLL(ret < 0, epoll_ctl_del, fd.get_fd())
}


void EpollList::del_fd(const FileDescriper & fd, EPOLL_EVENTS event)
{
    del_fd(fd, {event});
}

void EpollList::del_fd(const FileDescriper & fd, std::initializer_list<EPOLL_EVENTS> events)
{
    EpollPollerEvent epe{};
    for (auto event: events) {
        epe.add_event(event);
    }

    int ret = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd.get_fd(), &(epe.epoll_event_));
    FATAL_ERROR_EPOLL(ret < 0, epoll_ctl_del, fd.get_fd())
}

void EpollList::del_fd(EpollPollerEvent &event) {
    auto ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, event.get_fd(), &event.epoll_event_);
    FATAL_ERROR_EPOLL(ret < 0, EPOLL_CTL_DEL, event.get_fd())
}

void EpollList::mod_fd(EpollPollerEvent &event) {
    auto ret = ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, event.get_fd(), &event.epoll_event_);
    FATAL_ERROR_EPOLL(ret < 0, EPOLL_CTL_MOD, event.get_fd())
}

size_t EpollList::wait()
{
    return wait_timeout(-1);
}

size_t EpollList::wait_timeout(int timeout)
{
    int numEvents = epoll_wait(
            epoll_fd_,
            &(epoll_arr_.begin()->epoll_event_),
            (int) epoll_arr_.size(),
            timeout);

    if (numEvents < 0) {
        if (errno == EINTR)
            return 0;
        else
            YLOG_FATAL("epoll_wait() error: %s", StatusCode{errno}.ToString().c_str())
    } else if (numEvents == 0) {
        YLOG_INFO("epoll_wait() nothing happened")
    } else {
        epoll_arr_.resize(static_cast<size_t>(numEvents * 2)); // NOLINT(bugprone-misplaced-widening-cast)
    }
    return numEvents;
}




}