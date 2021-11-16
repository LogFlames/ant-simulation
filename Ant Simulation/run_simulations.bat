@echo off
::FOR %%x IN (1 2 3) DO ECHO %%x
::    FOR /L %%n IN (1, 1, 10) DO 
::        ECHO %%y
::    PAUSE
::PAUSE

Set map=.\res\textures\testmap_no_walls_1024x512.png

FOR /L %%n IN (1, 1, 10) DO ( 
    ECHO Running %%n
    "..\\bin\\Win32\\Debug\\Ant Simulation.exe" %map% 128 %%n
)
PAUSE