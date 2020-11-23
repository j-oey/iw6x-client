#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "scheduler.hpp"
#include "game/game.hpp"
#include "utils/hook.hpp"

#include <breakpoint.h>

	extern "C" int bps = false;

namespace arxan
	{

	namespace
	{
		typedef struct _OBJECT_HANDLE_ATTRIBUTE_INFORMATION
		{
			BOOLEAN Inherit;
			BOOLEAN ProtectFromClose;
		} OBJECT_HANDLE_ATTRIBUTE_INFORMATION;

		utils::hook::detour nt_close_hook;
		utils::hook::detour nt_query_information_process_hook;

		NTSTATUS WINAPI nt_query_information_process_stub(const HANDLE handle, const PROCESSINFOCLASS info_class,
		                                                  PVOID info,
		                                                  const ULONG info_length, const PULONG ret_length)
		{
			auto* orig = static_cast<decltype(NtQueryInformationProcess)*>(nt_query_information_process_hook.
				get_original());
			const auto status = orig(handle, info_class, info, info_length, ret_length);

			if (NT_SUCCESS(status))
			{
				if (info_class == ProcessBasicInformation)
				{
					static DWORD explorerPid = 0;
					if (!explorerPid)
					{
						auto* const shell_window = GetShellWindow();
						GetWindowThreadProcessId(shell_window, &explorerPid);
					}

					static_cast<PPROCESS_BASIC_INFORMATION>(info)->Reserved3 = PVOID(DWORD64(explorerPid));
				}
				else if (info_class == 30) // ProcessDebugObjectHandle
				{
					*static_cast<HANDLE*>(info) = nullptr;

					return 0xC0000353;
				}
				else if (info_class == 7) // ProcessDebugPort
				{
					*static_cast<HANDLE*>(info) = nullptr;
				}
				else if (info_class == 31)
				{
					*static_cast<ULONG*>(info) = 1;
				}
			}

			return status;
		}

		NTSTATUS NTAPI nt_close_stub(const HANDLE handle)
		{
			char info[16];
			if (NtQueryObject(handle, OBJECT_INFORMATION_CLASS(4), &info, sizeof(OBJECT_HANDLE_ATTRIBUTE_INFORMATION),
			                  nullptr) >= 0)
			{
				auto* orig = static_cast<decltype(NtClose)*>(nt_close_hook.get_original());
				return orig(handle);
			}

			return STATUS_INVALID_HANDLE;
		}

		jmp_buf* get_buffer()
		{
			static thread_local jmp_buf old_buffer;
			return &old_buffer;
		}

#pragma warning(push)
#pragma warning(disable: 4611)
		void reset_state()
		{
			game::longjmp(get_buffer(), -1);
		}
#pragma warning(pop)

		size_t get_reset_state_stub()
		{
			static auto* stub = utils::hook::assemble([](utils::hook::assembler& a)
			{
				a.sub(rsp, 0x10);
				a.or_(rsp, 0x8);
				a.jmp(reset_state);
			});

			return reinterpret_cast<size_t>(stub);
		}

	bool luuul = false;

		LONG WINAPI exception_filter(const LPEXCEPTION_POINTERS info)
		{
			if (info->ExceptionRecord->ExceptionCode == STATUS_INVALID_HANDLE)
			{
				return EXCEPTION_CONTINUE_EXECUTION;
			}
			
			if (info->ExceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION)
			{
				const auto address = reinterpret_cast<size_t>(info->ExceptionRecord->ExceptionAddress);
				if((address & ~0xFFFFFFF) == 0x280000000)
				{
					//MessageBoxA(nullptr, "Arxan Exception", "Oh fuck.", MB_ICONERROR);

					info->ContextRecord->Rip = get_reset_state_stub();
					return EXCEPTION_CONTINUE_EXECUTION;
				}
			}

			if(info->ExceptionRecord->ExceptionCode ==STATUS_SINGLE_STEP && luuul ) {
				//if(arxan::bps)
				{
					//MessageBoxA(0,"Single",0,0);
					printf("Ex: %p %d\n", info->ExceptionRecord->ExceptionAddress, (int)bps);
				}
				return EXCEPTION_CONTINUE_EXECUTION;
			}
			
			return EXCEPTION_CONTINUE_SEARCH;
		}

		void hide_being_debugged()
		{
			auto* const peb = PPEB(__readgsqword(0x60));
			peb->BeingDebugged = false;
			*PDWORD(LPSTR(peb) + 0xBC) &= ~0x70;
		}

		void remove_hardware_breakpoints()
		{
			CONTEXT context;
			ZeroMemory(&context, sizeof(context));
			context.ContextFlags = CONTEXT_DEBUG_REGISTERS;

			auto* const thread = GetCurrentThread();
			GetThreadContext(thread, &context);

			context.Dr0 = 0;
			context.Dr1 = 0;
			context.Dr2 = 0;
			context.Dr3 = 0;
			context.Dr6 = 0;
			context.Dr7 = 0;

			SetThreadContext(thread, &context);
		}

		BOOL WINAPI set_thread_context_stub(const HANDLE thread, CONTEXT* context)
		{
			if (!game::environment::is_sp()
				&& game::dwGetLogOnStatus(0) == game::DW_LIVE_CONNECTED
				&& context->ContextFlags == CONTEXT_DEBUG_REGISTERS)
			{
				return TRUE;
			}

			return SetThreadContext(thread, context);
		}

		void dw_frame_stub(const int index)
		{
			const auto status = game::dwGetLogOnStatus(index);

			if (status == game::DW_LIVE_CONNECTING)
			{
				// dwLogOnComplete
				reinterpret_cast<void(*)(int)>(0x1405894D0)(index);
			}
			else if (status == game::DW_LIVE_DISCONNECTED)
			{
				// dwLogOnStart
				reinterpret_cast<void(*)(int)>(0x140589E10)(index);
			}
			else
			{
				// dwLobbyPump
				//reinterpret_cast<void(*)(int)>(0x1405918E0)(index);

				// DW_Frame
				reinterpret_cast<void(*)(int)>(0x14000F9A6)(index);
			}
		}
	}

    inline void SetBits(ULONG_PTR& dw, int lowBit, int bits, int newValue)
    {
        int mask = (1 << bits) - 1; // e.g. 1 becomes 0001, 2 becomes 0011, 3 becomes 0111

        dw = (dw & ~(mask << lowBit)) | (newValue << lowBit);
    }

	void setBP(void* addr)
	{
		CONTEXT cxt;
            cxt.ContextFlags = CONTEXT_DEBUG_REGISTERS;
			auto hThread = GetCurrentThread();
		
           GetThreadContext(hThread, &cxt);

auto index = 0;
                SetBits(cxt.Dr7, index * 2, 1, 1);


                    switch (index)
                    {
                    case 0: cxt.Dr0 = (DWORD_PTR)addr; break;
                    case 1: cxt.Dr1 = (DWORD_PTR)addr; break;
                    case 2: cxt.Dr2 = (DWORD_PTR)addr; break;
                    case 3: cxt.Dr3 = (DWORD_PTR)addr; break;
                    }

                    SetBits(cxt.Dr7, 16 + (index * 4), 2, 1);
                    SetBits(cxt.Dr7, 18 + (index * 4), 2, 8);

           SetThreadContext(hThread, &cxt);
	}

	void* addr;

	void install_lul(void* lul)
	{
		/*setBP(lul);
		scheduler::once([lul]() {
			setBP(lul);
		}, scheduler::pipeline::server);*/
		auto* xx = &bps;
		
		auto x = LoadLibraryA("PhysXDevice64.dll");
		auto y = LoadLibraryA("PhysXUpdateLoader64.dll");

		scheduler::once([lul]() {
			//HWBreakpoint::Set(lul, HWBreakpoint::Condition::Write);
		}, scheduler::pipeline::async);
	}

	void* get_stack_backup()
	{
		static thread_local char backup[0x1000];
		return backup;
	}
	
	void backup_stack(void* addr)
	{
		memcpy(get_stack_backup(), addr, 0x1000);
	}

	void restore_stack(void* addr)
	{
		memcpy(addr, get_stack_backup(), 0x1000);
	}

#pragma warning(push)
#pragma warning(disable: 4611)
	int save_state_intenal()
	{
		static bool installed = false;
		if(!installed){
			installed = true;
		install_lul(_AddressOfReturnAddress());
			//AddVectoredExceptionHandler(1, exception_filter);
		}
		bps = true;
		luuul = true;
		addr = _AddressOfReturnAddress();
		printf("Pre: %p %p\n",_AddressOfReturnAddress(),  _ReturnAddress());

		backup_stack(_AddressOfReturnAddress());
		
		const auto recovered = game::_setjmp(get_buffer());
		if(recovered)
		{
			restore_stack(_AddressOfReturnAddress());
			
			printf("Recovering from arxan error...\n");
			MessageBoxA(0,0,0,0);
		}
		printf("Post: %p %p\n",_AddressOfReturnAddress(),  _ReturnAddress());

		bps = false;
		//HWBreakpoint::ClearAll();
		return recovered;
	}
#pragma warning(pop)

	bool save_state()
	{
		return save_state_intenal() != 0;
	}

	void* memmv( void* _Dst, void const* _Src,  size_t _Size)
    {
		if(size_t(_Dst) <= size_t(addr) &&size_t(_Dst) + _Size >= size_t(addr) && bps) 
		{
			printf("OK");
		}
		
	   return memmove(_Dst, _Src, _Size);
    }

	class component final : public component_interface
	{
	public:
		void* load_import(const std::string& library, const std::string& function) override
		{
			if (function == "SetThreadContext")
			{
				return set_thread_context_stub;
			}

			return nullptr;
		}

		void post_load() override
		{
			hide_being_debugged();
			scheduler::loop(hide_being_debugged, scheduler::pipeline::async);

			const utils::nt::library ntdll("ntdll.dll");
			nt_close_hook.create(ntdll.get_proc<void*>("NtClose"), nt_close_stub);
			nt_query_information_process_hook.create(ntdll.get_proc<void*>("NtQueryInformationProcess"),
			                                         nt_query_information_process_stub);

			AddVectoredExceptionHandler(1, exception_filter);
		}

		void post_unpack() override
		{
			// cba to implement sp, not sure if it's even needed
			if (game::environment::is_sp()) return;

			utils::hook::jump(0x1404FE1E0, 0x1404FE2D0); // idk
			utils::hook::jump(0x140558C20, 0x140558CB0); // dwNetPump
			utils::hook::jump(0x140591850, 0x1405918E0); // dwLobbyPump
			utils::hook::jump(0x140589480, 0x140589490); // dwGetLogonStatus
			
			utils::hook::jump(0x140730160, memmv);

			// These two are inlined with their synchronization. Need to work around that
			//utils::hook::jump(0x14015EB9A, 0x140589E10); // dwLogOnStart
			//utils::hook::call(0x140588306, 0x1405894D0); // dwLogOnComplete

			// Unfinished for now
			//utils::hook::jump(0x1405881E0, dw_frame_stub);

			scheduler::on_game_initialized(remove_hardware_breakpoints, scheduler::pipeline::main);
		}
	};
}

REGISTER_COMPONENT(arxan::component)
