gcc -static-libgcc -shared -Wl,--enable-stdcall-fixup -O1 -s -Wall -o winmm.dll main.c passthrough.c wrapper.c wrapper.def