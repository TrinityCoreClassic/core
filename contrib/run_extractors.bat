@echo off
setlocal

rem Detect number of logical processors
for /f "tokens=2 delims==" %%A in ('wmic cpu get NumberOfLogicalProcessors /value') do set THREADS=%%A

echo Detected %THREADS% threads.

echo Starting map extraction...
mapextractor.exe

echo Starting vmap extraction...
vmap4extractor.exe

echo Assembling vmaps...
vmap4assembler.exe

echo Generating mmaps with debug enabled...
mmaps_generator.exe --allowDebug --threads %THREADS%

echo mmaps generation completed successfully!
echo All done!

endlocal
