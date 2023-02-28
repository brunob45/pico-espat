# Set-ExecutionPolicy -ExecutionPolicy Unrestricted -Scope Process

(new-Object System.IO.Ports.SerialPort COM3,1200,None,8,one).Open()
Start-Sleep -Milliseconds 500
Copy-Item .\build\usb2serial.uf2 D:\
