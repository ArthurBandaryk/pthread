#include <pthread.h>

#include <iostream>
#include <tuple>

using namespace std;

namespace os {

////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
struct StackInfo {
  void* start = nullptr;
  void* end = nullptr;
  size_t size = 0;
  bool stack_grows_downward = true;
  inline size_t StackAvailable() {
    [[maybe_unused]] char local_var{};
#if defined(__x86_64__)
    return (&local_var)
        - ((char*) end) - sizeof(local_var);
#elif defined(__arm__) || defined(__arm64__) || defined(__aarch64__)
    if (stack_grows_downward) {
      return (&local_var) - ((char*) end) - sizeof(local_var);
    } else {
      return ((char*) end) - (&local_var);
    }
#else
    return Bytes{0};
#endif
  }
};

////////////////////////////////////////////////////////////////////////

// Function that returns true if the stack grows downward. If the stack
// grows upward - returns false.
inline bool StackGrowsDownward(size_t* var) {
  char local_var{};
  if (reinterpret_cast<char*>(var) < &local_var) {
    return false;
  } else {
    return true;
  }
}

////////////////////////////////////////////////////////////////////////

inline StackInfo GetStackInfo() {
  [[maybe_unused]] pthread_t self = pthread_self();
  void* stack_addr = nullptr;
  size_t size = 0;
  const bool stack_grows_downward = StackGrowsDownward(&size);
#ifdef __linux__
  pthread_attr_t attr;

  pthread_getattr_np(pthread_self(), &attr);


  pthread_attr_getstack(&attr, &stack_addr, &size);

  pthread_attr_destroy(&attr);

#endif // If Linux.

#ifdef __MACH__
  stack_addr = pthread_get_stackaddr_np(self);
  size = pthread_get_stacksize_np(self);
#endif // If macOS.

#if defined(__x86_64__)
#ifdef __linux__
  // Implementation for Windows might be added in future for this architecture.
  // For x86 stack grows downward (from highest address to lowest).
  // (check_line_length skip)
  // https://eli.thegreenplace.net/2011/02/04/where-the-top-of-the-stack-is-on-x86/
  // So the stack end will be the lowest address.
  // The function `pthread_attr_getstack` on Linux returns
  // the lowest stack address. Since the stack grows downward,
  // from the highest to the lowest address -> the stack
  // address from `pthread_attr_getstack` will be the end of
  // the stack.
  // https://linux.die.net/man/3/pthread_attr_getstack
  return StackInfo{
      /* start = */ static_cast<char*>(stack_addr) + size,
      /* end = */ stack_addr,
      /* size = */ size};
#endif // If Linux.

#ifdef __MACH__
  // For macOS `pthread_get_stackaddr_np` can return either the base
  // or the end of the stack.
  // Check 1274 line on the link below:
  // https://android.googlesource.com/platform/art/+/master/runtime/thread.cc

  // If the address of a local variable is greater than the stack
  // address from `pthread_get_stacksize_np` ->
  // `pthread_get_stacksize_np` returns the end of the stack since
  // on x86 architecture the stack grows downward (from highest to
  // lowest address).
  [[maybe_unused]] char local_var{};
  if (&local_var > stack_addr) {
    return StackInfo{
        /* start = */ static_cast<char*>(stack_addr) - size,
        /* end = */ stack_addr,
        /* size = */ size};
  } else {
    return StackInfo{
        /* start = */ stack_addr,
        /* end = */ static_cast<char*>(stack_addr) - size,
        /* size = */ size};
  }
#endif // If macOS.

#elif defined(__arm__) || defined(__arm64__) || defined(__aarch64__)
// Stack direction growth for arm architecture is selectable.
// (check_line_length skip)
// https://stackoverflow.com/questions/19070095/who-selects-the-arm-stack-direction
#ifdef __linux__
  // The function `pthread_attr_getstack` on Linux returns
  // the lowest stack address. So, `stack_addr` will always
  // have the lowest address.
  if (stack_grows_downward) {
    return StackInfo{
        /* start = */ static_cast<char*>(stack_addr) + size,
        /* end = */ stack_addr,
        /* size = */ size,
        stack_grows_downward};
  } else {
    return StackInfo{
        /* start = */ stack_addr,
        /* end = */ static_cast<char*>(stack_addr) + size,
        /* size = */ size,
        stack_grows_downward};
  }
#endif // If Linux.

#ifdef __MACH__
  char local_var{};

  // For macOS `stack_addr` can be either the base or the end of the stack.
  if (&local_var > stack_addr && stack_grows_downward) {
    return StackInfo{
        /* start = */ static_cast<char*>(stack_addr) - size,
        /* end = */ stack_addr,
        /* size = */ size,
        stack_grows_downward};
  } else if (&local_var > stack_addr && !stack_grows_downward) {
    return StackInfo{
        /* start = */ stack_addr,
        /* end = */ static_cast<char*>(stack_addr) + size,
        /* size = */ size,
        stack_grows_downward};
  } else if (stack_addr > &local_var && stack_grows_downward) {
    return StackInfo{
        /* start = */ stack_addr,
        /* end = */ static_cast<char*>(stack_addr) - size,
        /* size = */ size,
        stack_grows_downward};
  } else {
    return StackInfo{
        /* start = */ static_cast<char*>(stack_addr) - size,
        /* end = */ stack_addr,
        /* size = */ size,
        stack_grows_downward};
  }
#endif // If macOS.
#endif
}

////////////////////////////////////////////////////////////////////////

inline void CheckSufficientStackSpace(const size_t size) {
  // NOTE: making this a 'thread_local' so we only compute it once!
  static thread_local StackInfo stack_info = GetStackInfo();

  const size_t available = stack_info.StackAvailable();

  // NOTE: we determine sufficient stack space as follows. Assume
  // that for any continuation we may need at least two of them in
  // an unoptimized build, one for the caller and one for the
  // callee, plus we should have at least as much as a page size for
  // a buffer.
  bool has_sufficient_stack_space =
      available > (size * 2) + 4096;

  size_t size1 = 0;
  size_t size2 = 0;
  size_t size3 = 0;

  std::cout << "stack_size = " << stack_info.size << std::endl;
  std::cout << "stack_start = " << stack_info.start << std::endl;
  std::cout << "stack_end = " << stack_info.end << std::endl;
  std::cout << "size = " << &size1 << std::endl;
  std::cout << "size2 = " << &size2 << std::endl;
  std::cout << "size3 = " << &size3 << std::endl;
  std::cout << "stack_start - stack_end = " << (char*) stack_info.start - (char*) stack_info.end << std::endl;
  std::cout << "size - size2 = " << &size1 - &size2 << std::endl;
  std::cout << "size - size3 = " << &size1 - &size3 << std::endl;
  std::cout << "size - end = " << (char*) &size1 - (char*) (stack_info.end) << std::endl;
  std::cout << "size - start = " << (char*) &size1 - (char*) (stack_info.start) << std::endl;
  std::cout << "available = " << available << std::endl;
}
#else // If Windows.
inline void CheckSufficientStackSpace(const size_t size) {}
#endif

////////////////////////////////////////////////////////////////////////

} // namespace os


