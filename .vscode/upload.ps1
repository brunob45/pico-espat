picotool reboot -u -f
Start-Sleep -Milliseconds 500
picotool load -x build/pico-efi.uf2
Write-Output "Success"