#include <pthread.h>

#include <iostream>
#include <tuple>

using namespace std;

#ifdef __linux__
// size_t GetStackSize() {
//   size_t stack_size{};
//   pthread_attr_t attr;
//   pthread_attr_init(&attr);
//   pthread_attr_getstacksize(&attr, &stack_size);
//   pthread_attr_destroy(&attr);

//   return stack_size;
// }

std::tuple<void*, size_t> GetStackBaseAndStackSize() {
  pthread_attr_t attr;
  void* stack_base{};
  size_t stack_size{};

  pthread_getattr_np(pthread_self(), &attr);

  pthread_attr_getstack(&attr, &stack_base, &stack_size);

  pthread_attr_destroy(&attr);

  return std::make_tuple(stack_base, stack_size);
}

#elif __MACH__
std::tuple<void*, size_t> GetStackBaseAndStackSize() {
  pthread_t self = pthread_self();

  return std::make_tuple(
      pthread_get_stackaddr_np(self),
      pthread_get_stacksize_np(self));
}
#elif _WIN32
size_t GetStackSize() {
  return 0;
}

void* GetStackStart() {
  return nullptr;
}
#endif

#if defined(__x86_64__) || defined(_M_X64)
// For this architecture stack grows downward (from highest
// address to lowest).
// (check_line_length skip)
// https://eli.thegreenplace.net/2011/02/04/where-the-top-of-the-stack-is-on-x86/
// So the stack end will be the lowest address.

#ifdef __linux__
void* GetStackStart(
    void* stack_base,
    const size_t stack_size,
    size_t* latest_local_variable) {
  return (char*) stack_base + stack_size;
}

void* GetStackEnd(
    void* stack_base,
    const size_t stack_size,
    size_t* latest_local_variable) {
  return stack_base;
}
#endif

#ifdef __MACH__
void* GetStackStart(
    void* stack_base,
    const size_t stack_size,
    size_t* latest_local_variable) {
  if (latest_local_variable > stack_base) {
    return (char*) stack_base - stack_size;
  } else {
    return stack_base;
  }
}

void* GetStackEnd(
    void* stack_base,
    const size_t stack_size,
    size_t* latest_local_variable) {
  if (latest_local_variable > stack_base) {
    return stack_base;
  } else {
    return (char*) stack_base - stack_size;
  }
}
#endif

size_t Available(void* stack_end, size_t* latest_local_variable) {
  return ((char*) latest_local_variable)
      - ((char*) stack_end) - sizeof(latest_local_variable);
}

#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
// fail beacause we might add the implementation in future
#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
// ...
#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
// ...
#elif defined(__sparc__) || defined(__sparc)
#elif defined(/*ARM*/)
// For this architecture stack grows downward.
// https://docs.oracle.com/cd/E18752_01/html/816-5138/advanced-2.html
void* GetStackEnd(void* stack_start, const size_t stack_size) {
  return static_cast<char*>(stack_start) + stack_size;
}
#endif


int main(int argc, char** argv) {
  // size_t stack_size = GetStackSize();
  // void* stack_start = GetStackStart();
  // void* stack_end = GetStackEnd(stack_start, stack_size);

  // void *stack_start, *stack_end, *stack_base;
  // size_t stack_size;
  int available{};
  size_t size = 90;

  auto [stack_base, stack_size] = GetStackBaseAndStackSize();
  void* stack_start = GetStackStart(stack_base, stack_size, &size);
  void* stack_end = GetStackEnd(stack_base, stack_size, &size);
  available = Available(stack_end, &size);

  // #ifdef __linux__
  //   pthread_attr_t attr;

  //   pthread_getattr_np(pthread_self(), &attr);

  //   pthread_attr_getstack(&attr, &stack_end, &stack_size);

  //   stack_start = (char*) stack_end + stack_size;
  // #endif

  // #ifdef __MACH__
  //   pthread_t self = pthread_self();

  //   stack_base = pthread_get_stackaddr_np(self);
  //   stack_size = pthread_get_stacksize_np(self);

  //   if (&size > stack_base) {
  //     stack_end = stack_base;
  //     stack_start = (char*) stack_base - stack_size;
  //     available = ((char*) &size) - ((char*) stack_end) - sizeof(size);
  //   } else {
  //     stack_start = stack_base;
  //     stack_end = (char*) stack_start - stack_size;
  //     available = ((char*) &size) - ((char*) stack_end) - sizeof(size);
  //   }
  // #endif

  // #ifdef __linux__
  //   // stack_end = (char*) stack_start - stack_size;
  //   // available = ((char*) &size) - ((char*) stack_end) - sizeof(size);
  // #endif

  // #ifdef __MACH__
  //   // stack_end = (char*) stack_start + stack_size;
  //   // available = ((char*) &size) - ((char*) stack_end) - sizeof(size);
  // #endif

  // size_t available = ((char*) &size) - ((char*) stack_start);
  // available = ((char*) stack_end) - ((char*) &size);
  size_t size2 = 90;

  //  foo();

  std::cout << "stack_size = " << stack_size << std::endl;
  std::cout << "stack_start = " << stack_start << std::endl;
  std::cout << "stack_end = " << stack_end << std::endl;
  std::cout << "size = " << &size << std::endl;
  std::cout << "size2 = " << &size2 << std::endl;
  std::cout << "stack_start - stack_end = " << (char*) stack_start - (char*) stack_end << std::endl;
  std::cout << "size - size2 = " << &size - &size2 << std::endl;
  std::cout << "available = " << available << std::endl;
  //  std::cout << getBuild() << std::endl;

  // macos: st_e = st_s + stack_size
  // 140 732 898 152 448 - st_e
  // 140 732 889 757 996 - size
  // 140 732 889 763 840 - st_s

  // macos: st_e = st_s - stack_size
  // 140 732 735 488 000 - s_e
  // 140 732 743 876 608 - s_s
  // 140 732 735 482 160 - local var

  // linux: st_e = st_s - stack_size
  // 140 737 331 187 712 - st_s
  // 140 737 322 803 200 - st_e
  // 140 737 339 568 924 - size
  // 140 722 897 199 104 - s_e
  // 140 722 905 576 476 - size_adddr
#ifdef __linux__
  // pthread_attr_destroy(&attr);
#endif

  return 0;
}
