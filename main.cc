#include <iostream>
#include <pthread.h>

using namespace std;

int main(int argc, char **argv) {
  char dummy[1000];
  pthread_t self = pthread_self();

  void *stack_start = pthread_get_stackaddr_np(self);
  size_t stack_size = pthread_get_stacksize_np(self);
  void *stack_end = (char *)stack_start - stack_size;

  size_t size = sizeof(dummy);

  size_t available = ((char *)&size) - ((char *)stack_end);

  std::cout << "available = " << available << std::endl;

  return 0;
}
