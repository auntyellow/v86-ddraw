cl /O2 /W3 /MD /DDDS_FLIP testdd.c ddraw.lib user32.lib
del testdd.obj
rem cl /O2 /W3 /MD dbgview.c
rem del dbgview.obj