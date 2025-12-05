@echo off
setlocal enabledelayedexpansion

set "expanded_list="
pushd src
for /f "delims=" %%f in ('dir /b /s *.cpp') do (
    set "expanded_list=!expanded_list! "%%f""
)
popd

@echo on
"C:\Program Files\Poedit\GettextTools\bin\xgettext.exe" %expanded_list% ^
  --keyword="_gt" --keyword="gettext" --keyword="_lc" --keyword="lc_format" --keyword="throw_trouble" --keyword="throw_failed" ^
  --keyword="notify_info" --keyword="notify_warn" --keyword="notify_error" ^
  --output="config/locales/EDRobot.pot"

endlocal