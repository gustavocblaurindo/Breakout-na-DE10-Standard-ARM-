@echo off
setlocal

set ARMTOOLS=C:\intelFPGA_lite\23.1std\University_Program\Monitor_Program\arm_tools\baremetal
set CC=%ARMTOOLS%\arm-altera-eabi\bin\gcc.exe
set OC=%ARMTOOLS%\arm-altera-eabi\bin\objcopy.exe
set GCC_EXEC_PREFIX=%ARMTOOLS%\libexec\gcc\
set LIBGCC_DIR=C:/intelFPGA_lite/23.1std/University_Program/Monitor_Program/arm_tools/baremetal/lib/gcc/arm-altera-eabi/4.7.3
set B_FLAG=-B%LIBGCC_DIR%/
set L_FLAG=-L%LIBGCC_DIR%/
set LD_SCRIPT=C:\intelFPGA_lite\23.1std\University_Program\Monitor_Program\build\altera-socfpga-hosted.ld
set CCFLAGS=-Wall -c -g -O1 -std=c99 -mfloat-abi=soft -march=armv7-a -mtune=cortex-a9 -mcpu=cortex-a9

echo ============================================
echo  Compilando main.c (ARM Cortex-A9)
echo ============================================

echo [1/3] Compilando main.c ...
"%CC%" %CCFLAGS% main.c -o main.c.o
if %ERRORLEVEL% NEQ 0 (
    echo ERRO: falha na compilacao!
    goto :erro
)

echo [2/3] Linkando main.axf ...
"%CC%" %B_FLAG% %L_FLAG% main.c.o ^
  -Wl,--defsym,arm_program_mem=0x0 ^
  -Wl,--defsym,arm_available_mem_size=0x3ffffff8 ^
  -Wl,--defsym,__cs3_stack=0x3ffffff8 ^
  -T"%LD_SCRIPT%" ^
  -o main.axf
if %ERRORLEVEL% NEQ 0 (
    echo ERRO: falha na linkagem!
    goto :erro
)

echo [3/3] Gerando main.srec ...
"%OC%" -O srec main.axf main.srec
if %ERRORLEVEL% NEQ 0 (
    echo ERRO: falha ao gerar srec!
    goto :erro
)

echo.
echo ==============================================
echo  COMPILACAO CONCLUIDA! Agora va no Monitor
echo  Program e clique em Load apontando para main.srec
echo ==============================================
goto :fim

:erro
echo ==============================================
echo  ERRO. Verifique as mensagens acima.
echo ==============================================

:fim
endlocal
pause