//===----------------------------------------------------------------------===//
//
//                        JFS - The JIT Fuzzing Solver
//
// Copyright 2017 Daniel Liew
//
// This file is distributed under the MIT license.
// See LICENSE.txt for details.
//
//===----------------------------------------------------------------------===//
#include "jfs/Support/ScopedTimer.h"
#include <chrono>
#include <condition_variable>
#include <thread>

namespace jfs {
namespace support {

class ScopedTimerImpl {
private:
  uint64_t maxTime;
  ScopedTimer::CallBackTy callBack;
  std::unique_ptr<std::thread> waiter;
  std::condition_variable cv;
  std::mutex cvMutex;
  bool realWakeUp;

public:
  void waiterFunction() {
    std::chrono::seconds timeout(maxTime);
    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + timeout;
    std::unique_lock<std::mutex> lock(cvMutex);
    // Loop just in case there are spurious wake ups
    while (true) {
      std::cv_status status = cv.wait_until(lock, endTime);
      if (status == std::cv_status::timeout) {
        callBack();
        break;
      }
      if (realWakeUp) {
        break;
      }
      // Spurious wake up. Just sleep again
    }
  }
  ScopedTimerImpl(uint64_t maxTime, ScopedTimer::CallBackTy callBack)
      : maxTime(maxTime), callBack(callBack), waiter(nullptr),
        realWakeUp(false) {
    if (maxTime == 0) {
      return;
    }
    waiter.reset(new std::thread(&ScopedTimerImpl::waiterFunction, this));
  }
  ~ScopedTimerImpl() {
    if (waiter) {
      {
        std::lock_guard<std::mutex> lock(cvMutex);
        // Wake up the thread if it's still running
        realWakeUp = true;
      }
      cv.notify_one();
      waiter->join();
    }
  }
};

ScopedTimer::ScopedTimer(uint64_t maxTime, CallBackTy callBack)
    : impl(new ScopedTimerImpl(maxTime, callBack)) {}

ScopedTimer::~ScopedTimer() {
  // Let ~ScopedTimerImpl() do the work
}
}
}