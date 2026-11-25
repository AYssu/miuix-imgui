#pragma once

// Linux/Android host wake source. Own before starting producer threads; stop
// and join them before destroying it. The callback never calls ImGui.
class FrameWake {
  public:
    FrameWake();
    ~FrameWake();
    FrameWake(const FrameWake &) = delete;
    FrameWake &operator=(const FrameWake &) = delete;
    bool Available() const { return fd_>=0; }
    void Notify() const;
    static void Notify(void *user) {static_cast<FrameWake *>(user)->Notify();}
    bool Drain() const;
    bool Wait(double seconds,int input_fd=-1) const;
  private:
    int fd_=-1;
};
