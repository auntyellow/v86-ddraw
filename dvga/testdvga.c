#include <stdio.h>
#include <windows.h>
#define PITCH(x) ((x + 3) & ~3)
#define BMP_WIDTH 64
#define BMP_HEIGHT 64
#define BMP_PITCH PITCH(BMP_WIDTH*3)
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define TEST_BPP 8
#define XPITCH (TEST_BPP/8)
#define IMG_PITCH PITCH(BMP_WIDTH*XPITCH)
#define SCREEN_PITCH PITCH(SCREEN_WIDTH*XPITCH)
#define _256_TO_6(x) (((x) + 25)/51)

void DbgPrint(const char *format, ...) {
  char buffer[512];
  va_list args;
  va_start(args, format);
  _vsnprintf(buffer, sizeof(buffer), format, args);
  // OutputDebugStringA(buffer);
  MessageBoxA(NULL, buffer, "DVGA Test", MB_ICONEXCLAMATION);
  va_end(args);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  WNDCLASS wc;
  HWND hwnd;
  FILE *fp;
  BITMAPFILEHEADER bmfh;
  BITMAPINFOHEADER bmih;
  int x, y, dx, dy;
  DWORD written;
  BYTE bmBits[BMP_HEIGHT*BMP_PITCH];
  BYTE data[16 + BMP_HEIGHT*IMG_PITCH];
  LPBYTE src, dst;
  DEVMODE devMode;
  MSG msg;
  LONG result;
  BOOL success;
  HANDLE hDVGA;
#if TEST_BPP == 8
  HANDLE hDPal;
  HDC hdc;
  HPALETTE hPal;
  LPLOGPALETTE pLogPal;
  PALETTEENTRY *pe;
  int r, g, b;
  OSVERSIONINFO osVerInfo;
#endif

  // Read BMP
  fp = fopen("wizard.bmp", "rb");
  if (fp == NULL) {
    DbgPrint("Error %d (%s): Could not open wizard.bmp\n", errno, strerror(errno));
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

  for (y = 0; y < BMP_HEIGHT; y ++) {
    src = bmBits + (BMP_HEIGHT - 1 - y)*BMP_PITCH;
    dst = data + 16 + y*IMG_PITCH;
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

  hDVGA = CreateFile("\\\\.\\DirectVGA2", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (hDVGA == INVALID_HANDLE_VALUE) {
    DbgPrint("Error %d: Could not open device \\\\.\\DirectVGA2. Make sure the driver is loaded.\n", GetLastError());
    return 1;
  }

  // Change Display Mode
  ZeroMemory(&devMode, sizeof(devMode));
  devMode.dmSize = sizeof(devMode);
  if (!EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode)) {
    DbgPrint("Error %d: Could not get display mode.\n", GetLastError());
    // ignore under Windows NT 3.51
  };
  devMode.dmPelsWidth = SCREEN_WIDTH;
  devMode.dmPelsHeight = SCREEN_HEIGHT;
  devMode.dmBitsPerPel = TEST_BPP;
  devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
  result = ChangeDisplaySettings(&devMode, CDS_FULLSCREEN);
  if (result != DISP_CHANGE_SUCCESSFUL) {
    DbgPrint("Error %d: Could not change display mode.\n", result);
    return 1;
  }
  // Windows NT 3.51 returns success but doesn't work. Should change manually and restart

  // Create Window
  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = "DVGA2Wnd";
  RegisterClass(&wc);
  hwnd = CreateWindowEx(0, "DVGA2Wnd", "DVGA2 Demo", WS_POPUP | WS_VISIBLE, 0, 0, 0 /* SCREEN_WIDTH */, 0 /* SCREEN_HEIGHT */, NULL, NULL, hInst, NULL);
  if (!hwnd) {
    DbgPrint("Error %d: Could not create window.\n", GetLastError());
    return 1;
  }
  success = ShowWindow(hwnd, SW_SHOW);
  if (!success) {
    DbgPrint("Error %d: Could not create window.\n", GetLastError());
  }

#if TEST_BPP == 8
  pLogPal = (LPLOGPALETTE) HeapAlloc(GetProcessHeap(), 0, sizeof(LOGPALETTE) + 215*sizeof(PALETTEENTRY));
  pLogPal->palVersion = 0x300;
  pLogPal->palNumEntries = 216;
  pe = pLogPal->palPalEntry;
  for (r = 0; r < 6; r ++) {
    for (g = 0; g < 6; g ++) {
      for (b = 0; b < 6; b ++) {
        pe->peRed = r*51;
        pe->peGreen = g*51;
        pe->peBlue = b*51;
        pe->peFlags = 0;
        pe ++;
      }
    }
  }

  hDPal = CreateFile("\\\\.\\DirectPalette", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (hDPal == INVALID_HANDLE_VALUE) {
    // DbgPrint("Warning %d: Could not open device \\\\.\\DirectPalette.\n", GetLastError());
  } else {
    success = WriteFile(hDPal, pLogPal->palPalEntry, pLogPal->palNumEntries*4, &written, NULL);
    if (success) {
      // DbgPrint("Wrote %d bytes into VGA palette\n", written);
    } else {
      DbgPrint("Error %d: Write palette failed\n", GetLastError());
    }
    CloseHandle(hDPal);
  }
  osVerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&osVerInfo);
  if (osVerInfo.dwMajorVersion >= 5) {
    hPal = CreatePalette(pLogPal);
    if (!hPal) {
      DbgPrint("Error %d: CreatePalette failed.\n", GetLastError());
      return 1;
    }
    hdc = GetDC(NULL);
    SetSystemPaletteUse(hdc, SYSPAL_NOSTATIC256);
    SelectPalette(hdc, hPal, FALSE);
    RealizePalette(hdc);
    ReleaseDC(NULL, hdc);
    DeleteObject(hPal);
  }
  HeapFree(GetProcessHeap(), 0, pLogPal);
#endif

  x = 0;
  y = 0;
  dx = 1;
  dy = 1;
  while (TRUE) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT || (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)) {
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
    ((LPDWORD) data)[0] = y*SCREEN_PITCH + x*XPITCH;
    ((LPDWORD) data)[1] = SCREEN_PITCH;
    ((LPDWORD) data)[2] = BMP_WIDTH*XPITCH;
    ((LPDWORD) data)[3] = BMP_HEIGHT;
    success = WriteFile(hDVGA, data, sizeof(data), &written, NULL);
    if (success) {
      // DbgPrint("Wrote %d bytes to (%d, %d)\n", written, x, y);
    } else {
      DbgPrint("Error %d: Write failed\n", GetLastError());
    }
    Sleep(10);
  }
  CloseHandle(hDVGA);
  return 0;
}