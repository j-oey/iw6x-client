#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "debugger.hpp"
#include <unordered_map>

namespace
{
	bool acquire_debug_privilege()
	{
		TOKEN_PRIVILEGES token_privileges;
		ZeroMemory(&token_privileges, sizeof(token_privileges));
		token_privileges.PrivilegeCount = 1;

		HANDLE token;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token))
		{
			return false;
		}

		if (!LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &token_privileges.Privileges[0].Luid))
		{
			CloseHandle(token);
			return false;
		}

		token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		DWORD size;
		if (!AdjustTokenPrivileges(token, FALSE, &token_privileges, 0, nullptr, &size))
		{
			CloseHandle(token);
			return false;
		}

		return CloseHandle(token) != FALSE;
	}
}

debugger::debugger(const unsigned long process_id, const bool start)
{
	if (!start)
	{
		return;
	}

	this->runner_ = std::thread([this, process_id]()
	{
		this->terminate_ = false;
		this->run(process_id);
	});
}

debugger::~debugger()
{
	this->terminate_ = true;
	if (this->runner_.joinable())
	{
		this->runner_.join();
	}
}

void set_single_step(DWORD thread_id)
{
	HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, false, thread_id);
	if (hThread)
	{
		CONTEXT context;
		context.ContextFlags = CONTEXT_ALL | CONTEXT_DEBUG_REGISTERS | CONTEXT_CONTROL;
		if (GetThreadContext(hThread, &context))
		{
			context.EFlags |= 0x100;
			SetThreadContext(hThread, &context);
		}
		CloseHandle(hThread);
	}
}

void* read_data(DWORD pid, void* addr)
{
	HANDLE hProcess = OpenProcess(PROCESS_VM_READ, 0, pid);
	ReadProcessMemory(hProcess, addr, &addr, sizeof(addr), 0);
	CloseHandle(hProcess);
	return addr;
}

static std::unordered_map<size_t, size_t> lulMap = {
	{0x140035EA7, 0x14A8BDC44},
	{0x14A0B52A8, 0x149D7DFCE},
	{0x14B1A892E, 0x14B00A068},
	{0x14AEF4F39, 0x14AAEE1FC},
};

void debugger::run(const unsigned long process_id) const
{
	acquire_debug_privilege();
	if (!DebugActiveProcess(process_id))
	{
		return;
	}

	//SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	static char data[0x10000];
	
	DEBUG_EVENT event;
	while (!this->terminate_ && WaitForDebugEvent(&event,INFINITE))
	{
		if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
		{
			if(event.u.Exception.ExceptionRecord.ExceptionCode == STATUS_ACCESS_VIOLATION)
			{
				snprintf(data, sizeof(data), "Violation: %p %p\n", event.u.Exception.ExceptionRecord.ExceptionAddress, (void*)event.u.Exception.ExceptionRecord.ExceptionInformation[1]);
				OutputDebugStringA(data);
				TerminateProcess(OpenProcess(PROCESS_ALL_ACCESS, false, event.dwProcessId), 1);
				OutputDebugStringA("================\n");
				OutputDebugStringA("We crashed\n");
				OutputDebugStringA("================\n");
				//ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
			}
			
			if (event.u.Exception.ExceptionRecord.ExceptionCode == STATUS_SINGLE_STEP)
			{
				static size_t trace = 0;
				static bool was_single_step = false;
				static void* lastAddr = 0;
				static bool should_log = false;

				auto addr = size_t(event.u.Exception.ExceptionRecord.ExceptionAddress);

				if (!trace && lulMap.find(addr) != lulMap.end())
				{
					snprintf(data, sizeof(data), "Starting execution trace: %p\n", event.u.Exception.ExceptionRecord.ExceptionAddress);
					OutputDebugStringA(data);
					trace = addr;
				}

				if (trace && addr == lulMap[trace])
				{
					trace = false;
					OutputDebugStringA("Stopping!\n");
					OutputDebugStringA("\n");
					OutputDebugStringA("\n");
					OutputDebugStringA("\n");
					OutputDebugStringA("\n");
				}

				if(should_log)
				{
					should_log= false;
					snprintf(data, sizeof(data), "Triggering: %p     %p\n", lastAddr, read_data(event.dwProcessId, (void*)0x13FE288));
					OutputDebugStringA(data);
				}

				if(!was_single_step)
				{
					lastAddr = event.u.Exception.ExceptionRecord.ExceptionAddress;
					should_log = true;
				}

				if (trace)
				{
					snprintf(data, sizeof(data), "--> %p\n", event.u.Exception.ExceptionRecord.ExceptionAddress);
					OutputDebugStringA(data);
				}

				was_single_step = false;

				if(trace|| should_log){
					set_single_step(event.dwThreadId);
					was_single_step = true;
				}

				ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
			}

			ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
			continue;
		}

		if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
		{
			ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
			break;
		}

#ifdef DEV_BUILD
		if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
		{
			//MessageBoxA(0,0,0,0);
			OutputDebugStringA("Debugger attached!\n");
		}
#endif

		ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
	}
}
