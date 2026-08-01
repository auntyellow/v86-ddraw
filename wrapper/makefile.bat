cl /DNDEBUG /DRA95_FLIP /O2 /W3 /MD /LD /Feddraw.dll ddraw_wrapper.c dxguid.lib user32.lib ddraw.def
del ddraw_wrapper.obj
del ddraw.exp
del ddraw.lib