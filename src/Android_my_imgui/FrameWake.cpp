#include "FrameWake.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

FrameWake::FrameWake() : fd_(::eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC)) {}
FrameWake::~FrameWake() {if(fd_>=0) ::close(fd_);}
void FrameWake::Notify() const {
    if(fd_<0) return;
    std::uint64_t one=1;
    while(::write(fd_,&one,sizeof(one))<0 && errno==EINTR) {}
    // EAGAIN means the counter is already signalled; another write isn't needed.
}
bool FrameWake::Drain() const {
    if(fd_<0) return false;
    std::uint64_t count=0;ssize_t size;
    do {size=::read(fd_,&count,sizeof(count));} while(size<0 && errno==EINTR);
    return size==static_cast<ssize_t>(sizeof(count));
}
bool FrameWake::Wait(double seconds,int input_fd) const {
    if(fd_<0) return false; // Preserve continuous rendering if wake creation failed.
    double duration=std::isfinite(seconds) ? std::clamp(seconds,0.0,.1) : .1;
    pollfd descriptors[2]{{fd_,POLLIN,0},{input_fd,POLLIN,0}};
    return ::poll(descriptors,input_fd>=0 ? 2 : 1,static_cast<int>(std::ceil(duration*1000)))>0;
    // EINTR deliberately returns to the main loop, including termination.
}
