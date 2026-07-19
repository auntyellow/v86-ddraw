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
  va_list args;
  char buffer[512];
  int i;
  va_start(args, format);
  i = _vsnprintf(buffer, sizeof(buffer), format, args);
  buffer[i < 0 ? sizeof(buffer) - 1 : i] = '\0';
  OutputDebugStringA(buffer);
  // TODO write into file under Win9x
  // MessageBoxA(NULL, buffer, "DirectDraw Test", MB_ICONEXCLAMATION);
  va_end(args);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_DESTROY || (msg == WM_KEYDOWN && wParam == VK_ESCAPE)) {
    PostQuitMessage(0);
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ReleaseSurface(LPDIRECTDRAWSURFACE lpSurface) {
  if (lpSurface != NULL) {
    IDirectDrawSurface_Release(lpSurface);
  }
}

int CleanUp(int ret, HINSTANCE hInst, HWND hwnd, LPDIRECTDRAW lpDD, LPDIRECTDRAWSURFACE lpPrimary, LPDIRECTDRAWSURFACE lpBackBuffer, LPDIRECTDRAWSURFACE lpImage, LPDIRECTDRAWPALETTE lpPalette) {
  ReleaseSurface(lpImage);
  ReleaseSurface(lpBackBuffer);
  ReleaseSurface(lpPrimary);
  if (lpPalette != NULL) {
    IDirectDrawPalette_Release(lpPalette);
  }
  if (lpDD != NULL) {
    IDirectDraw_Release(lpDD);
  }
  DestroyWindow(hwnd);
  UnregisterClass("DDrawWnd", hInst);
  return ret;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  WNDCLASS wc;
  HWND hwnd;
  FILE *fp;
  BITMAPFILEHEADER bmfh;
  BITMAPINFOHEADER bmih;
  BYTE bmBits[BMP_HEIGHT*BMP_PITCH];
  LPBYTE src, dst, lpSurfaceImage;
  MSG msg;
  HRESULT hr;
  DDSURFACEDESC ddsd;
  DDSCAPS ddscaps;
  LPDIRECTDRAW lpDD;
  LPDIRECTDRAWSURFACE lpPrimary, lpBackBuffer, lpImage;
  LPDIRECTDRAWPALETTE lpPalette;
  RECT rect;
  LONG lPitchImage;
  int i, x, y, dx, dy;
  /*
  HBITMAP hBmp, hOldBmp;
  BITMAP bmp;
  HDC hdc, hdcBmp;
  */
#if TEST_BPP == 8
  PALETTEENTRY entries[256];
  int r, g, b;
#endif

  // Read BMP
  fp = fopen("wizard.bmp", "rb");
  if (fp == NULL) {
    DbgPrint("Could not open wizard.bmp, error = %d (%s)\n", errno, strerror(errno));
    return 1;
  }
  if (fread(&bmfh, sizeof(bmfh), 1, fp) != 1 || fread(&bmih, sizeof(bmih), 1, fp) != 1) {
    DbgPrint("Error: Could not read file or info header of wizard.bmp\n");
    fclose(fp);
    return 1;
  }
  if (bmfh.bfType != 0x4D42 || bmih.biWidth != BMP_WIDTH || bmih.biHeight != BMP_HEIGHT || bmih.biBitCount != 24) {
    DbgPrint("Error: wizard.bmp should be \"BM\" %d x %d x 24, actual 0x%04X %d x %d x %d\n", BMP_WIDTH, BMP_HEIGHT, bmfh.bfType, bmih.biWidth, bmih.biHeight, bmih.biBitCount);
    fclose(fp);
    return 1;
  }
  fseek(fp, bmfh.bfOffBits, SEEK_SET);
  if (fread(bmBits, sizeof(bmBits), 1, fp) != 1) {
    DbgPrint("Error: Could not read data of wizard.bmp\n");
    fclose(fp);
    return 1;
  }
  fclose(fp);

  // Create Window
  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = "DDrawWnd";
  if (RegisterClass(&wc) == 0) {
    DbgPrint("Could not register class \"DDrawWnd\", error = %lu\n", GetLastError());
    return 1;
  }
  hwnd = CreateWindow("DDrawWnd", "DirectDraw Demo", WS_POPUP | WS_VISIBLE, 0, 0, 0 /* SCREEN_WIDTH */, 0 /* SCREEN_HEIGHT */, NULL, NULL, hInst, NULL);
  if (!hwnd) {
    DbgPrint("Could not create window, error = %lu\n", GetLastError());
    return 1;
  }
  ShowWindow(hwnd, SW_SHOW);

  // Init DirectDraw, Surfaces and Palette
  lpDD = NULL;
  hr = DirectDrawCreate(NULL, &lpDD, NULL);
  if (FAILED(hr)) {
    DbgPrint("DirectDrawCreate Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, NULL, NULL, NULL, NULL, NULL);
  }
  hr = IDirectDraw_SetCooperativeLevel(lpDD, hwnd, DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE);
  if (FAILED(hr)) {
    DbgPrint("SetCooperativeLevel Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, lpDD, NULL, NULL, NULL, NULL);
  }
  hr = IDirectDraw_SetDisplayMode(lpDD, SCREEN_WIDTH, SCREEN_HEIGHT, TEST_BPP);
  if (FAILED(hr)) {
    DbgPrint("SetCooperativeLevel Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, lpDD, NULL, NULL, NULL, NULL);
  }

  lpPrimary = NULL;
  ZeroMemory(&ddsd, sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);
  ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
  ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
  ddsd.dwBackBufferCount = 1;
  hr = IDirectDraw_CreateSurface(lpDD, &ddsd, &lpPrimary, NULL);
  if (FAILED(hr)) {
    DbgPrint("CreateSurface (Primary) Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, lpDD, NULL, NULL, NULL, NULL);
  }

  lpBackBuffer = NULL;
  ZeroMemory(&ddscaps, sizeof(ddscaps));
  ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
  hr = IDirectDrawSurface_GetAttachedSurface(lpPrimary, &ddscaps, &lpBackBuffer);
  if (FAILED(hr)) {
    DbgPrint("GetAttachedSurface Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, NULL, NULL, NULL);
  }

  lpImage = NULL;
  ZeroMemory(&ddsd, sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);
  ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
  ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
  ddsd.dwWidth = BMP_WIDTH;
  ddsd.dwHeight = BMP_HEIGHT;
  hr = IDirectDraw_CreateSurface(lpDD, &ddsd, &lpImage, NULL);
  if (FAILED(hr)) {
    DbgPrint("CreateSurface (Image) Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, NULL, NULL);
  }

  lpPalette = NULL;
#if TEST_BPP == 8
  /*
  for (i = 0; i < 256; ++i) {
    entries[i].peRed = i;
    entries[i].peGreen = i;
    entries[i].peBlue = i;
    entries[i].peFlags = 0;
  }
  */
  i = 0;
  for (r = 0; r < 6; r ++) {
    for (g = 0; g < 6; g ++) {
      for (b = 0; b < 6; b ++) {
        entries[i].peRed = r*51;
        entries[i].peGreen = g*51;
        entries[i].peBlue = b*51;
        entries[i].peFlags = 0;
        i ++;
      }
    }
  }
  for (; i < 256; i ++) {
    entries[i].peRed = 0;
    entries[i].peGreen = 0;
    entries[i].peBlue = 0;
    entries[i].peFlags = 0;
  }
  hr = IDirectDraw_CreatePalette(lpDD, DDPCAPS_8BIT | DDPCAPS_ALLOW256, entries, &lpPalette, NULL);
  if (FAILED(hr)) {
    DbgPrint("CreatePalette Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, NULL, NULL);
  }
  hr = IDirectDrawSurface_SetPalette(lpPrimary, lpPalette);
  if (FAILED(hr)) {
    DbgPrint("SetPalette (Primary) Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, NULL, lpPalette);
  }
  hr = IDirectDrawSurface_SetPalette(lpBackBuffer, lpPalette);
  if (FAILED(hr)) {
    DbgPrint("SetPalette (BackBuffer) Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, NULL, lpPalette);
  }
  hr = IDirectDrawSurface_SetPalette(lpImage, lpPalette);
  if (FAILED(hr)) {
    DbgPrint("SetPalette (Image) Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, lpImage, lpPalette);
  }
#endif

  // Prepare Image
  // Method 1: BitBlt
  /*
  hBmp = (HBITMAP) LoadImage(NULL, "wizard.bmp", IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_LOADFROMFILE);
  if (!hBmp) {
    DbgPrint("Could not open wizard.bmp, error = %lu\n", GetLastError());
    return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, lpImage, lpPalette);
  }
  if (!GetObject(hBmp, sizeof(bmp), &bmp)) {
    DbgPrint("Could not get BITMAP, error = %lu\n", GetLastError());
    return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, lpImage, lpPalette);
  }
  if (bmp.bmWidth != BMP_WIDTH || bmp.bmHeight != BMP_HEIGHT || bmp.bmBitsPixel != 24) {
    DbgPrint("Error: wizard.bmp should be %d x %d x 24, actual %d x %d x %d\n", BMP_WIDTH, BMP_HEIGHT, bmp.bmWidth, bmp.bmHeight, bmp.bmBitsPixel);
    return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, lpImage, lpPalette);
  }
  hr = IDirectDrawSurface_GetDC(lpImage, &hdc);
  if (FAILED(hr)) {
    DbgPrint("GetDC Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, lpImage, lpPalette);
  }
  hdcBmp = CreateCompatibleDC(NULL);
  hOldBmp = (HBITMAP) SelectObject(hdcBmp, hBmp);
  BitBlt(hdc, 0, 0, BMP_WIDTH, BMP_HEIGHT, hdcBmp, 0, 0, SRCCOPY);
  SelectObject(hdcBmp, hOldBmp);
  DeleteDC(hdcBmp);
  IDirectDrawSurface_ReleaseDC(lpImage, hdc);
  DeleteObject(hBmp);
  */

  // Method 2: Copy Memory
  ZeroMemory(&ddsd, sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);
  hr = IDirectDrawSurface_Lock(lpImage, NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL);
  if (FAILED(hr)) {
    DbgPrint("Lock Failed, error = 0x%08X", hr);
    return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, lpImage, lpPalette);
  }

  lpSurfaceImage = (LPBYTE) ddsd.lpSurface;
  lPitchImage = ddsd.lPitch;
  for (y = 0; y < BMP_HEIGHT; y ++) {
    src = bmBits + (BMP_HEIGHT - 1 - y)*BMP_PITCH;
    dst = lpSurfaceImage + y*lPitchImage;
    for (x = 0; x < BMP_WIDTH; x ++) {
#if TEST_BPP == 8
      dst[0] = _256_TO_6(src[2])*36 + _256_TO_6(src[1])*6 + _256_TO_6(src[0]);
      dst ++;
#elif TEST_BPP == 16
      // ReactOS Generic VESA Adapter: 5:5:5
      // ((LPWORD) dst)[0] = ((src[2]>>3)<<10) | ((src[1]>>3)<<5) | (src[0]>>3);
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

  // IDirectDrawSurface_Unlock(lpImage, NULL);

  x = 0;
  y = 0;
  dx = 1;
  dy = 1;
  while (TRUE) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        break;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    // Update Position
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
    // DbgPrint("Position: (%d, %d)\n", x, y);
    // Render
    // IDirectDrawSurface_BltFast(lpBackBuffer, x, y, lpImage, NULL, DDBLTFAST_WAIT);

    // Write into BackBuffer Directly
    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    rect.left = x;
    rect.top = y;
    rect.right = x + BMP_WIDTH;
    rect.bottom = y + BMP_HEIGHT;
    // Lock(lpPrimary, ...) returns DDERR_CANTLOCKSURFACE under VBEMP
    // Lock(..., &rect, ...) doesn't work under cnc/vga-ddraw
    // hr = IDirectDrawSurface_Lock(lpBackBuffer, &rect, &ddsd, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
    hr = IDirectDrawSurface_Lock(lpBackBuffer, NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);
    if (FAILED(hr)) {
      DbgPrint("Lock Failed, error = 0x%08X", hr);
      return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, lpImage, lpPalette);
    }
    for (i = 0; i < BMP_HEIGHT; i ++) {
      // for Lock(..., &rect, ...)
      // memcpy((LPBYTE) ddsd.lpSurface + i*ddsd.lPitch, lpSurfaceImage + i*lPitchImage, lPitchImage);
      // for Lock(..., NULL, ...)
      memcpy((LPBYTE) ddsd.lpSurface + (y + i)*ddsd.lPitch + x*(TEST_BPP/8), lpSurfaceImage + i*lPitchImage, lPitchImage);
    }
    // Unlock(lpBackBuffer, &rect) returns DDERR_NOTLOCKED
    hr = IDirectDrawSurface_Unlock(lpBackBuffer, NULL);
    if (FAILED(hr)) {
      DbgPrint("Unlock returned 0x%08X", hr);
      return CleanUp(1, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, lpImage, lpPalette);
    }

    IDirectDrawSurface_Flip(lpPrimary, NULL, DDFLIP_WAIT);
    Sleep(10);
  }

  IDirectDrawSurface_Unlock(lpImage, NULL);

  // Reset Cursor
  ClipCursor(NULL);
  for (i = 0; i < 1000; i ++) {
    if (ShowCursor(TRUE) >= 0) {
      break;
    }
    Sleep(1);
  }
  SetCursor(LoadCursor(NULL, IDC_ARROW));
  return CleanUp(0, hInst, hwnd, lpDD, lpPrimary, lpBackBuffer, lpImage, lpPalette);
}