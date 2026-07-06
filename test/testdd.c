#include <stdio.h>
#include <windows.h>
#include <ddraw.h>

#define BMP_WIDTH 64
#define BMP_HEIGHT 64
#define BMP_PITCH ((BMP_WIDTH*3 + 3) & ~3)
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define TEST_BPP 8
#define _256_TO_6(x) (((x) + 25)/51)

void DbgPrint(const char *format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    _vsnprintf(buffer, sizeof(buffer), format, args);
    // OutputDebugStringA(buffer);
    MessageBoxA(NULL, buffer, "DirectDraw Test", MB_ICONEXCLAMATION);
    va_end(args);
}

LPDIRECTDRAW lpDD = NULL;
LPDIRECTDRAWSURFACE lpPrimary = NULL;
LPDIRECTDRAWSURFACE lpBackBuffer = NULL;
LPDIRECTDRAWSURFACE lpImage = NULL;
LPDIRECTDRAWPALETTE lpPalette = NULL;

int x = 0, y = 0;
int dx = 1, dy = 1;

void RestoreCursorAndMode(void) {
    ClipCursor(NULL);
    while (ShowCursor(TRUE) < 0) {
        Sleep(1);
    }
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    if (lpDD != NULL) {
        IDirectDraw_RestoreDisplayMode(lpDD);
    }
}

HRESULT CreateAndAttachPalette() {
    PALETTEENTRY entries[256];
    int i, r, g, b;

    /*
    for (i = 0; i < 256; ++i) {
        entries[i].peRed = i;
        entries[i].peGreen = i;
        entries[i].peBlue = i;
        entries[i].peFlags = 0;
    }
    */
    i = 0;
    for (r = 0; r < 6; ++r) {
        for (g = 0; g < 6; ++g) {
            for (b = 0; b < 6; ++b) {
                entries[i].peRed = r*51;
                entries[i].peGreen = g*51;
                entries[i].peBlue = b*51;
                entries[i].peFlags = 0;
                ++i;
            }
        }
    }
    for (; i < 256; ++i) {
        entries[i].peRed = 0;
        entries[i].peGreen = 0;
        entries[i].peBlue = 0;
        entries[i].peFlags = 0;
    }

    if (FAILED(IDirectDraw_CreatePalette(lpDD, DDPCAPS_8BIT | DDPCAPS_ALLOW256, entries, &lpPalette, NULL))) {
        DbgPrint("CreatePalette Failed");
        return E_FAIL;
    }

    if (FAILED(IDirectDrawSurface_SetPalette(lpPrimary, lpPalette))) {
        DbgPrint("SetPalette(primary) Failed");
        return E_FAIL;
    }

    if (FAILED(IDirectDrawSurface_SetPalette(lpBackBuffer, lpPalette))) {
        DbgPrint("SetPalette(backbuffer) Failed");
        return E_FAIL;
    }

    return DD_OK;
}

HRESULT InitDirectDraw(HWND hwnd) {
    DDSURFACEDESC ddsd;
    DDSCAPS ddscaps;

    if (FAILED(DirectDrawCreate(NULL, &lpDD, NULL))) {
        DbgPrint("DirectDrawCreate Failed");
        return E_FAIL;
    }
    if (FAILED(IDirectDraw_SetCooperativeLevel(lpDD, hwnd, DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE))) {
        DbgPrint("SetCooperativeLevel Failed");
        return E_FAIL;
    }
    if (FAILED(IDirectDraw_SetDisplayMode(lpDD, SCREEN_WIDTH, SCREEN_HEIGHT, TEST_BPP))) {
        DbgPrint("SetDisplayMode Failed");
        return E_FAIL;
    }

    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    ddsd.dwBackBufferCount = 1;

    if (FAILED(IDirectDraw_CreateSurface(lpDD, &ddsd, &lpPrimary, NULL))) {
        DbgPrint("CreateSurface Failed");
        return E_FAIL;
    }

    ZeroMemory(&ddscaps, sizeof(ddscaps));
    ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
    if (FAILED(IDirectDrawSurface_GetAttachedSurface(lpPrimary, &ddscaps, &lpBackBuffer))) {
        DbgPrint("GetAttachedSurface Failed");
        return E_FAIL;
    }

    if (TEST_BPP == 8) {
        if (FAILED(CreateAndAttachPalette())) {
            return E_FAIL;
        }
    }

    return DD_OK;
}

