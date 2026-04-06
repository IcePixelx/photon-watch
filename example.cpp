constexpr size_t USUAL_PAGE_SIZE = 0x1000;
typedef struct _MEMORY_RANGE_ENTRY
{
	PVOID  VirtualAddress;
	SIZE_T NumberOfBytes;
} MEMORY_RANGE_ENTRY, * PMEMORY_RANGE_ENTRY;

typedef enum _VIRTUAL_MEMORY_INFORMATION_CLASS
{
	VmPrefetchInformation,
	VmPagePriorityInformation,
	VmCfgCallTargetInformation,
	VmPageDirtyStateInformation,
	VmImageHotPatchInformation,
	VmPhysicalContiguityInformation,
	VmVirtualMachinePrepopulateInformation,
	VmRemoveFromWorkingSetInformation,
	MaxVmInfoClass
} VIRTUAL_MEMORY_INFORMATION_CLASS;

typedef NTSTATUS(NTAPI* NtSetInformationVirtualMemory_t)(HANDLE ProcessHandle, VIRTUAL_MEMORY_INFORMATION_CLASS VmInformationClass, ULONG_PTR NumberOfEntries, PMEMORY_RANGE_ENTRY VirtualAddresses, PVOID VmInformation, ULONG VmInformationLength);

inline ULONG_PTR AddressToPageIndex(const ULONG_PTR addr)
{
	return (addr >> 12);
}

static PPSAPI_WORKING_SET_INFORMATION QueryWorkingSetRetry()
{
	DWORD bufSize = sizeof(PSAPI_WORKING_SET_INFORMATION);
	void* buf = malloc(bufSize);
	if (buf == nullptr)
	{
		// Insert panic code here
		return nullptr;
	}

	while (true)
	{
		PPSAPI_WORKING_SET_INFORMATION pwsi = reinterpret_cast<PPSAPI_WORKING_SET_INFORMATION>(buf);

		const bool ok = K32QueryWorkingSet(GetCurrentProcess(), pwsi, bufSize);
		if (ok)
		{
			return pwsi;
		}

		if (GetLastError() != ERROR_BAD_LENGTH)
		{
			// Insert panic code here
			free(buf);
			return nullptr;
		}

		// Could do numOfEntries but blegh, we like eating memory
		bufSize *= 2;

		void* const newBuf = realloc(buf, bufSize);
		if (newBuf == nullptr)
		{
			// insert panic code here
			free(buf);
			return nullptr;
		}

		buf = newBuf;
	}
}

int main(int argc, char* argv[])
{
	auto PrintIfPageIndexInWorkingSet = [](const ULONG_PTR pageIndex, const size_t testIt) -> void
	{
		bool foundPage = false;

		PPSAPI_WORKING_SET_INFORMATION pwsi = QueryWorkingSetRetry();
		for (int i = 0; i < pwsi->NumberOfEntries; i++)
		{
			PSAPI_WORKING_SET_BLOCK& wsb = pwsi->WorkingSetInfo[i];
			if (wsb.VirtualPage == pageIndex)
			{
				foundPage = true;
			}
		}
		free(pwsi);

		if (foundPage)
		{
			std::cout << "Found Page in working set on test " << testIt << ".\n";
		}
	};

	const LPVOID addr = VirtualAlloc(nullptr, USUAL_PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	std::cout << "Allocated Page at " << std::hex << addr << "\n";

	ULONG_PTR pageIndex = AddressToPageIndex(reinterpret_cast<ULONG_PTR>(addr));
	std::cout << "Translating Page to PageIndex at " << std::hex << pageIndex << "\n";

	// Force introducing the page to the working set by writing to it.
	memset(addr, 0x1, USUAL_PAGE_SIZE);

	PrintIfPageIndexInWorkingSet(pageIndex, 1);

	// Time to remove the page from the working set now, grab the export from ntdll & set the memory range for our addr.
	const HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	if (ntdll == nullptr)
	{
		// Insert panic code here
		return -1;
	}

	NtSetInformationVirtualMemory_t NtSetInformationVirtualMemory = reinterpret_cast<NtSetInformationVirtualMemory_t>(GetProcAddress(ntdll, "NtSetInformationVirtualMemory"));
	if (NtSetInformationVirtualMemory == nullptr)
	{
		// Insert panic code here
		return -1;
	}

	MEMORY_RANGE_ENTRY entry;
	entry.VirtualAddress = addr;
	entry.NumberOfBytes = USUAL_PAGE_SIZE;

	DWORD vmInfo = 0;
	NTSTATUS status = NtSetInformationVirtualMemory(GetCurrentProcess(), VmRemoveFromWorkingSetInformation, 1, &entry, &vmInfo, sizeof(vmInfo));
	if (status != STATUS_SUCCESS)
	{
		// Insert panic code here
		return -1;
	}

	PrintIfPageIndexInWorkingSet(pageIndex, 2);

	// Wait loop to launch external process, make it access our allocated address for testing.
	while (true)
	{
		if (GetAsyncKeyState(VK_F3))
		{
			break;
		}

		Sleep(1000);
	}

	PrintIfPageIndexInWorkingSet(pageIndex, 3);
	return 0;
}