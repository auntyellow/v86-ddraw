#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <io.h>
#include <ddraw.h>

#ifdef NDEBUG

#define printf(x)

#else

#define printf(x) DebugPrint x

static FILE *g_fpDebug = NULL;
static int g_fdDebug = -1;

void DebugPrint(const char *format, ...)
{
    va_list args;
    char buffer[1024];
    int i;
    va_start(args, format);
    if (g_fpDebug) {
        vfprintf(g_fpDebug, format, args);
        fflush(g_fpDebug);
        if (g_fdDebug != -1) {
            _commit(g_fdDebug);
        }
    } else {
        i = _vsnprintf(buffer, sizeof(buffer), format, args);
        buffer[i < 0 ? sizeof(buffer) - 1 : i] = '\0';
        OutputDebugStringA(buffer);
    }
    va_end(args);
}

#endif

#if defined(RA95_FLIP) && defined(RA95_BLTFAST)
#error "remove RA95_FLIP or RA95_BLTFAST"
#endif

static HMODULE g_hRealDDraw = NULL;

typedef HRESULT (WINAPI *DirectDrawCreate_t)(LPGUID, LPDIRECTDRAW *, IUnknown *);
typedef HRESULT (WINAPI *DirectDrawEnumerateA_t)(LPDDENUMCALLBACKA, LPVOID);

static DirectDrawCreate_t pReal_DirectDrawCreate = NULL;
static DirectDrawEnumerateA_t pReal_DirectDrawEnumerateA = NULL;

static void LoadRealDDraw()
{
    char sysPath[MAX_PATH];
    if (g_hRealDDraw) {
        printf(("LoadRealDDraw: ddraw.dll already loaded\n"));
        return;
    }

    if (!GetSystemDirectoryA(sysPath, MAX_PATH)) {
        printf(("LoadRealDDraw: GetSystemDirectoryA failed\n"));
        return;
    }
    strcat(sysPath, "\\ddraw.dll");

    g_hRealDDraw = LoadLibraryA(sysPath);
    if (!g_hRealDDraw) {
        printf(("LoadRealDDraw: LoadLibraryA failed, error = %lu, path = %s\n", GetLastError(), sysPath));
        return;
    }

    pReal_DirectDrawCreate = (DirectDrawCreate_t) GetProcAddress(g_hRealDDraw, "DirectDrawCreate");
    printf(("LoadRealDDraw: GetProcAddress(DirectDrawCreate) = %p\n", pReal_DirectDrawCreate));
    pReal_DirectDrawEnumerateA = (DirectDrawEnumerateA_t) GetProcAddress(g_hRealDDraw, "DirectDrawEnumerateA");
    printf(("LoadRealDDraw: GetProcAddress(DirectDrawEnumerateA) = %p\n", pReal_DirectDrawEnumerateA));
}

typedef struct WrappedDirectDraw {
    IDirectDrawVtbl *lpVtbl;
    LPDIRECTDRAW real;
    LONG ref;
} WrappedDirectDraw;

typedef struct WrappedSurface {
    IDirectDrawSurfaceVtbl *lpVtbl;
    LPDIRECTDRAWSURFACE real, attached;
    LONG ref;
} WrappedSurface;

WrappedSurface *WRAP_Surface(LPDIRECTDRAWSURFACE real, LPDIRECTDRAWSURFACE attached);

typedef struct WrappedPalette {
    IDirectDrawPaletteVtbl *lpVtbl;
    LPDIRECTDRAWPALETTE real;
    LONG ref;
} WrappedPalette;

WrappedPalette *WRAP_Palette(LPDIRECTDRAWPALETTE real);

typedef struct WrappedClipper {
    IDirectDrawClipperVtbl *lpVtbl;
    LPDIRECTDRAWCLIPPER real;
    LONG ref;
} WrappedClipper;

WrappedClipper *WRAP_Clipper(LPDIRECTDRAWCLIPPER real);

ULONG STDMETHODCALLTYPE DD_AddRef(WrappedDirectDraw *This)
{
    LONG newRef;
    printf(("DD_AddRef: this=0x%p\n", This));
    newRef = InterlockedIncrement(&This->ref);
    This->real->lpVtbl->AddRef(This->real);
    return newRef;
}

HRESULT STDMETHODCALLTYPE DD_QueryInterface(WrappedDirectDraw *This, REFIID riid, LPVOID *ppvObj)
{
    printf(("DD_QueryInterface: this=0x%p riid=0x%p out=0x%p\n", This, &riid, ppvObj));

    if (!ppvObj) {
        return E_POINTER;
    }
    *ppvObj = NULL;

    if (IsEqualIID(riid, &IID_IDirectDraw) || IsEqualIID(riid, &IID_IUnknown)) {
        DD_AddRef(This);
        *ppvObj = This;
        return S_OK;
    }

    return This->real->lpVtbl->QueryInterface(This->real, riid, ppvObj);
}

ULONG STDMETHODCALLTYPE DD_Release(WrappedDirectDraw *This)
{
    LONG newRef;
    printf(("DD_Release: this=0x%p\n", This));
    This->real->lpVtbl->Release(This->real);

    newRef = InterlockedDecrement(&This->ref);
    if (newRef == 0) {
        HeapFree(GetProcessHeap(), 0, This->lpVtbl);
        HeapFree(GetProcessHeap(), 0, This);
        return 0;
    }
    return newRef;
}

#ifndef NDEBUG

void dump_ddsd(DWORD dwFlags)
{
    printf(("begin dump_ddsd, 0x%08lX\n", dwFlags));
    if (dwFlags & DDSD_CAPS) {
        printf(("    DDSD_CAPS\n"));
    }
    if (dwFlags & DDSD_HEIGHT) {
        printf(("    DDSD_HEIGHT\n"));
    }
    if (dwFlags & DDSD_WIDTH) {
        printf(("    DDSD_WIDTH\n"));
    }
    if (dwFlags & DDSD_PITCH) {
        printf(("    DDSD_PITCH\n"));
    }
    if (dwFlags & DDSD_BACKBUFFERCOUNT) {
        printf(("    DDSD_BACKBUFFERCOUNT\n"));
    }
    if (dwFlags & DDSD_ZBUFFERBITDEPTH) {
        printf(("    DDSD_ZBUFFERBITDEPTH\n"));
    }
    if (dwFlags & DDSD_ALPHABITDEPTH) {
        printf(("    DDSD_ALPHABITDEPTH\n"));
    }
    if (dwFlags & DDSD_LPSURFACE) {
        printf(("    DDSD_LPSURFACE\n"));
    }
    if (dwFlags & DDSD_PIXELFORMAT) {
        printf(("    DDSD_PIXELFORMAT\n"));
    }
    if (dwFlags & DDSD_CKDESTOVERLAY) {
        printf(("    DDSD_CKDESTOVERLAY\n"));
    }
    if (dwFlags & DDSD_CKDESTBLT) {
        printf(("    DDSD_CKDESTBLT\n"));
    }
    if (dwFlags & DDSD_CKSRCOVERLAY) {
        printf(("    DDSD_CKSRCOVERLAY\n"));
    }
    if (dwFlags & DDSD_CKSRCBLT) {
        printf(("    DDSD_CKSRCBLT\n"));
    }
    if (dwFlags & DDSD_MIPMAPCOUNT) {
        printf(("    DDSD_MIPMAPCOUNT\n"));
    }
    if (dwFlags & DDSD_REFRESHRATE) {
        printf(("    DDSD_REFRESHRATE\n"));
    }
    if (dwFlags & DDSD_LINEARSIZE) {
        printf(("    DDSD_LINEARSIZE\n"));
    }
    if ((dwFlags & DDSD_ALL) == DDSD_ALL) {
        printf(("    DDSD_ALL\n"));
    }
    printf(("end dump_ddsd\n"));
}

