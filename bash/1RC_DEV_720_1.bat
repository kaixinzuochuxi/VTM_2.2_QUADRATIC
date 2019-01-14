@echo off & setlocal
::set /p date_create=date_create
::set seq_name=PartyScene_832x480_50

:: other sequence
::"Traffic_2560x1600_30_crop"
::"PeopleOnStreet_2560x1600_30_crop"
::"BQTerrace_1920x1080_60"
::"BasketballDrive_1920x1080_50"
::"Cactus_1920x1080_50"
::"Kimono1_1920x1080_24"
::"ParkScene_1920x1080_24"
::"FourPeople_1280x720_60"
::"Johnny_1280x720_60"
::"KristenAndSara_1280x720_60"
::"BQMall_832x480_60"
::"BasketballDrill_832x480_50"
::"PartyScene_832x480_50"
::"RaceHorses_832x480_30"
::"BQSquare_416x240_60"
::"BasketballPass_416x240_50"
::"BlowingBubbles_416x240_50"
::"RaceHorses_416x240_30"

::function 
::call:RC_266 RaceHorses_416x240_30 1000000 EncoderApp LDP
::call:RC_266 RaceHorses_416x240_30 1000000 EncoderApp LDP
::call:fix_QP_266 RaceHorses_416x240_30 22 EncoderApp LDP


@echo off

set date_t=20181113

::set encoder=EncoderApp_2.2_%date_t%
set encoder=EncoderApp
set encoding_mode=AI



start	.\RC_266_DEV.bat	Johnny_1280x720_60	22	%encoder%	%encoding_mode% "--LCULevelRateControl=0"
::start	.\RC_266_DEV.bat	Johnny_1280x720_60	27	%encoder%	%encoding_mode% "--LCULevelRateControl=0"
::start	.\RC_266_DEV.bat	Johnny_1280x720_60	32	%encoder%	%encoding_mode% "--LCULevelRateControl=0"
::start	.\RC_266_DEV.bat	Johnny_1280x720_60	37	%encoder%	%encoding_mode% "--LCULevelRateControl=0"












