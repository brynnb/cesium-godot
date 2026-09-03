@echo off
python tools\build_extension.py --platform windows --arch x64 --jobs 4 %*
