#include <iostream>
#include <fstream>

#if defined(_WIN32)
#include <windows.h>

void init_wait() {

}

void wait_for_signal() {
    HANDLE hMutex;

    while (true) {
        hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, TEXT("Global\\KALDI_ALIGNER"));
        if (hMutex) {
            break;
        }
        Sleep(500);
    }
    CloseHandle(hMutex);
}


void do_signal() {
    HANDLE hMutex;
    hMutex=CreateMutex(NULL,FALSE,TEXT("Global\\KALDI_ALIGNER"));
    if (hMutex) {
        Sleep(750);
        CloseHandle(hMutex);
    }
}
#else
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
#endif