void dump_ddscaps(DWORD dwCaps)
{
    printf(("begin dump_ddscaps, 0x%08lX\n", dwCaps));
    if (dwCaps & DDSCAPS_ALPHA) {
        printf(("    DDSCAPS_ALPHA\n"));
    }
    if (dwCaps & DDSCAPS_BACKBUFFER) {
        printf(("    DDSCAPS_BACKBUFFER\n"));
    }
    if (dwCaps & DDSCAPS_FLIP) {
        printf(("    DDSCAPS_FLIP\n"));
    }
    if (dwCaps & DDSCAPS_FRONTBUFFER) {
        printf(("    DDSCAPS_FRONTBUFFER\n"));
    }
    if (dwCaps & DDSCAPS_PALETTE) {
        printf(("    DDSCAPS_PALETTE\n"));
    }
    if (dwCaps & DDSCAPS_TEXTURE) {
        printf(("    DDSCAPS_TEXTURE\n"));
    }
    if (dwCaps & DDSCAPS_PRIMARYSURFACE) {
        printf(("    DDSCAPS_PRIMARYSURFACE\n"));
    }
    if (dwCaps & DDSCAPS_OFFSCREENPLAIN) {
        printf(("    DDSCAPS_OFFSCREENPLAIN\n"));
    }
    if (dwCaps & DDSCAPS_VIDEOMEMORY) {
        printf(("    DDSCAPS_VIDEOMEMORY\n"));
    }
    if (dwCaps & DDSCAPS_LOCALVIDMEM) {
        printf(("    DDSCAPS_LOCALVIDMEM\n"));
    }
    if (dwCaps & DDSCAPS_SYSTEMMEMORY) {
        printf(("    DDSCAPS_SYSTEMMEMORY\n"));
	}
    printf(("end dump_ddscaps\n"));
}

#endif