#ifndef _WIN32
struct StackInfo {
  void* start = nullptr;
  void* end = nullptr;
  size_t size = 0;
};

////////////////////////////////////////////////////////////////////////

inline StackInfo GetStackInfo(bool stack_grows_downward = true) {
#ifdef __linux__
  void* stack_addr = nullptr;
  size_t size = 0;
  pthread_attr_t attr;

  pthread_getattr_np(pthread_self(), &attr);

  pthread_attr_getstack(&attr, &stack_addr, &size);

  pthread_attr_destroy(&attr);
#endif // If Linux.

#ifdef __MACH__
  pthread_t self = pthread_self();

  // For macOS `pthread_get_stackaddr_np` can return either the base
  // or the end of the stack.
  // Check 1274 line on the link below:
  // https://android.googlesource.com/platform/art/+/master/runtime/thread.cc
  void* stack_addr = pthread_get_stackaddr_np(self);

  size_t size = pthread_get_stacksize_np(self);
#endif // If macOS.

#if defined(__x86_64__)
#ifdef __linux__
  // Implementation for Windows might be added in future for this architecture.
  // For x86 stack grows downward (from highest address to lowest).
  // (check_line_length skip)
  // https://eli.thegreenplace.net/2011/02/04/where-the-top-of-the-stack-is-on-x86/
  // So the stack end will be the lowest address.
  // The function `pthread_attr_getstack` on Linux returns
  // the lowest stack address. Since the stack grows downward,
  // from the highest to the lowest address -> the stack
  // address from `pthread_attr_getstack` will be the end of
  // the stack.
  // https://linux.die.net/man/3/pthread_attr_getstack
  void* end = stack_addr;
  void* start = static_cast<char*>(stack_addr) + size;
  [[maybe_unused]] char local_var{};
#endif // If Linux.

#ifdef __MACH__
  void* start = nullptr;
  void* end = nullptr;
  [[maybe_unused]] char local_var{};

  // If the address of a local variable is greater than the stack
  // address from `pthread_get_stacksize_np` ->
  // `pthread_get_stacksize_np` returns the end of the stack since
  // on x86 architecture the stack grows downward (from highest to
  // lowest address).
  if (&local_var > stack_addr) {
    start = static_cast<char*>(stack_addr) - size;
    end = stack_addr;
  } else {
    start = stack_addr;
    end = static_cast<char*>(stack_addr) - size;
  }
#endif // If macOS.
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
// Implementation might be added in future.
#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__)
  || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__)
      || defined(_ARCH_PPC)
