del /F /Q *.aps
del /F /Q *.ncb
del /F /AH /Q *.suo
del /F /Q *.user
del /F /Q *.sdf


del /F /Q *.ncb
del /F /Q *.ncb
del /F /Q *.ncb

rd /S /Q Dll
rd /S /Q ipch
rd /S /Q Lib
rd /S /Q Outdir

rd /S /Q Debug
rd /S /Q Release

rd /S /Q _ReSharper.JRSMemory_VC2012

call :treeProcess
goto :eof

:treeProcess
rd /S /Q Debug
rd /S /Q Release

for /D %%d in (*) do (
    cd %%d
    call :treeProcess
    cd ..
)