HRESULT LoadBMPToSurface(LPDIRECTDRAWSURFACE* surface, LPCSTR filename) {
    DDSURFACEDESC ddsd;
    /*
    HBITMAP hBmp, hOldBmp;
    BITMAP bmp;
    HDC hdc, hdcBmp;
    */
    FILE *fp;
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;
    BYTE bmBits[BMP_HEIGHT*BMP_PITCH];
    LPBYTE src, dst;
    int i, j;

    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    ddsd.dwWidth = BMP_WIDTH;
    ddsd.dwHeight = BMP_HEIGHT;

    if (FAILED(IDirectDraw_CreateSurface(lpDD, &ddsd, surface, NULL))) {
        DbgPrint("CreateSurface Failed");
        return E_FAIL;
    }

    if (TEST_BPP == 8 && lpPalette != NULL) {
        if (FAILED(IDirectDrawSurface_SetPalette(*surface, lpPalette))) {
            DbgPrint("SetPalette(image) Failed");
            return E_FAIL;
        }
    }

    // Method 1: BitBlt
    /*
    hBmp = (HBITMAP) LoadImage(NULL, "wizard.bmp", IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_LOADFROMFILE);
    if (!hBmp) {
        DbgPrint("Error %d: Could not open wizard.bmp\n", GetLastError());
        return E_FAIL;
    }
    if (!GetObject(hBmp, sizeof(bmp), &bmp)) {
        DbgPrint("Error %d: Could not get BITMAP\n", GetLastError());
        return E_FAIL;
    }
    if (bmp.bmWidth != BMP_WIDTH || bmp.bmHeight != BMP_HEIGHT || bmp.bmBitsPixel != 24) {
        DbgPrint("Error: wizard.bmp should be %d x %d x 24, actual %d x %d x %d\n", BMP_WIDTH, BMP_HEIGHT, bmp.bmWidth, bmp.bmHeight, bmp.bmBitsPixel);
        return E_FAIL;
    }

    if (FAILED(IDirectDrawSurface_GetDC(*surface, &hdc))) {
        DbgPrint("GetDC Failed");
        return E_FAIL;
    }
    hdcBmp = CreateCompatibleDC(NULL);
    hOldBmp = (HBITMAP) SelectObject(hdcBmp, hBmp);
    BitBlt(hdc, 0, 0, BMP_WIDTH, BMP_HEIGHT, hdcBmp, 0, 0, SRCCOPY);
    SelectObject(hdcBmp, hOldBmp);
    DeleteDC(hdcBmp);
    IDirectDrawSurface_ReleaseDC(*surface, hdc);

    DeleteObject(hBmp);
    */

    // Method 2: Copy Memory
    fp = fopen("wizard.bmp", "rb");
    if (fp == NULL) {
        DbgPrint("Error %d (%s): Could not open wizard.bmp\n", errno, strerror(errno));
        return E_FAIL;
    }
    if (fread(&bmfh, sizeof(bmfh), 1, fp) != 1 || fread(&bmih, sizeof(bmih), 1, fp) != 1) {
        DbgPrint("Error: Could not read file or info header of wizard.bmp\n");
        fclose(fp);
        return E_FAIL;
    }
    if (bmfh.bfType != 0x4D42 || bmih.biWidth != BMP_WIDTH || bmih.biHeight != BMP_HEIGHT || bmih.biBitCount != 24) {
        DbgPrint("Error: wizard.bmp should be \"BM\" %d x %d x 24, actual 0x%04X %d x %d x %d\n", BMP_WIDTH, BMP_HEIGHT, bmfh.bfType, bmih.biWidth, bmih.biHeight, bmih.biBitCount);
        fclose(fp);
        return E_FAIL;
    }
    fseek(fp, bmfh.bfOffBits, SEEK_SET);
    if (fread(bmBits, sizeof(bmBits), 1, fp) != 1) {
        DbgPrint("Error: Could not read data of wizard.bmp\n");
        fclose(fp);
        return E_FAIL;
    }
    fclose(fp);

    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    if (FAILED(IDirectDrawSurface_Lock(*surface, NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL))) {
        DbgPrint("Lock Failed");
        return E_FAIL;
    }

    for (i = 0; i < BMP_HEIGHT; i ++) {
        src = bmBits + (BMP_HEIGHT - 1 - i)*BMP_PITCH;
        dst = (LPBYTE) ddsd.lpSurface + i*ddsd.lPitch;
        for (j = 0; j < BMP_WIDTH; j ++) {
#if TEST_BPP == 8
            dst[0] = _256_TO_6(src[2])*36 + _256_TO_6(src[1])*6 + _256_TO_6(src[0]);
            dst ++;
#elif TEST_BPP == 16
            ((LPWORD) dst)[0] = ((src[2]>>3)<<11) | ((src[1]>>2)<<5) | (src[0]>>3);
            dst += 2;
#elif TEST_BPP == 24 || TEST_BPP == 32
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
    #if TEST_BPP == 32
            dst[3] = 0xFF;
            dst += 4;
    #else
            dst += 3;
    #endif
#else
    #error "set TEST_BPP to 8|16|24|32 for framebuffer packing"
#endif
            src += 3;
        }
    }

    IDirectDrawSurface_Unlock(*surface, NULL);

    return DD_OK;
}

