#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include <cstdarg>
#include <cstdio>
#include <io.h>

namespace {
FILE* g_result = nullptr;

void record(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
    std::fflush(stdout);

    if (g_result)
    {
        va_start(args, format);
        std::vfprintf(g_result, format, args);
        va_end(args);
        std::fflush(g_result);
        FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(g_result))));
    }
}

LONG write_exception(const char* source, EXCEPTION_POINTERS* info)
{
    const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    const void* address = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;
#if defined(_M_IX86) || defined(__i386__)
    const CONTEXT* context = info ? info->ContextRecord : nullptr;
    record("SOA_D3D9_PROBE_EXCEPTION source=%s code=0x%08lx address=%p "
           "eip=0x%08lx esp=0x%08lx ebp=0x%08lx eax=0x%08lx ebx=0x%08lx "
           "ecx=0x%08lx edx=0x%08lx esi=0x%08lx edi=0x%08lx\n",
           source, static_cast<unsigned long>(code), address,
           context ? static_cast<unsigned long>(context->Eip) : 0,
           context ? static_cast<unsigned long>(context->Esp) : 0,
           context ? static_cast<unsigned long>(context->Ebp) : 0,
           context ? static_cast<unsigned long>(context->Eax) : 0,
           context ? static_cast<unsigned long>(context->Ebx) : 0,
           context ? static_cast<unsigned long>(context->Ecx) : 0,
           context ? static_cast<unsigned long>(context->Edx) : 0,
           context ? static_cast<unsigned long>(context->Esi) : 0,
           context ? static_cast<unsigned long>(context->Edi) : 0);
#else
    record("SOA_D3D9_PROBE_EXCEPTION source=%s code=0x%08lx address=%p\n",
           source, static_cast<unsigned long>(code), address);
#endif
    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI vectored_handler(EXCEPTION_POINTERS* info)
{
    return write_exception("vectored", info);
}

LONG WINAPI unhandled_filter(EXCEPTION_POINTERS* info)
{
    write_exception("unhandled", info);
    return EXCEPTION_EXECUTE_HANDLER;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

int fail(const int code, const char* stage, const HRESULT result = S_OK)
{
    if (result == S_OK)
        record("SOA_D3D9_PROBE_FAIL stage=%s code=%d last_error=%lu\n",
               stage, code, static_cast<unsigned long>(GetLastError()));
    else
        record("SOA_D3D9_PROBE_FAIL stage=%s code=%d hr=0x%08lx last_error=%lu\n",
               stage, code, static_cast<unsigned long>(result),
               static_cast<unsigned long>(GetLastError()));
    return code;
}
}

int main(int argc, char** argv)
{
    if (argc >= 2)
        g_result = std::fopen(argv[1], "wb");

    SetUnhandledExceptionFilter(unhandled_filter);
    PVOID vectored = AddVectoredExceptionHandler(1, vectored_handler);

    record("SOA_D3D9_PROBE_BEGIN pid=%lu result_file=%s\n",
           static_cast<unsigned long>(GetCurrentProcessId()),
           argc >= 2 ? argv[1] : "<none>");

    HINSTANCE instance = GetModuleHandleA(nullptr);
    WNDCLASSA window_class{};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = "SOAD3D9Probe";
    if (!RegisterClassA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return fail(10, "RegisterClass");
    record("STAGE RegisterClass PASS\n");

    HWND window = CreateWindowA(window_class.lpszClassName, "SOA D3D9 Probe",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                320, 240, nullptr, nullptr, instance, nullptr);
    if (!window)
        return fail(11, "CreateWindow");
    record("STAGE CreateWindow PASS hwnd=%p\n", window);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    record("STAGE Direct3DCreate9 result=%p\n", static_cast<void*>(d3d));
    if (!d3d)
        return fail(20, "Direct3DCreate9");

    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window;
    present.BackBufferFormat = D3DFMT_UNKNOWN;
    present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9* device = nullptr;
    const HRESULT create = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                              D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                              &present, &device);
    record("STAGE CreateDevice hr=0x%08lx device=%p\n",
           static_cast<unsigned long>(create), static_cast<void*>(device));
    if (FAILED(create) || !device)
    {
        d3d->Release();
        return fail(21, "CreateDevice", create);
    }

    const HRESULT cooperative = device->TestCooperativeLevel();
    const HRESULT clear = device->Clear(0, nullptr, D3DCLEAR_TARGET,
                                        D3DCOLOR_XRGB(32, 96, 160), 1.0f, 0);
    const HRESULT begin = device->BeginScene();
    const HRESULT end = SUCCEEDED(begin) ? device->EndScene() : begin;
    const HRESULT present_result = device->Present(nullptr, nullptr, nullptr, nullptr);

    record("STAGE Render cooperative=0x%08lx clear=0x%08lx begin=0x%08lx "
           "end=0x%08lx present=0x%08lx\n",
           static_cast<unsigned long>(cooperative), static_cast<unsigned long>(clear),
           static_cast<unsigned long>(begin), static_cast<unsigned long>(end),
           static_cast<unsigned long>(present_result));

    device->Release();
    d3d->Release();
    DestroyWindow(window);

    if (FAILED(cooperative) || FAILED(clear) || FAILED(begin)
        || FAILED(end) || FAILED(present_result))
        return fail(22, "RenderOrPresent");

    record("SOA_D3D9_PROBE_PASS\n");
    if (vectored)
        RemoveVectoredExceptionHandler(vectored);
    if (g_result)
        std::fclose(g_result);
    return 0;
}
