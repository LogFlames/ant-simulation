@echo off

:: FOR %%m IN (.\res\textures\testmap_no_walls_1024x512.png .\res\textures\testmap_simplemaze_1024x512.png .\res\textures\testmap_split_path_1024x512.png .\res\textures\testmap_multifood_1024x512.png) DO (
FOR %%m IN (.\res\textures\testmap_split_path_1024x512.png .\res\textures\testmap_multifood_1024x512.png) DO (
	FOR %%a IN (2 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768) DO (
		ECHO Calculating map %%m
		FOR /L %%n IN (1, 1, 40) DO ( 
			ECHO     Running %%n
			"..\\bin\\Win32\\Debug\\Ant Simulation.exe" %%m %%a %%n
		)
	)
)
PAUSE