void CleanUp() {
    if (lpImage != NULL) {
        IDirectDrawSurface_Release(lpImage);
        lpImage = NULL;
    }

    if (lpPalette != NULL) {
        IDirectDrawPalette_Release(lpPalette);
        lpPalette = NULL;
    }

    if (lpBackBuffer != NULL) {
        IDirectDrawSurface_Release(lpBackBuffer);
        lpBackBuffer = NULL;
    }

    if (lpPrimary != NULL) {
        IDirectDrawSurface_Release(lpPrimary);
        lpPrimary = NULL;
    }

    RestoreCursorAndMode();

    if (lpDD != NULL) {
        IDirectDraw_Release(lpDD);
        lpDD = NULL;
    }
}

void UpdatePosition() {
    x += dx;
    y += dy;

    if (x < 0 || x + BMP_WIDTH > SCREEN_WIDTH) {
        dx = -dx;
        x += dx*2;
    }
    if (y < 0 || y + BMP_HEIGHT > SCREEN_HEIGHT) {
        dy = -dy;
        y += dy*2;
    }
}

void Render() {
    IDirectDrawSurface_BltFast(lpBackBuffer, x, y, lpImage, NULL, DDBLTFAST_WAIT);
    IDirectDrawSurface_Flip(lpPrimary, NULL, DDFLIP_WAIT);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    WNDCLASS wc;
    HWND hwnd;
    MSG msg;

    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "DDrawWnd";
    RegisterClass(&wc);

    hwnd = CreateWindow("DDrawWnd", "DirectDraw Demo", WS_POPUP, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, SW_SHOW);

    if (FAILED(InitDirectDraw(hwnd))) {
        DbgPrint("InitDirectDraw Failed");
        return 0;
    }
    if (FAILED(LoadBMPToSurface(&lpImage, "wizard.bmp"))) {
        DbgPrint("LoadBMPToSurface Failed");
        return 0;
    }

    while (TRUE) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT || (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)) {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        UpdatePosition();
        Render();
        Sleep(10);
    }

    CleanUp();
    return 0;
}