// Implementation might be added in future.
#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
// Implementation might be added in future.
#elif defined(__sparc__) || defined(__sparc)
// Implementation might be added in future.
#elif defined(__arm__) || defined(__arm64__) || defined(__aarch64__)
  // Stack direction growth for arm architecture is selectable.
  // (check_line_length skip)
  // https://stackoverflow.com/questions/19070095/who-selects-the-arm-stack-direction
#ifdef __linux__
  void* start = nullptr;
  void* end = nullptr;

  // The function `pthread_attr_getstack` on Linux returns
  // the lowest stack address. So, `stack_addr` will always
  // have the lowest address.
  if (stack_grows_downward) {
    end = stack_addr;
    start = static_cast<char*>(stack_addr) + size;
  } else {
    start = stack_addr;
    end = static_cast<char*>(stack_addr) + size;
  }
#endif // If Linux.

#ifdef __MACH__
  void* start = nullptr;
  void* end = nullptr;
  char local_var{};

  // For macOS `stack_addr` can be either the base or the end of the stack.
  if (&local_var > stack_addr && stack_grows_downward) {
    start = static_cast<char*>(stack_addr) - size;
    end = stack_addr;
  } else if (&local_var > stack_addr && !stack_grows_downward) {
    end = static_cast<char*>(stack_addr) + size;
    start = stack_addr;
  } else if (stack_addr > &local_var && stack_grows_downward) {
    end = static_cast<char*>(stack_addr) - size;
    start = stack_addr;
  } else {
    end = stack_addr;
    start = static_cast<char*>(stack_addr) - size;
  }
#endif // If macOS.
#endif // If arm.

  return StackInfo{start, end, size};
}

////////////////////////////////////////////////////////////////////////