HRESULT STDMETHODCALLTYPE DD_CreateSurface(WrappedDirectDraw *This, LPDDSURFACEDESC lpddsd, WrappedSurface **ppSurf, IUnknown *iu)
{
    LPDIRECTDRAWSURFACE real, attached;
    HRESULT hr;
    BOOL bAttach;
    DDSURFACEDESC ddsd;
    DDSCAPS ddsCaps;

    printf(("DD_CreateSurface: this=0x%p desc=0x%p out_wrapped=0x%p\n", This, lpddsd, ppSurf));
    ddsd = *lpddsd;
#ifndef NDEBUG
    dump_ddsd(ddsd.dwFlags);
    dump_ddscaps(ddsd.ddsCaps.dwCaps);
#endif

    if (!ppSurf) {
        return DDERR_INVALIDPARAMS;
    }
    real = NULL;
    bAttach = FALSE;
#if defined(RA95_FLIP) || defined(RA95_BLTFAST)
    if ((ddsd.dwFlags & DDSD_CAPS) && (ddsd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && !(ddsd.ddsCaps.dwCaps & (DDSCAPS_FLIP | DDSCAPS_COMPLEX))) {
#ifdef RA95_FLIP
        ddsd.dwFlags |= DDSD_BACKBUFFERCOUNT;
        ddsd.dwBackBufferCount = 1;
        ddsd.ddsCaps.dwCaps |= DDSCAPS_FLIP | DDSCAPS_COMPLEX;
#endif
        bAttach = TRUE;
    }
#endif
    hr = This->real->lpVtbl->CreateSurface(This->real, &ddsd, &real, iu);
    if (FAILED(hr)) {
        printf(("DD_CreateSurface returned 0x%08lX\n", hr));
        return hr;
    }

    attached = NULL;
    if (bAttach) {
        UNREFERENCED_PARAMETER(ddsCaps);
#ifdef RA95_FLIP
        ZeroMemory(&ddsCaps, sizeof(ddsCaps));
        ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
        hr = real->lpVtbl->GetAttachedSurface(real, &ddsCaps, &attached);
        printf(("DD_CreateSurface: GetAttachedSurface returned 0x%08lX (use Flip)\n", hr));
#endif
#ifdef RA95_BLTFAST
        ZeroMemory(&ddsd, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        hr = real->lpVtbl->GetSurfaceDesc(real, &ddsd);
        if (FAILED(hr)) {
            printf(("DD_CreateSurface: GetSurfaceDesc returned 0x%08lX (ignored)\n", hr));
        } else if ((ddsd.dwFlags & (DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT)) != (DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT)) {
            printf(("DD_CreateSurface: flags returned by GetSurfaceDesc doesn't contain DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT (ignored)\n"));
#ifndef NDEBUG
            dump_ddsd(ddsd.dwFlags);
#endif
        } else {
            ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
            ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
            ddsd.dwBackBufferCount = 0;
            hr = This->real->lpVtbl->CreateSurface(This->real, &ddsd, &attached, NULL);
            printf(("DD_CreateSurface: CreateSurface (attached) returned 0x%08lX (use BltFast)\n", hr));
        }
#endif
    }

    *ppSurf = WRAP_Surface(real, attached);
    printf(("DD_CreateSurface: returned wrapped=0x%p, lpSurface=0x%p\n", *ppSurf, lpddsd->lpSurface));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DD_CreatePalette(WrappedDirectDraw *This, DWORD dwFlags, LPPALETTEENTRY lpPaletteEntry, WrappedPalette **ppPal, IUnknown *iu)
{
    LPDIRECTDRAWPALETTE real;
    HRESULT hr;
    printf(("DD_CreatePalette: this=0x%p flags=0x%08lX out_wrapped=0x%p\n", This, dwFlags, ppPal));

    if (!ppPal) {
        return DDERR_INVALIDPARAMS;
    }
    real = NULL;
    hr = This->real->lpVtbl->CreatePalette(This->real, dwFlags, lpPaletteEntry, &real, iu);
    if (FAILED(hr)) {
        printf(("DD_CreatePalette returned 0x%08lX\n", hr));
        return hr;
    }

    *ppPal = WRAP_Palette(real);
    printf(("DD_CreatePalette: returned wrapped=0x%p\n", *ppPal));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DD_Compact(WrappedDirectDraw *This)
{
    HRESULT hr = This->real->lpVtbl->Compact(This->real);
    printf(("DD_Compact: this=0x%p result=0x%08lX\n", This, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_CreateClipper(WrappedDirectDraw *This, DWORD dwFlags, WrappedClipper **ppClip, IUnknown *iu)
{
    LPDIRECTDRAWCLIPPER real;
    HRESULT hr;
    printf(("DD_CreateClipper: this=0x%p flags=0x%08lX out=0x%p\n", This, dwFlags, ppClip));
    if (!ppClip) {
        return DDERR_INVALIDPARAMS;
    }
    real = NULL;
    hr = This->real->lpVtbl->CreateClipper(This->real, dwFlags, &real, iu);
    if (FAILED(hr)) {
        printf(("DD_CreateClipper returned 0x%08lX\n", hr));
        return hr;
    }
    *ppClip = WRAP_Clipper(real);
    printf(("DD_CreateClipper: returned wrapped=0x%p\n", *ppClip));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DD_DuplicateSurface(WrappedDirectDraw *This, WrappedSurface *src, WrappedSurface **ppSurf)
{
    LPDIRECTDRAWSURFACE real;
    HRESULT hr;
    printf(("DD_DuplicateSurface: this=0x%p src_wrapped=0x%p out_wrapped=0x%p\n", This, src, ppSurf));
    if (!src || !ppSurf) {
        return DDERR_INVALIDPARAMS;
    }
    real = NULL;
    hr = This->real->lpVtbl->DuplicateSurface(This->real, src->real, &real);
    if (FAILED(hr)) {
        printf(("DD_DuplicateSurface returned 0x%08lX\n", hr));
        return hr;
    }
    *ppSurf = WRAP_Surface(real, NULL);
    printf(("DD_DuplicateSurface: returned wrapped=0x%p\n", *ppSurf));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DD_EnumDisplayModes(WrappedDirectDraw *This, DWORD dwFlags, LPDDSURFACEDESC lpddsd, LPVOID lpContext, LPDDENUMMODESCALLBACK lpCallback)
{
    HRESULT hr = This->real->lpVtbl->EnumDisplayModes(This->real, dwFlags, lpddsd, lpContext, lpCallback);
    printf(("DD_EnumDisplayModes: this=0x%p flags=0x%08lX desc=0x%p context=0x%p callback=0x%p result=0x%08lX\n", This, dwFlags, lpddsd, lpContext, lpCallback, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_EnumSurfaces(WrappedDirectDraw *This, DWORD dwFlags, LPDDSURFACEDESC lpddsd, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpCallback)
{
    HRESULT hr = This->real->lpVtbl->EnumSurfaces(This->real, dwFlags, lpddsd, lpContext, lpCallback);
    printf(("DD_EnumSurfaces: this=0x%p flags=0x%08lX desc=0x%p context=0x%p callback=0x%p result=0x%08lX\n", This, dwFlags, lpddsd, lpContext, lpCallback, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_FlipToGDISurface(WrappedDirectDraw *This)
{
    HRESULT hr = This->real->lpVtbl->FlipToGDISurface(This->real);
    printf(("DD_FlipToGDISurface: this=0x%p result=0x%08lX\n", This, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_GetCaps(WrappedDirectDraw *This, LPDDCAPS lpCaps1, LPDDCAPS lpCaps2)
{
    HRESULT hr = This->real->lpVtbl->GetCaps(This->real, lpCaps1, lpCaps2);
    printf(("DD_GetCaps: this=0x%p caps1=0x%p caps2=0x%p result=0x%08lX\n", This, lpCaps1, lpCaps2, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_GetDisplayMode(WrappedDirectDraw *This, LPDDSURFACEDESC lpddsd)
{
    HRESULT hr = This->real->lpVtbl->GetDisplayMode(This->real, lpddsd);
    printf(("DD_GetDisplayMode: this=0x%p desc=0x%p result=0x%08lX\n", This, lpddsd, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_GetFourCCCodes(WrappedDirectDraw *This, LPDWORD lpCodes, LPDWORD lpCount)
{
    HRESULT hr = This->real->lpVtbl->GetFourCCCodes(This->real, lpCodes, lpCount);
    printf(("DD_GetFourCCCodes: this=0x%p codes=0x%p count=0x%p result=0x%08lX\n", This, lpCodes, lpCount, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_GetGDISurface(WrappedDirectDraw *This, WrappedSurface **ppSurf)
{
    LPDIRECTDRAWSURFACE real;
    HRESULT hr;
    printf(("DD_GetGDISurface: this=0x%p out_wrapped=0x%p\n", This, ppSurf));
    if (!ppSurf) {
        return DDERR_INVALIDPARAMS;
    }
    real = NULL;
    hr = This->real->lpVtbl->GetGDISurface(This->real, &real);
    if (FAILED(hr)) {
        printf(("DD_GetGDISurface returned 0x%08lX\n", hr));
        return hr;
    }
    *ppSurf = WRAP_Surface(real, NULL);
    printf(("DD_GetGDISurface: returned wrapped=0x%p\n", *ppSurf));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DD_GetMonitorFrequency(WrappedDirectDraw *This, LPDWORD lpOut)
{
    HRESULT hr = This->real->lpVtbl->GetMonitorFrequency(This->real, lpOut);
    printf(("DD_GetMonitorFrequency: this=0x%p out=0x%p result=0x%08lX\n", This, lpOut, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_GetScanLine(WrappedDirectDraw *This, LPDWORD lpOut)
{
    HRESULT hr = This->real->lpVtbl->GetScanLine(This->real, lpOut);
    printf(("DD_GetScanLine: this=0x%p out=0x%p result=0x%08lX\n", This, lpOut, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_GetVerticalBlankStatus(WrappedDirectDraw *This, LPBOOL lpOut)
{
    HRESULT hr = This->real->lpVtbl->GetVerticalBlankStatus(This->real, lpOut);
    printf(("DD_GetVerticalBlankStatus: this=0x%p out=0x%p result=0x%08lX\n", This, lpOut, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_Initialize(WrappedDirectDraw *This, GUID *lpGuid)
{
    HRESULT hr = This->real->lpVtbl->Initialize(This->real, lpGuid);
    printf(("DD_Initialize: this=0x%p guid=0x%p result=0x%08lX\n", This, lpGuid, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_RestoreDisplayMode(WrappedDirectDraw *This)
{
    HRESULT hr = This->real->lpVtbl->RestoreDisplayMode(This->real);
    printf(("DD_RestoreDisplayMode: this=0x%p result=0x%08lX\n", This, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_SetCooperativeLevel(WrappedDirectDraw *This, HWND hwnd, DWORD dwFlags)
{
    // HWND newHwnd = (dwFlags & DDSCL_NORMAL) && hwnd == NULL ? GetForegroundWindow() : hwnd;
    HRESULT hr = This->real->lpVtbl->SetCooperativeLevel(This->real, hwnd, dwFlags);
    printf(("DD_SetCooperativeLevel: this=0x%p hwnd=0x%p flags=0x%08lX result=0x%08lX\n", This, hwnd, dwFlags, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_SetDisplayMode(WrappedDirectDraw *This, DWORD w, DWORD h, DWORD bpp)
{
    HRESULT hr = This->real->lpVtbl->SetDisplayMode(This->real, w, h, bpp);
    printf(("DD_SetDisplayMode: this=0x%p w=%lu h=%lu bpp=%lu result=0x%08lX\n", This, w, h, bpp, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE DD_WaitForVerticalBlank(WrappedDirectDraw *This, DWORD dwFlags, HANDLE hEvent)
{
    HRESULT hr = This->real->lpVtbl->WaitForVerticalBlank(This->real, dwFlags, hEvent);
    printf(("DD_WaitForVerticalBlank: this=0x%p flags=0x%08lX event=0x%p result=0x%08lX\n", This, dwFlags, hEvent, hr));
    return hr;
}

WrappedDirectDraw *WRAP_DirectDraw(LPDIRECTDRAW real)
{
    WrappedDirectDraw *w;
    IDirectDrawVtbl *lpVtbl;
    if (!real) {
        return NULL;
    }

    w = (WrappedDirectDraw *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WrappedDirectDraw));
    if (!w) {
        return NULL;
    }

    lpVtbl = (IDirectDrawVtbl *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectDrawVtbl));
    if (!lpVtbl) {
        HeapFree(GetProcessHeap(), 0, w);
        return NULL;
    }

    lpVtbl->QueryInterface = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, REFIID, LPVOID *))DD_QueryInterface;
    lpVtbl->AddRef = (ULONG (STDMETHODCALLTYPE *)(LPDIRECTDRAW))DD_AddRef;
    lpVtbl->Release = (ULONG (STDMETHODCALLTYPE *)(LPDIRECTDRAW))DD_Release;

    lpVtbl->Compact = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW))DD_Compact;
    lpVtbl->CreateClipper = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, DWORD, LPDIRECTDRAWCLIPPER *, IUnknown *))DD_CreateClipper;
    lpVtbl->CreatePalette = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE *, IUnknown *))DD_CreatePalette;
    lpVtbl->CreateSurface = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, LPDDSURFACEDESC, LPDIRECTDRAWSURFACE *, IUnknown *))DD_CreateSurface;
    lpVtbl->DuplicateSurface = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, LPDIRECTDRAWSURFACE, LPDIRECTDRAWSURFACE *))DD_DuplicateSurface;
    lpVtbl->EnumDisplayModes = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMMODESCALLBACK))DD_EnumDisplayModes;
    lpVtbl->EnumSurfaces = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMSURFACESCALLBACK))DD_EnumSurfaces;
    lpVtbl->FlipToGDISurface = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW))DD_FlipToGDISurface;
    lpVtbl->GetCaps = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, LPDDCAPS, LPDDCAPS))DD_GetCaps;
    lpVtbl->GetDisplayMode = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, LPDDSURFACEDESC))DD_GetDisplayMode;
    lpVtbl->GetFourCCCodes = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, LPDWORD, LPDWORD))DD_GetFourCCCodes;
    lpVtbl->GetGDISurface = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, LPDIRECTDRAWSURFACE *))DD_GetGDISurface;
    lpVtbl->GetMonitorFrequency = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, LPDWORD))DD_GetMonitorFrequency;
    lpVtbl->GetScanLine = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, LPDWORD))DD_GetScanLine;
    lpVtbl->GetVerticalBlankStatus = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, LPBOOL))DD_GetVerticalBlankStatus;
    lpVtbl->Initialize = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, GUID *))DD_Initialize;
    lpVtbl->RestoreDisplayMode = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW))DD_RestoreDisplayMode;
    lpVtbl->SetCooperativeLevel = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, HWND, DWORD))DD_SetCooperativeLevel;
    lpVtbl->SetDisplayMode = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, DWORD, DWORD, DWORD))DD_SetDisplayMode;
    lpVtbl->WaitForVerticalBlank = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAW, DWORD, HANDLE))DD_WaitForVerticalBlank;

    w->lpVtbl = lpVtbl;
    w->real = real;
    w->ref = 1;

    return w;
}

ULONG STDMETHODCALLTYPE Surf_AddRef(WrappedSurface *This)
{
    LONG newRef;
    printf(("Surf_AddRef: this=0x%p\n", This));
    newRef = InterlockedIncrement(&This->ref);
    This->real->lpVtbl->AddRef(This->real);
    if (This->attached) {
        This->attached->lpVtbl->AddRef(This->attached);
    }
    return newRef;
}

HRESULT STDMETHODCALLTYPE Surf_QueryInterface(WrappedSurface *This, REFIID riid, LPVOID *ppvObj)
{
    printf(("Surf_QueryInterface: this=0x%p riid=0x%p out=0x%p\n", This, &riid, ppvObj));

    if (!ppvObj) {
        return E_POINTER;
    }
    *ppvObj = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectDrawSurface)) {
        Surf_AddRef(This);
        *ppvObj = This;
        return S_OK;
    }

    return This->real->lpVtbl->QueryInterface(This->real, riid, ppvObj);
}

ULONG STDMETHODCALLTYPE Surf_Release(WrappedSurface *This)
{
    LONG newRef;
    if (This->attached) {
        newRef = This->attached->lpVtbl->Release(This->attached);
        printf(("Surf_Release: attached=0x%p, ref=%ld\n", This->attached, newRef));
    }
    newRef = This->real->lpVtbl->Release(This->real);
    printf(("Surf_Release: real=0x%p, ref=%ld\n", This->real, newRef));
    newRef = InterlockedDecrement(&This->ref);
    printf(("Surf_Release: this=0x%p, ref=%ld\n", This, newRef));
    if (newRef == 0) {
        HeapFree(GetProcessHeap(), 0, This->lpVtbl);
        HeapFree(GetProcessHeap(), 0, This);
        return 0;
    }
    return newRef;
}

HRESULT STDMETHODCALLTYPE Surf_AddAttachedSurface(WrappedSurface *This, WrappedSurface *lpSurf)
{
    HRESULT hr = This->real->lpVtbl->AddAttachedSurface(This->real, lpSurf->real);
    printf(("Surf_AddAttachedSurface: this=0x%p attached_wrapped=0x%p result=0x%08lX\n", This, lpSurf, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_AddOverlayDirtyRect(WrappedSurface *This, LPRECT lpRect)
{
    HRESULT hr = This->real->lpVtbl->AddOverlayDirtyRect(This->real, lpRect);
    if (lpRect) {
        printf(("Surf_AddOverlayDirtyRect: this=0x%p rect=(%ld,%ld)-(%ld,%ld) result=0x%08lX\n", This, lpRect->left, lpRect->top, lpRect->right, lpRect->bottom, hr));
    } else {
        printf(("Surf_AddOverlayDirtyRect: this=0x%p rect=NULL result=0x%08lX\n", This, hr));
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_Blt(WrappedSurface *This, LPRECT dstRect, WrappedSurface *wrappedSrc, LPRECT srcRect, DWORD dwFlags, LPDDBLTFX lpBltFx)
{
    // TODO dump dstRect and srcRect
    HRESULT hr = This->real->lpVtbl->Blt(This->real, dstRect, wrappedSrc->real, srcRect, dwFlags, lpBltFx);
    printf(("Surf_Blt: this=0x%p dst_rect=0x%p src_wrapped=0x%p src_rect=0x%p flags=0x%08lX bltfx=0x%p result=0x%08lX\n", This, dstRect, wrappedSrc, srcRect, dwFlags, lpBltFx, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_BltBatch(WrappedSurface *This, LPDDBLTBATCH lpBatch, DWORD dwCount, DWORD dwFlags)
{
    HRESULT hr = This->real->lpVtbl->BltBatch(This->real, lpBatch, dwCount, dwFlags);
    printf(("Surf_BltBatch: this=0x%p batch=0x%p count=%lu flags=0x%08lX result=0x%08lX\n", This, lpBatch, dwCount, dwFlags, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_BltFast(WrappedSurface *This, DWORD x, DWORD y, WrappedSurface *src, LPRECT srcRect, DWORD dwFlags)
{
    HRESULT hr = This->real->lpVtbl->BltFast(This->real, x, y, src->real, srcRect, dwFlags);
    printf(("Surf_BltFast: this=0x%p x=%lu y=%lu src_wrapped=0x%p src_rect=0x%p flags=0x%08lX result=0x%08lX\n", This, x, y, src, srcRect, dwFlags, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_DeleteAttachedSurface(WrappedSurface *This, DWORD dwFlags, WrappedSurface *attached)
{
    HRESULT hr = This->real->lpVtbl->DeleteAttachedSurface(This->real, dwFlags, attached->real);
    printf(("Surf_DeleteAttachedSurface: this=0x%p flags=0x%08lX attached_wrapped=0x%p result=0x%08lX\n", This, dwFlags, attached, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_EnumAttachedSurfaces(WrappedSurface *This, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpCallback)
{
    // What you get in the callback is the real surface. If a wrapped version is needed, you need to create another thunk layer for it.
    HRESULT hr = This->real->lpVtbl->EnumAttachedSurfaces(This->real, lpContext, lpCallback);
    printf(("Surf_EnumAttachedSurfaces: this=0x%p ctx=0x%p callback=0x%p result=0x%08lX\n", This, lpContext, lpCallback, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_EnumOverlayZOrders(WrappedSurface *This, DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpCallback)
{
    HRESULT hr = This->real->lpVtbl->EnumOverlayZOrders(This->real, dwFlags, lpContext, lpCallback);
    printf(("Surf_EnumOverlayZOrders: this=0x%p flags=0x%08lX context=0x%p callback=0x%p result=0x%08lX\n", This, dwFlags, lpContext, lpCallback, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_Flip(WrappedSurface *This, WrappedSurface *target, DWORD dwFlags)
{
    HRESULT hr = This->real->lpVtbl->Flip(This->real, target == NULL ? NULL : target->real, dwFlags);
    printf(("Surf_Flip: surface=0x%p target_wrapped=0x%p flags=0x%08lX result=0x%08lX\n", This, target, dwFlags, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_GetAttachedSurface(WrappedSurface *This, LPDDSCAPS caps, WrappedSurface **ppSurf)
{
    LPDIRECTDRAWSURFACE real;
    HRESULT hr;
    printf(("Surf_GetAttachedSurface: this=0x%p caps=0x%p out_wrapped=0x%p\n", This, caps, ppSurf));
    if (!ppSurf) {
        return DDERR_INVALIDPARAMS;
    }
    real = NULL;
    hr = This->real->lpVtbl->GetAttachedSurface(This->real, caps, &real);
    if (FAILED(hr)) {
        printf(("Surf_GetAttachedSurface returned 0x%08lX\n", hr));
        return hr;
    }
    *ppSurf = WRAP_Surface(real, NULL);
    printf(("Surf_GetAttachedSurface: returned wrapped=0x%p\n", *ppSurf));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE Surf_GetBltStatus(WrappedSurface *This, DWORD dwFlags)
{
    HRESULT hr = This->real->lpVtbl->GetBltStatus(This->real, dwFlags);
    printf(("Surf_GetBltStatus: this=0x%p flags=0x%08lX result=0x%08lX\n", This, dwFlags, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_GetCaps(WrappedSurface *This, LPDDSCAPS caps)
{
    HRESULT hr;
    printf(("Surf_GetCaps: this=0x%p caps=0x%p\n", This, caps));
    hr = This->real->lpVtbl->GetCaps(This->real, caps);
#ifndef NDEBUG
    dump_ddscaps(caps->dwCaps);
#endif
    // clear DDSCAPS_SYSTEMMEMORY can cheat ra95
    // see https://github.com/electronicarts/CnC_Red_Alert/blob/main/CODE/STARTUP.CPP#L485
#ifdef RA95_SKIP_CHECK
    if (caps->dwCaps & DDSCAPS_PRIMARYSURFACE) {
#else
    if (This->attached) {
#endif
        caps->dwCaps &= ~DDSCAPS_SYSTEMMEMORY;
    }
    printf(("Surf_GetCaps returned 0x%08lX\n", hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_GetClipper(WrappedSurface *This, WrappedClipper **ppClip)
{
    LPDIRECTDRAWCLIPPER real;
    HRESULT hr;
    printf(("Surf_GetClipper: this=0x%p out=0x%p\n", This, ppClip));
    if (!ppClip) {
        return DDERR_INVALIDPARAMS;
    }
    real = NULL;
    hr = This->real->lpVtbl->GetClipper(This->real, &real);
    if (FAILED(hr)) {
        printf(("Surf_GetClipper returned 0x%08lX\n", hr));
        return hr;
    }
    *ppClip = WRAP_Clipper(real);
    printf(("Surf_GetClipper: returned wrapped=0x%p\n", *ppClip));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE Surf_GetColorKey(WrappedSurface *This, DWORD dwFlags, LPDDCOLORKEY lpKey)
{
    HRESULT hr = This->real->lpVtbl->GetColorKey(This->real, dwFlags, lpKey);
    printf(("Surf_GetColorKey: this=0x%p flags=0x%08lX key=0x%p result=0x%08lX\n", This, dwFlags, lpKey, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_GetDC(WrappedSurface *This, HDC *pHdc)
{
    HRESULT hr;
    printf(("Surf_GetDC: this=0x%p out_hdc=0x%p\n", This, pHdc));

    hr = This->real->lpVtbl->GetDC(This->real, pHdc);
    if (SUCCEEDED(hr)) {
        printf(("Surf_GetDC: returned HDC=0x%p\n", *pHdc));
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_GetFlipStatus(WrappedSurface *This, DWORD dwFlags)
{
    HRESULT hr = This->real->lpVtbl->GetFlipStatus(This->real, dwFlags);
    printf(("Surf_GetFlipStatus: this=0x%p flags=0x%08lX result=0x%08lX\n", This, dwFlags, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_GetOverlayPosition(WrappedSurface *This, LPLONG px, LPLONG py)
{
    HRESULT hr = This->real->lpVtbl->GetOverlayPosition(This->real, px, py);
    printf(("Surf_GetOverlayPosition: this=0x%p x=0x%p y=0x%p result=0x%08lX\n", This, px, py, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_GetPalette(WrappedSurface *This, WrappedPalette **ppPal)
{
    LPDIRECTDRAWPALETTE real;
    HRESULT hr;
    printf(("Surf_GetPalette: this=0x%p out_wrapped=0x%p\n", This, ppPal));
    if (!ppPal) {
        return DDERR_INVALIDPARAMS;
    }
    real = NULL;
    hr = This->real->lpVtbl->GetPalette(This->real, &real);
    if (FAILED(hr)) {
        printf(("Surf_GetPalette returned 0x%08lX\n", hr));
        return hr;
    }
    *ppPal = WRAP_Palette(real);
    printf(("Surf_GetPalette: returned wrapped=0x%p\n", *ppPal));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE Surf_GetPixelFormat(WrappedSurface *This, LPDDPIXELFORMAT pf)
{
    HRESULT hr = This->real->lpVtbl->GetPixelFormat(This->real, pf);
    printf(("Surf_GetPixelFormat: this=0x%p pf=0x%p result=0x%08lX\n", This, pf, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_GetSurfaceDesc(WrappedSurface *This, LPDDSURFACEDESC lpddsd)
{
    HRESULT hr = This->real->lpVtbl->GetSurfaceDesc(This->real, lpddsd);
    printf(("Surf_GetSurfaceDesc: this=0x%p desc=0x%p result=0x%08lX\n", This, lpddsd, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_Initialize(WrappedSurface *This, WrappedDirectDraw *lpDD, LPDDSURFACEDESC lpddsd)
{
    HRESULT hr = This->real->lpVtbl->Initialize(This->real, lpDD->real, lpddsd);
    printf(("Surf_Initialize: this=0x%p dd_wrapped=0x%p desc=0x%p result=0x%08lX\n", This, lpDD, lpddsd, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_IsLost(WrappedSurface *This)
{
    HRESULT hr = This->real->lpVtbl->IsLost(This->real);
    printf(("Surf_IsLost: this=0x%p result=0x%08lX\n", This, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_Lock(WrappedSurface *This, LPRECT lpRect, LPDDSURFACEDESC lpddsd, DWORD dwFlags, HANDLE hEvent)
{
    HRESULT hr;
    if (lpRect) {
        printf(("Surf_Lock: this=0x%p rect=(%ld,%ld)-(%ld,%ld) desc=0x%p flags=0x%08lX event=0x%p\n", This, lpRect->left, lpRect->top, lpRect->right, lpRect->bottom, lpddsd, dwFlags, hEvent));
    } else {
        printf(("Surf_Lock: this=0x%p rect=NULL desc=0x%p flags=0x%08lX event=0x%p\n", This, lpddsd, dwFlags, hEvent));
    }
    if (This->attached) {
        hr = This->attached->lpVtbl->Lock(This->attached, lpRect, lpddsd, dwFlags, hEvent);
        printf(("Surf_Lock: attached=0x%p lpSurface=0x%p result=0x%08lX\n", This->attached, lpddsd->lpSurface, hr));
    } else {
        hr = This->real->lpVtbl->Lock(This->real, lpRect, lpddsd, dwFlags, hEvent);
        printf(("Surf_Lock: real=0x%p lpSurface=0x%p result=0x%08lX\n", This->real, lpddsd->lpSurface, hr));
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_ReleaseDC(WrappedSurface *This, HDC hdc)
{
    HRESULT hr = This->real->lpVtbl->ReleaseDC(This->real, hdc);
    printf(("Surf_ReleaseDC: this=0x%p HDC=0x%p result=0x%08lX\n", This, hdc, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_Restore(WrappedSurface *This)
{
    HRESULT hr = This->real->lpVtbl->Restore(This->real);
    printf(("Surf_Restore: this=0x%p result=0x%08lX\n", This, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_SetClipper(WrappedSurface *This, WrappedClipper *lpClipper)
{
    HRESULT hr = This->real->lpVtbl->SetClipper(This->real, lpClipper->real);
    printf(("Surf_SetClipper: this=0x%p clipper=0x%p result=0x%08lX\n", This, lpClipper, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_SetColorKey(WrappedSurface *This, DWORD dwFlags, LPDDCOLORKEY key)
{
    HRESULT hr = This->real->lpVtbl->SetColorKey(This->real, dwFlags, key);
    printf(("Surf_SetColorKey: this=0x%p flags=0x%08lX key=0x%p result=0x%08lX\n", This, dwFlags, key, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_SetOverlayPosition(WrappedSurface *This, LONG x, LONG y)
{
    HRESULT hr = This->real->lpVtbl->SetOverlayPosition(This->real, x, y);
    printf(("Surf_SetOverlayPosition: this=0x%p x=%ld y=%ld result=0x%08lX\n", This, x, y, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_SetPalette(WrappedSurface *This, WrappedPalette *pPal)
{
    LPDIRECTDRAWPALETTE realPal = pPal == NULL ? NULL : pPal->real;
    HRESULT hr = This->real->lpVtbl->SetPalette(This->real, realPal);
    if (This->attached) {
        This->attached->lpVtbl->SetPalette(This->attached, realPal);
    }
    printf(("Surf_SetPalette: this=0x%p pal_wrapped=0x%p result=0x%08lX\n", This, pPal, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_Unlock(WrappedSurface *This, LPRECT lpRect)
{
    HRESULT hr, hr2;
    if (lpRect) {
        printf(("Surf_Unlock: this=0x%p rect=(%ld,%ld)-(%ld,%ld)\n", This, lpRect->left, lpRect->top, lpRect->right, lpRect->bottom));
    } else {
        printf(("Surf_Unlock: this=0x%p rect=NULL\n", This));
    }
    if (This->attached) {
        hr = This->attached->lpVtbl->Unlock(This->attached, lpRect);
        UNREFERENCED_PARAMETER(hr2);
#ifdef RA95_FLIP
        hr2 = This->real->lpVtbl->Flip(This->real, NULL, DDFLIP_WAIT);
        printf(("Surf_Unlock: Flip returned 0x%08lX\n", hr2));
#endif
#ifdef RA95_BLTFAST
        hr2 = This->real->lpVtbl->BltFast(This->real, 0, 0, This->attached, NULL, DDBLTFAST_WAIT | DDBLTFAST_NOCOLORKEY);
        printf(("Surf_Unlock: BltFast returned 0x%08lX\n", hr2));
#endif
        printf(("Surf_Unlock: attached=0x%p result=0x%08lX\n", This->attached, hr));
    } else {
        hr = This->real->lpVtbl->Unlock(This->real, lpRect);
        printf(("Surf_Unlock: real=0x%p result=0x%08lX\n", This->real, hr));
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_UpdateOverlay(WrappedSurface *This, LPRECT srcRect, WrappedSurface *dest, LPRECT destRect, DWORD dwFlags, LPDDOVERLAYFX fx)
{
    // TODO dump dstRect and srcRect
    HRESULT hr = This->real->lpVtbl->UpdateOverlay(This->real, srcRect, dest->real, destRect, dwFlags, fx);
    printf(("Surf_UpdateOverlay: this=0x%p srcRect=0x%p dest_wrapped=0x%p destRect=0x%p flags=0x%08lX fx=0x%p result=0x%08lX\n", This, srcRect, dest, destRect, dwFlags, fx, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_UpdateOverlayDisplay(WrappedSurface *This, DWORD dwFlags)
{
    HRESULT hr = This->real->lpVtbl->UpdateOverlayDisplay(This->real, dwFlags);
    printf(("Surf_UpdateOverlayDisplay: this=0x%p flags=0x%08lX result=0x%08lX\n", This, dwFlags, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Surf_UpdateOverlayZOrder(WrappedSurface *This, DWORD dwFlags, WrappedSurface *ref)
{
    HRESULT hr = This->real->lpVtbl->UpdateOverlayZOrder(This->real, dwFlags, ref->real);
    printf(("Surf_UpdateOverlayZOrder: this=0x%p flags=0x%08lX ref_wrapped=0x%p result=0x%08lX\n", This, dwFlags, ref, hr));
    return hr;
}

WrappedSurface *WRAP_Surface(LPDIRECTDRAWSURFACE real, LPDIRECTDRAWSURFACE attached)
{
    WrappedSurface *w;
    IDirectDrawSurfaceVtbl *lpVtbl;
    if (!real) {
        return NULL;
    }

    w = (WrappedSurface *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WrappedSurface));
    if (!w) {
        return NULL;
    }

    lpVtbl = (IDirectDrawSurfaceVtbl *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectDrawSurfaceVtbl));
    if (!lpVtbl) {
        HeapFree(GetProcessHeap(), 0, w);
        return NULL;
    }

    w->lpVtbl = lpVtbl;
    w->real = real;
    w->attached = attached;
    w->ref = 1;

    lpVtbl->QueryInterface = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, REFIID, LPVOID *)) Surf_QueryInterface;
    lpVtbl->AddRef = (ULONG (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE)) Surf_AddRef;
    lpVtbl->Release = (ULONG (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE)) Surf_Release;

    lpVtbl->AddAttachedSurface = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDIRECTDRAWSURFACE)) Surf_AddAttachedSurface;
    lpVtbl->AddOverlayDirtyRect = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPRECT)) Surf_AddOverlayDirtyRect;
    lpVtbl->Blt = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPRECT, LPDIRECTDRAWSURFACE, LPRECT, DWORD, LPDDBLTFX)) Surf_Blt;
    lpVtbl->BltBatch = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDDBLTBATCH, DWORD, DWORD)) Surf_BltBatch;
    lpVtbl->BltFast = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, DWORD, DWORD, LPDIRECTDRAWSURFACE, LPRECT, DWORD)) Surf_BltFast;
    lpVtbl->DeleteAttachedSurface = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, DWORD, LPDIRECTDRAWSURFACE)) Surf_DeleteAttachedSurface;
    lpVtbl->EnumAttachedSurfaces = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPVOID, LPDDENUMSURFACESCALLBACK)) Surf_EnumAttachedSurfaces;
    lpVtbl->EnumOverlayZOrders = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, DWORD, LPVOID, LPDDENUMSURFACESCALLBACK)) Surf_EnumOverlayZOrders;
    lpVtbl->Flip = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDIRECTDRAWSURFACE, DWORD)) Surf_Flip;
    lpVtbl->GetAttachedSurface = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDDSCAPS, LPDIRECTDRAWSURFACE *)) Surf_GetAttachedSurface;
    lpVtbl->GetBltStatus = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, DWORD)) Surf_GetBltStatus;
    lpVtbl->GetCaps = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDDSCAPS)) Surf_GetCaps;
    lpVtbl->GetClipper = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDIRECTDRAWCLIPPER *)) Surf_GetClipper;
    lpVtbl->GetColorKey = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, DWORD, LPDDCOLORKEY)) Surf_GetColorKey;
    lpVtbl->GetDC = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, HDC *)) Surf_GetDC;
    lpVtbl->GetFlipStatus = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, DWORD)) Surf_GetFlipStatus;
    lpVtbl->GetOverlayPosition = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPLONG, LPLONG)) Surf_GetOverlayPosition;
    lpVtbl->GetPalette = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDIRECTDRAWPALETTE *)) Surf_GetPalette;
    lpVtbl->GetPixelFormat = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDDPIXELFORMAT)) Surf_GetPixelFormat;
    lpVtbl->GetSurfaceDesc = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDDSURFACEDESC)) Surf_GetSurfaceDesc;
    lpVtbl->Initialize = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDIRECTDRAW, LPDDSURFACEDESC)) Surf_Initialize;
    lpVtbl->IsLost = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE)) Surf_IsLost;
    lpVtbl->Lock = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPRECT, LPDDSURFACEDESC, DWORD, HANDLE)) Surf_Lock;
    lpVtbl->ReleaseDC = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, HDC)) Surf_ReleaseDC;
    lpVtbl->Restore = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE)) Surf_Restore;
    lpVtbl->SetClipper = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDIRECTDRAWCLIPPER)) Surf_SetClipper;
    lpVtbl->SetColorKey = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, DWORD, LPDDCOLORKEY)) Surf_SetColorKey;
    lpVtbl->SetOverlayPosition = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LONG, LONG)) Surf_SetOverlayPosition;
    lpVtbl->SetPalette = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPDIRECTDRAWPALETTE)) Surf_SetPalette;
    lpVtbl->Unlock = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPVOID)) Surf_Unlock;
    lpVtbl->UpdateOverlay = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, LPRECT, LPDIRECTDRAWSURFACE, LPRECT, DWORD, LPDDOVERLAYFX)) Surf_UpdateOverlay;
    lpVtbl->UpdateOverlayDisplay = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, DWORD)) Surf_UpdateOverlayDisplay;
    lpVtbl->UpdateOverlayZOrder = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWSURFACE, DWORD, LPDIRECTDRAWSURFACE)) Surf_UpdateOverlayZOrder;

    return w;
}

ULONG STDMETHODCALLTYPE Clip_AddRef(WrappedClipper *This)
{
    LONG newRef;
    printf(("Clip_AddRef: this=0x%p\n", This));
    newRef = InterlockedIncrement(&This->ref);
    This->real->lpVtbl->AddRef(This->real);
    return newRef;
}

HRESULT STDMETHODCALLTYPE Clip_QueryInterface(WrappedClipper *This, REFIID riid, LPVOID *ppvObj)
{
    printf(("Clip_QueryInterface: this=0x%p riid=0x%p out=0x%p\n", This, &riid, ppvObj));

    if (!ppvObj) {
        return E_POINTER;
    }
    *ppvObj = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectDrawClipper)) {
        Clip_AddRef(This);
        *ppvObj = This;
        return S_OK;
    }

    return This->real->lpVtbl->QueryInterface(This->real, riid, ppvObj);
}

ULONG STDMETHODCALLTYPE Clip_Release(WrappedClipper *This)
{
    LONG newRef;
    printf(("Clip_Release: this=0x%p\n", This));
    This->real->lpVtbl->Release(This->real);
    newRef = InterlockedDecrement(&This->ref);
    if (newRef == 0) {
        HeapFree(GetProcessHeap(), 0, This->lpVtbl);
        HeapFree(GetProcessHeap(), 0, This);
        return 0;
    }
    return newRef;
}

HRESULT STDMETHODCALLTYPE Clip_GetClipList(WrappedClipper *This, LPRECT lpRect, LPRGNDATA lpData, LPDWORD lpSize)
{
    HRESULT hr = This->real->lpVtbl->GetClipList(This->real, lpRect, lpData, lpSize);
    if (lpRect) {
        printf(("Clip_GetClipList: this=0x%p rect=(%ld,%ld)-(%ld,%ld) data=0x%p size=0x%p result=0x%08lX\n", This, lpRect->left, lpRect->top, lpRect->right, lpRect->bottom, lpData, lpSize, hr));
    } else {
        printf(("Clip_GetClipList: this=0x%p rect=NULL data=0x%p size=0x%p result=0x%08lX\n", This, lpData, lpSize, hr));
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE Clip_GetHWnd(WrappedClipper *This, HWND *pHwnd)
{
    HRESULT hr = This->real->lpVtbl->GetHWnd(This->real, pHwnd);
    printf(("Clip_GetHWnd: this=0x%p hwnd_out=0x%p result=0x%08lX\n", This, pHwnd, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Clip_Initialize(WrappedClipper *This, WrappedDirectDraw *lpDD, DWORD dwFlags)
{
    HRESULT hr = This->real->lpVtbl->Initialize(This->real, lpDD->real, dwFlags);
    printf(("Clip_Initialize: this=0x%p dd_wrapped=0x%p flags=0x%08lX result=0x%08lX\n", This, lpDD, dwFlags, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Clip_IsClipListChanged(WrappedClipper *This, LPBOOL lpChanged)
{
    HRESULT hr = This->real->lpVtbl->IsClipListChanged(This->real, lpChanged);
    printf(("Clip_IsClipListChanged: this=0x%p changed=0x%p result=0x%08lX\n", This, lpChanged, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Clip_SetClipList(WrappedClipper *This, LPRGNDATA lpData, DWORD dwFlags)
{
    HRESULT hr = This->real->lpVtbl->SetClipList(This->real, lpData, dwFlags);
    printf(("Clip_SetClipList: this=0x%p data=0x%p flags=0x%08lX result=0x%08lX\n", This, lpData, dwFlags, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Clip_SetHWnd(WrappedClipper *This, DWORD dwFlags, HWND hwnd)
{
    HRESULT hr = This->real->lpVtbl->SetHWnd(This->real, dwFlags, hwnd);
    printf(("Clip_SetHWnd: this=0x%p flags=0x%08lX hwnd=0x%p result=0x%08lX\n", This, dwFlags, hwnd, hr));
    return hr;
}

WrappedClipper *WRAP_Clipper(LPDIRECTDRAWCLIPPER real)
{
    WrappedClipper *w;
    IDirectDrawClipperVtbl *lpVtbl;
    if (!real) {
        return NULL;
    }

    w = (WrappedClipper *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WrappedClipper));
    if (!w) {
        return NULL;
    }

    lpVtbl = (IDirectDrawClipperVtbl *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectDrawClipperVtbl));
    if (!lpVtbl) {
        HeapFree(GetProcessHeap(), 0, w);
        return NULL;
    }

    lpVtbl->QueryInterface = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWCLIPPER, REFIID, LPVOID *)) Clip_QueryInterface;
    lpVtbl->AddRef = (ULONG (STDMETHODCALLTYPE *)(LPDIRECTDRAWCLIPPER)) Clip_AddRef;
    lpVtbl->Release = (ULONG (STDMETHODCALLTYPE *)(LPDIRECTDRAWCLIPPER)) Clip_Release;
    lpVtbl->GetClipList = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWCLIPPER, LPRECT, LPRGNDATA, LPDWORD)) Clip_GetClipList;
    lpVtbl->GetHWnd = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWCLIPPER, HWND *)) Clip_GetHWnd;
    lpVtbl->Initialize = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWCLIPPER, LPDIRECTDRAW, DWORD)) Clip_Initialize;
    lpVtbl->IsClipListChanged = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWCLIPPER, LPBOOL)) Clip_IsClipListChanged;
    lpVtbl->SetClipList = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWCLIPPER, LPRGNDATA, DWORD)) Clip_SetClipList;
    lpVtbl->SetHWnd = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWCLIPPER, DWORD, HWND)) Clip_SetHWnd;

    w->lpVtbl = lpVtbl;
    w->real = real;
    w->ref = 1;

    return w;
}

ULONG STDMETHODCALLTYPE Pal_AddRef(WrappedPalette *This)
{
    LONG newRef;
    printf(("Pal_AddRef: this=0x%p\n", This));
    newRef = InterlockedIncrement(&This->ref);
    This->real->lpVtbl->AddRef(This->real);
    return newRef;
}

HRESULT STDMETHODCALLTYPE Pal_QueryInterface(WrappedPalette *This, REFIID riid, LPVOID *ppvObj)
{
    printf(("Pal_QueryInterface: this=0x%p riid=0x%p out=0x%p\n", This, &riid, ppvObj));

    if (!ppvObj) {
        return E_POINTER;
    }
    *ppvObj = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectDrawPalette)) {
        Pal_AddRef(This);
        *ppvObj = This;
        return S_OK;
    }

    return This->real->lpVtbl->QueryInterface(This->real, riid, ppvObj);
}

ULONG STDMETHODCALLTYPE Pal_Release(WrappedPalette *This)
{
    LONG newRef;
    printf(("Pal_Release: this=0x%p\n", This));
    This->real->lpVtbl->Release(This->real);
    newRef = InterlockedDecrement(&This->ref);
    if (newRef == 0) {
        HeapFree(GetProcessHeap(), 0, This->lpVtbl);
        HeapFree(GetProcessHeap(), 0, This);
        return 0;
    }
    return newRef;
}

HRESULT STDMETHODCALLTYPE Pal_GetCaps(WrappedPalette *This, LPDWORD pdw)
{
    HRESULT hr = This->real->lpVtbl->GetCaps(This->real, pdw);
    printf(("Pal_GetCaps: pal=0x%p out=0x%p result=0x%08lX\n", This, pdw, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Pal_GetEntries(WrappedPalette *This, DWORD dwFlags, DWORD dwStart, DWORD dwCount, LPPALETTEENTRY lpEntries)
{
    HRESULT hr = This->real->lpVtbl->GetEntries(This->real, dwFlags, dwStart, dwCount, lpEntries);
    printf(("Pal_GetEntries: pal=0x%p flags=0x%08lX start=%lu count=%lu out=0x%p result=0x%08lX\n", This, dwFlags, dwStart, dwCount, lpEntries, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Pal_Initialize(WrappedPalette *This, LPDIRECTDRAW lpDD, DWORD dwFlags, LPPALETTEENTRY lpEntries)
{
    HRESULT hr = This->real->lpVtbl->Initialize(This->real, lpDD, dwFlags, lpEntries);
    printf(("Pal_Initialize: pal=0x%p dd=0x%p flags=0x%08lX entries=0x%p result=0x%08lX\n", This, lpDD, dwFlags, lpEntries, hr));
    return hr;
}

HRESULT STDMETHODCALLTYPE Pal_SetEntries(WrappedPalette *This, DWORD dwFlags, DWORD dwStart, DWORD dwCount, LPPALETTEENTRY lpEntries)
{
    HRESULT hr = This->real->lpVtbl->SetEntries(This->real, dwFlags, dwStart, dwCount, lpEntries);
    printf(("Pal_SetEntries: pal=0x%p flags=0x%08lX start=%lu count=%lu entries=0x%p result=0x%08lX\n", This, dwFlags, dwStart, dwCount, lpEntries, hr));
    return hr;
}

WrappedPalette *WRAP_Palette(LPDIRECTDRAWPALETTE real)
{
    WrappedPalette *w;
    IDirectDrawPaletteVtbl *lpVtbl;
    if (!real) {
        return NULL;
    }

    w = (WrappedPalette *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WrappedPalette));
    if (!w) {
        return NULL;
    }

    lpVtbl = (IDirectDrawPaletteVtbl *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectDrawPaletteVtbl));
    if (!lpVtbl) {
        HeapFree(GetProcessHeap(), 0, w);
        return NULL;
    }

    lpVtbl->QueryInterface = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWPALETTE, REFIID, LPVOID *)) Pal_QueryInterface;
    lpVtbl->AddRef = (ULONG (STDMETHODCALLTYPE *)(LPDIRECTDRAWPALETTE)) Pal_AddRef;
    lpVtbl->Release = (ULONG (STDMETHODCALLTYPE *)(LPDIRECTDRAWPALETTE)) Pal_Release;
    lpVtbl->GetCaps = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWPALETTE, LPDWORD)) Pal_GetCaps;
    lpVtbl->GetEntries = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWPALETTE, DWORD, DWORD, DWORD, LPPALETTEENTRY)) Pal_GetEntries;
    lpVtbl->Initialize = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWPALETTE, LPDIRECTDRAW, DWORD, LPPALETTEENTRY)) Pal_Initialize;
    lpVtbl->SetEntries = (HRESULT (STDMETHODCALLTYPE *)(LPDIRECTDRAWPALETTE, DWORD, DWORD, DWORD, LPPALETTEENTRY)) Pal_SetEntries;

    w->lpVtbl = lpVtbl;
    w->real = real;
    w->ref = 1;

    return w;
}

HRESULT WINAPI DirectDrawCreate(LPGUID lpGUID, LPDIRECTDRAW *ppDD, IUnknown *iu)
{
    LPDIRECTDRAW realDD;
    HRESULT hr;
    printf(("DirectDrawCreate: entry lpGUID=0x%p out_wrapped=0x%p pUnkOuter=0x%p\n", lpGUID, ppDD, iu));

    LoadRealDDraw();
    if (!pReal_DirectDrawCreate) {
        printf(("DirectDrawCreate returned DDERR_UNSUPPORTED\n"));
        return DDERR_UNSUPPORTED;
    }

    realDD = NULL;
    hr = pReal_DirectDrawCreate(lpGUID, &realDD, iu);
    if (FAILED(hr)) {
        printf(("DirectDrawCreate returned 0x%08lX\n", hr));
        return hr;
    }

    *ppDD = (LPDIRECTDRAW) WRAP_DirectDraw(realDD);
    printf(("DirectDrawCreate: returned wrapped=0x%p\n", *ppDD));
    return S_OK;
}

HRESULT WINAPI DirectDrawEnumerateA(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext)
{
    HRESULT hr;
    printf(("DirectDrawEnumerateA: entry callback=0x%p ctx=0x%p\n", lpCallback, lpContext));

    LoadRealDDraw();
    if (!pReal_DirectDrawEnumerateA) {
        return DDERR_UNSUPPORTED;
    }
    hr = pReal_DirectDrawEnumerateA(lpCallback, lpContext);
    printf(("DirectDrawEnumerateA returned 0x%08lX\n", hr));
    return hr;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    OSVERSIONINFO osvi;
    printf(("DllMain: reason=%lu hinst=0x%p\n", fdwReason, hinstDLL));

    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&osvi);
#ifndef NDEBUG
        if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
            g_fpDebug = fopen("ddraw_wrapper.log", "wt");
            g_fdDebug = _fileno(g_fpDebug);
        }
#endif
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        if (g_hRealDDraw) {
            FreeLibrary(g_hRealDDraw);
            g_hRealDDraw = NULL;
        }
#ifndef NDEBUG
        if (g_fpDebug != NULL) {
            fclose(g_fpDebug);
            g_fpDebug = NULL;
            g_fdDebug = -1;
        }
#endif
    }
    return TRUE;
}