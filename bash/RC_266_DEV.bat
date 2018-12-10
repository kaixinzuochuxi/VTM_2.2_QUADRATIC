
@echo off



::take 4 parameters, 
::first is the seq name like PartyScene_832x480_50, 
::second is bitrate, 
::third is encoder.exe name, 
::fourth is the mode

SETLOCAL
set seq_name=%1
set bitrate=%2
set encoder=%3
set coding_structure=%4

for /f "delims=_  tokens=1" %%a in ("%seq_name%") do (
   set seq=%%a
   )

for /f "delims=_  tokens=2" %%a in ("%seq_name%") do (
   set resolution=%%a
   )
   
for /f "delims=x  tokens=1" %%a in ("%resolution%") do (
   set width=%%a
   )
for /f "delims=x  tokens=2" %%a in ("%resolution%") do (
   set height=%%a
   )
   
for /f "delims=_  tokens=3" %%a in ("%seq_name%") do (
	for /f "delims=fps  tokens=1" %%b in ("%%a") do (
	set frame_rate=%%b
	)
   )

  
if "%4"=="AI" set cfg_file=encoder_intra_vtm
if "%4"=="RA" set cfg_file=encoder_randomaccess_vtm
if "%4"=="LDB" set cfg_file=encoder_lowdelay_vtm
if "%4"=="LDP" set cfg_file=encoder_lowdelay_P_vtm
  
echo seq:%seq%
echo resolution:%resolution%
echo width:%width%
echo height:%height%
echo frame_rate:%frame_rate%
echo bitrate:%bitrate%
echo encoder:%encoder% 
echo coding_structure:%coding_structure%

set configuration=--PrintFrameMSE=1 -ts 1 -wdt %width% -hgt %height% -fr %frame_rate% -f %frame_rate%0 
set RC_configuration=--RateControl=1 --TargetBitrate=%bitrate%
echo %configuration% %RC_configuration% %5
::set configuration=-f 50 --PrintFrameMSE=1 -ts 1 --RateControl=1 --TargetBitrate=44449334.4 --LCULevelRateControl=0 --RCCpbSaturation=0 |

..\bin\%encoder%.exe -c D:\Projects\data\cfg_VTM\%cfg_file%.cfg -c D:\Projects\data\cfg_VTM\per-sequence\PartyScene.cfg -i D:\Projects\data\test_seq\HEVC\%seq_name%.yuv %configuration% %RC_configuration% %5


ENDLOCAL