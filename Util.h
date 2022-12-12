#ifndef _UTIL_
#define _UTIL_

#include <atomic>
#include <functional>
#include <future>
#include <thread>
#include <chrono>

/**
 * 测试这个并没有异步，很奇怪不知道为什么
 */

template <typename F, typename ... Args>
void setInterval(
   std::atomic_bool& cancelToken, 
   size_t interval, 
   F&& f, 
   Args&&... args
)
{
  cancelToken.store(true);
  auto cb = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

  std::async(std::launch::async , [=, &cancelToken]() mutable
   {
      while (cancelToken.load())
      {
         cb();
         std::this_thread::sleep_for(std::chrono::seconds(interval));
      }
   }
  );
}

#endif
