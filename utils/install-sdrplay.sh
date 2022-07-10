#!/bin/sh
# quick script to install SDRplay API/HW driver

set -e

SDRPLAY_SOFTWARE_DOWNLOADS='https://www.sdrplay.com/software'
SDRPLAY_API_FILE_LINUX='SDRplay_RSP_API-Linux-3.07.1.run'
SDRPLAY_API_FILE_MACOS='SDRplay_RSP_API-MacOSX-3.07.3.pkg'

case "$(uname -s)" in
  Linux) install_file="/tmp/$SDRPLAY_API_FILE_LINUX"
         curl -s -S -o "$install_file" "$SDRPLAY_SOFTWARE_DOWNLOADS/$SDRPLAY_API_FILE_LINUX"
         sudo sh "$install_file"
         rm "$install_file"
         ;;
  Darwin) install_file="/tmp/$SDRPLAY_API_FILE_MACOS"
          curl -s -S -o "$install_file" "$SDRPLAY_SOFTWARE_DOWNLOADS/$SDRPLAY_API_FILE_MACOS"
          sudo installer -pkg "$install_file" -target /
          rm "$install_file"
          ;;
  *) echo "unknown OS: $(uname -s)" 1>&2; exit 1
     ;;
esac

echo "SDRplay API/HW driver installed successfully" 1>&2

exit 0
