#include "Acceptor.h"
#include "EventLoop.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      acceptSocket_(),
      acceptChannel_(loop, acceptSocket_.fd()),
      listening_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bind(listenAddr);

    acceptChannel_.setReadCallback(
        std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    if (idleFd_ >= 0) {
        ::close(idleFd_);
    }
}

void Acceptor::listen() {
    loop_->assertInLoopThread();
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
    loop_->assertInLoopThread();
    InetAddress peerAddr;

    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr);
        } else {
            ::close(connfd);
        }
    } else {
        // Handle fd exhaustion: close the idle fd, accept again, then reopen idle fd
        if (errno == EMFILE || errno == ENFILE) {
            std::cerr << "[Acceptor] Too many open files, closing idle fd" << std::endl;
            ::close(idleFd_);
            idleFd_ = acceptSocket_.accept(&peerAddr);
            if (idleFd_ >= 0) {
                ::close(idleFd_);
            }
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}
