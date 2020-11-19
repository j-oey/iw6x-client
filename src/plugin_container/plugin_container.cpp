#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdlib>
#include <string>

#define IPC_IMPLEMENTATION
#include <ipc.h>

#define WORKUNITS 1024
#define WORKUNITSIZE (64*64)
#define TOTALSIZE WORKUNITS+WORKUNITS*WORKUNITSIZE+1

int __stdcall WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
	ipc_sharedsemaphore coop;
	ipc_sharedmemory mem;

	std::string sem_name{"ipc_test_semaphore"};
	std::string mem_name{"ipc_test_memory"};

	ipc_sem_init(&coop, sem_name.data());
	ipc_mem_init(&mem, mem_name.data(), TOTALSIZE);

	ipc_mem_close(&mem);
	ipc_sem_close(&coop);

	return 1;
}
