
#include <filesystem>
#include <string>
#include <memory>
#include <variant>
#include <mutex>

#include <Windows.h>
#include <DbgEng.h>

#include <nethost.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>


#define EXT_MAJOR_VER 1
#define EXT_MINOR_VER 0

namespace fs = std::filesystem;

typedef HRESULT(CORECLR_DELEGATE_CALLTYPE* managed_extension_method)(IDebugClient*, PCSTR);


namespace {

const wchar_t* dotnet_lib_name = L"DbgXExt.Lib.dll";
const wchar_t* dotnet_lib_config = L"DbgXExt.Lib.runtimeconfig.json";
const wchar_t* dotnet_type = L"DbgXExt.Lib.Ext, DbgXExt.Lib";
const wchar_t* dotnet_delegate_type = L"DbgXExt.Lib.Ext+ExtEntry, DbgXExt.Lib";

fs::path basedir{};

load_assembly_and_get_function_pointer_fn loading_fn{};

struct dotnet_runtime_fptrs {
    hostfxr_initialize_for_runtime_config_fn init;
    hostfxr_get_runtime_delegate_fn get_delegate;
    hostfxr_close_fn close;
};

std::variant<HRESULT, dotnet_runtime_fptrs> load_hostfxr() {
    wchar_t buffer[MAX_PATH];
    size_t buffer_size = _countof(buffer);
    if (auto hr = get_hostfxr_path(buffer, &buffer_size, nullptr); FAILED(hr)) {
        return hr;
    }

    // Load hostfxr and get desired exports
    if (auto lib = ::LoadLibraryW(buffer); lib != nullptr) {
        auto init_fptr = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(::GetProcAddress(lib, "hostfxr_initialize_for_runtime_config"));
        if (init_fptr == nullptr) {
            return HRESULT_FROM_WIN32(::GetLastError());
        }
        auto get_delegate_fptr = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(::GetProcAddress(lib, "hostfxr_get_runtime_delegate"));
        if (get_delegate_fptr == nullptr) {
            return HRESULT_FROM_WIN32(::GetLastError());
        }
        auto close_fptr = reinterpret_cast<hostfxr_close_fn>(::GetProcAddress(lib, "hostfxr_close"));
        if (close_fptr == nullptr) {
            return HRESULT_FROM_WIN32(::GetLastError());
        }

        return dotnet_runtime_fptrs{ init_fptr, get_delegate_fptr, close_fptr };
    } else {
        return HRESULT_FROM_WIN32(::GetLastError());
    }
}

HRESULT initialize_dotnet_host() {
    if (auto vres = load_hostfxr(); std::holds_alternative<HRESULT>(vres)) {
        return std::get<HRESULT>(vres);
    } else {
        const auto& runtime_fptrs = std::get<dotnet_runtime_fptrs>(vres);

        auto create_host_handle = [&runtime_fptrs]() -> std::variant<HRESULT, hostfxr_handle> {
            const std::wstring config_path{ basedir / dotnet_lib_config };

            hostfxr_handle host_handle{};
            if (const auto hr = runtime_fptrs.init(config_path.c_str(), nullptr, &host_handle); FAILED(hr) || host_handle == nullptr) {
                return hr;
            }

            return host_handle;
        };

        if (auto vh = create_host_handle(); std::holds_alternative<HRESULT>(vh)) {
            return std::get<HRESULT>(vh);
        } else {
            std::unique_ptr<void, hostfxr_close_fn> cxt{ std::get<hostfxr_handle>(vh), runtime_fptrs.close };

            if (const auto hr = runtime_fptrs.get_delegate(cxt.get(), hdt_load_assembly_and_get_function_pointer, 
                    reinterpret_cast<void**>(&loading_fn)); SUCCEEDED(hr) && loading_fn != nullptr) {
                return S_OK;
            } else {
                return hr;
            }
        }
    }
}

} // namespace

extern "C" HRESULT CALLBACK DebugExtensionInitialize(PULONG version, PULONG flags) {
    *version = DEBUG_EXTENSION_VERSION(EXT_MAJOR_VER, EXT_MINOR_VER);
    *flags = 0;

    return initialize_dotnet_host();
}

extern "C" void CALLBACK DebugExtensionNotify([[maybe_unused]] ULONG notify, [[maybe_unused]] ULONG64 argument) {}

extern "C" void CALLBACK DebugExtensionUninitialize(void) {}

extern "C" HRESULT CALLBACK hello_world(IDebugClient * dbgclient, PCSTR args) {
    static managed_extension_method fn{};
    static std::once_flag called{};
    std::call_once(called, []() {
        const std::wstring dotnetlib_path{ basedir / dotnet_lib_name };
        if (auto hr = loading_fn(dotnetlib_path.c_str(), dotnet_type, L"HelloWorld",
            dotnet_delegate_type, nullptr, reinterpret_cast<void**>(&fn)); FAILED(hr)) {
            fn = nullptr;
        }
        });

    return fn ? fn(dbgclient, args) : E_FAIL;
}

extern "C" HRESULT CALLBACK hello_world2(IDebugClient * dbgclient, PCSTR args) {
    static managed_extension_method fn{};
    static std::once_flag called{};
    std::call_once(called, []() {
        const std::wstring dotnetlib_path{ basedir / dotnet_lib_name };
        if (auto hr = loading_fn(dotnetlib_path.c_str(), dotnet_type, L"HelloWorld2",
            dotnet_delegate_type, nullptr, reinterpret_cast<void**>(&fn)); FAILED(hr)) {
            fn = nullptr;
        }
        });

    return fn ? fn(dbgclient, args) : E_FAIL;
}

BOOL APIENTRY DllMain(HMODULE hModule, [[maybe_unused]] DWORD ul_reason_for_call, [[maybe_unused]] LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        wchar_t filepath[MAX_PATH];
        if (auto len = ::GetModuleFileName(hModule, filepath, _countof(filepath));
            len != 0) {
            basedir = fs::path{ std::wstring_view(filepath, len) }.parent_path();
        } else {
            return FALSE;
        }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
