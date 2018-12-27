
@echo off



::take 4 parameters, 
::first is the seq name like PartyScene_832x480_50, 
::second is bitrate, 
::third is encoder.exe name, 
::fourth is the mode

SETLOCAL
set seq_name=%1
set fix_QP=%2

if %1==BasketballDrill_832x480_50	if %2==22	echo	18504608.8
if %1==BasketballDrill_832x480_50	if %2==27	echo	9908970.4
if %1==BasketballDrill_832x480_50	if %2==32	echo	5332497.6
if %1==BasketballDrill_832x480_50	if %2==37	echo	2946748.8
if %1==BasketballDrive_1920x1080_50	if %2==22	echo	64028779.2
if %1==BasketballDrive_1920x1080_50	if %2==27	echo	26212308
if %1==BasketballDrive_1920x1080_50	if %2==32	echo	13441606.4
if %1==BasketballDrive_1920x1080_50	if %2==37	echo	7492284
if %1==BasketballPass_416x240_50	if %2==22	echo	4775440
if %1==BasketballPass_416x240_50	if %2==27	echo	2862928
if %1==BasketballPass_416x240_50	if %2==32	echo	1657066.4
if %1==BasketballPass_416x240_50	if %2==37	echo	924892
if %1==BlowingBubbles_416x240_50	if %2==22	echo	10666775.2
if %1==BlowingBubbles_416x240_50	if %2==27	echo	6616793.6
if %1==BlowingBubbles_416x240_50	if %2==32	echo	3782460.8
if %1==BlowingBubbles_416x240_50	if %2==37	echo	1923687.2
if %1==BQMall_832x480_60	if %2==22	echo	21038757.6
if %1==BQMall_832x480_60	if %2==27	echo	12617112
if %1==BQMall_832x480_60	if %2==32	echo	7427436
if %1==BQMall_832x480_60	if %2==37	echo	4223824.8
if %1==BQSquare_416x240_60	if %2==22	echo	12339100
if %1==BQSquare_416x240_60	if %2==27	echo	7898929.6
if %1==BQSquare_416x240_60	if %2==32	echo	4869597.6
if %1==BQSquare_416x240_60	if %2==37	echo	2876344
if %1==BQTerrace_1920x1080_60	if %2==22	echo	170591560.8
if %1==BQTerrace_1920x1080_60	if %2==27	echo	73123200
if %1==BQTerrace_1920x1080_60	if %2==32	echo	36711484
if %1==BQTerrace_1920x1080_60	if %2==37	echo	19550096
if %1==Cactus_1920x1080_50	if %2==22	echo	100731320
if %1==Cactus_1920x1080_50	if %2==27	echo	43989674.4
if %1==Cactus_1920x1080_50	if %2==32	echo	23781052.8
if %1==Cactus_1920x1080_50	if %2==37	echo	12825628.8
if %1==FourPeople_1280x720_60	if %2==22	echo	26769896.8
if %1==FourPeople_1280x720_60	if %2==27	echo	16444881.6
if %1==FourPeople_1280x720_60	if %2==32	echo	10162103.2
if %1==FourPeople_1280x720_60	if %2==37	echo	6144760.8
if %1==Johnny_1280x720_60	if %2==22	echo	18064174.4
if %1==Johnny_1280x720_60	if %2==27	echo	10055632
if %1==Johnny_1280x720_60	if %2==32	echo	5723545.6
if %1==Johnny_1280x720_60	if %2==37	echo	3289476
if %1==Kimono1_1920x1080_24	if %2==22	echo	19760437.6
if %1==Kimono1_1920x1080_24	if %2==27	echo	10770244
if %1==Kimono1_1920x1080_24	if %2==32	echo	6101960.8
if %1==Kimono1_1920x1080_24	if %2==37	echo	3415300.8
if %1==KristenAndSara_1280x720_60	if %2==22	echo	19759017.6
if %1==KristenAndSara_1280x720_60	if %2==27	echo	11883180
if %1==KristenAndSara_1280x720_60	if %2==32	echo	7201027.2
if %1==KristenAndSara_1280x720_60	if %2==37	echo	4363652.8
if %1==ParkScene_1920x1080_24	if %2==22	echo	47892453.6
if %1==ParkScene_1920x1080_24	if %2==27	echo	25923807.2
if %1==ParkScene_1920x1080_24	if %2==32	echo	13519676.8
if %1==ParkScene_1920x1080_24	if %2==37	echo	6667985.6
if %1==PartyScene_832x480_50	if %2==22	echo	40692235.2
if %1==PartyScene_832x480_50	if %2==27	echo	25200196.8
if %1==PartyScene_832x480_50	if %2==32	echo	15075630.4
if %1==PartyScene_832x480_50	if %2==37	echo	8095344.8
if %1==RaceHorses_416x240_30	if %2==22	echo	4102340
if %1==RaceHorses_416x240_30	if %2==27	echo	2490414.4
if %1==RaceHorses_416x240_30	if %2==32	echo	1387984.8
if %1==RaceHorses_416x240_30	if %2==37	echo	689338.4
if %1==RaceHorses_832x480_30	if %2==22	echo	13836398.4
if %1==RaceHorses_832x480_30	if %2==27	echo	8289692.8
if %1==RaceHorses_832x480_30	if %2==32	echo	4775920.8
if %1==RaceHorses_832x480_30	if %2==37	echo	2424324.8



ENDLOCAL