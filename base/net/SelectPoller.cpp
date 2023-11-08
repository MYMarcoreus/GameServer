#include "SelectPoller.h"


namespace yy::net {


SelectPoller::SelectPoller(EventLoop *loop) : Poller(loop) {

}

SelectPoller::~SelectPoller() {

}

void SelectPoller::PollWait(Poller::ChannelList &activeChannel, int timeout) {

}

void SelectPoller::UpdateChannel(Channel *) {

}

void SelectPoller::RemoveChannel(Channel *) {

}

void SelectPoller::FillActiveChannel(int activeEventsNum, Poller::ChannelList *channelList) {

}

void SelectPoller::Update(Channel *channel) {

}
}