@echo off
setlocal EnableDelayedExpansion

set TargetDir=%1
set FilesAll=

for /r %TargetDir% %%f in (*.h *.cpp) do (
  set FilesAll=!FilesAll! %%f
)

python.exe -m cpplint %FilesAll%