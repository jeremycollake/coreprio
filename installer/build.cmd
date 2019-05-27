echo Building installer
"c:\dev\nsis\bin\makensis" coreprio.nsi

rem sign installers
echo Signing ... 2=%2 3=%3
if exist "..\x64\release\corepriosetup.exe" (			
	call bitsum-sign "..\x64\release\corepriosetup.exe"
	)