inline size_t StackAvailable(
    void* stack_start,
    void* stack_end,
    bool stack_grows_downward = true) {
  [[maybe_unused]] char local_var{};
#if defined(__x86_64__)
  return (&local_var)
      - ((char*) stack_end) - sizeof(local_var);
#elif defined(__arm__) || defined(__arm64__) || defined(__aarch64__)
  if (stack_grows_downward) {
    return (&local_var)
        - ((char*) stack_end) - sizeof(local_var);
  } else {
    return ((char*) stack_end) - (&local_var);
  }
#else
  return Bytes{0};
#endif
}

////////////////////////////////////////////////////////////////////////

// Function that returns true if the stack grows downward. If the stack
// grows upward - returns false.
inline bool StackGrowsDownward(char* var) {
  char local_var{};
  if (var < &local_var) {
    std::cout << "stack grows upward" << std::endl;
    return false;
  } else {
    std::cout << "stack grows downward" << std::endl;
    return true;
  }
}

////////////////////////////////////////////////////////////////////////

inline void CheckSufficientStackSpace(const size_t size) {
  // A simple char variable for checking growth direction of stack;
  char var{};
  static thread_local StackInfo stack_info =
      GetStackInfo(StackGrowsDownward(&var));

  const size_t available = StackAvailable(stack_info.start, stack_info.end);

  // NOTE: we determine sufficient stack space as follows. Assume
  // that for any continuation we may need at least two of them in
  // an unoptimized build, one for the caller and one for the
  // callee, plus we should have at least as much as a page size for
  // a buffer.
  bool has_sufficient_stack_space =
      available > (size * 2) + 4096;

  size_t size1 = 0;
  size_t size2 = 0;
  size_t size3 = 0;

  std::cout << "stack_size = " << stack_info.size << std::endl;
  std::cout << "stack_start = " << stack_info.start << std::endl;
  std::cout << "stack_end = " << stack_info.end << std::endl;
  std::cout << "size = " << &size1 << std::endl;
  std::cout << "size2 = " << &size2 << std::endl;
  std::cout << "size3 = " << &size3 << std::endl;
  std::cout << "stack_start - stack_end = " << (char*) stack_info.start - (char*) stack_info.end << std::endl;
  std::cout << "size - size2 = " << &size1 - &size2 << std::endl;
  std::cout << "size - size3 = " << &size1 - &size3 << std::endl;
  std::cout << "size - end = " << (char*) &size1 - (char*) (stack_info.end) << std::endl;
  std::cout << "size - start = " << (char*) &size1 - (char*) (stack_info.start) << std::endl;
  std::cout << "available = " << available << std::endl;
}
#else // If Windows.
inline void CheckSufficientStackSpace(const size_t size) {}
#endif

int main(int argc, char** argv) {
  // size_t stack_size = GetStackSize();
  // void* stack_start = GetStackStart();
  // void* stack_end = GetStackEnd(stack_start, stack_size);

  // void *stack_start, *stack_end, *stack_base;
  // size_t stack_size;
  os::CheckSufficientStackSpace(1000);
  // int available{};
  // size_t size = 90;

  // auto [stack_base, stack_size] = GetStackBaseAndStackSize();
  // void* stack_start = GetStackStart(stack_base, stack_size, &size);
  // void* stack_end = GetStackEnd(stack_base, stack_size, &size);
  // available = Available(stack_end, &size);

  // size_t size2 = 90;

  // std::cout << "stack_size = " << stack_size << std::endl;
  // std::cout << "stack_start = " << stack_start << std::endl;
  // std::cout << "stack_end = " << stack_end << std::endl;
  // std::cout << "size = " << &size << std::endl;
  // std::cout << "size2 = " << &size2 << std::endl;
  // std::cout << "stack_start - stack_end = " << (char*) stack_start - (char*) stack_end << std::endl;
  // std::cout << "size - size2 = " << &size - &size2 << std::endl;
  // std::cout << "available = " << available << std::endl;
  // //  std::cout << getBuild() << std::endl;

  // 140 721 354 944 320 - size1
  // 140 721 354 944 328
  return 0;
}
