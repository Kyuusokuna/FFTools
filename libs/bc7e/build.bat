@echo off
ispc -O2 "bc7e.ispc" -o "$bc7e.obj" -h "$bc7e_ispc.h" --target=sse2,sse4,avx,avx2 --opt=disable-assertions