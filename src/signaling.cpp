#include <iostream>
#include <fstream>
#include <signal.h>
#include <unistd.h>

void handle(int signal) {
}

void init_wait() {
  struct sigaction sa{};
  sa.sa_flags = SA_RESTART;
  sa.sa_handler = handle;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, 0);
}

void wait_for_signal() {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  int ret = 0;
  std::ofstream pid("proc.pid");
  pid << getpid() << std::endl;
  pid.close();
  std::cout << "Waiting for signal..." << std::endl;
  sigwait(&set, &ret);
  std::cout << "Continuing!" << std::endl;
}

void do_signal() {
  std::ifstream pidf("proc.pid");
  int pid;
  pidf >> pid;
  pidf.close();
  kill(pid, SIGUSR1);
}