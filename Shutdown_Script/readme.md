# Shutdown Listener

A Python program that listens for special UDP packets to remotely shut down the computer.  
Supports hidden background execution and can be converted into a Windows executable (.exe).

## 🚀 Running the program as a Windows Service (pre-login)

To have the executable run in the background even before any user logs in, you can register it as a **Windows Service** using **NSSM (Non-Sucking Service Manager)**.

1. Download NSSM from: [https://nssm.cc/download](https://nssm.cc/download)  
2. Extract the files to a folder, for example: ```C:\nssm```
3. Install the service
4. Open Command Prompt as Administrator and run: ```nssm install ShutdownListener```
5. In the window that opens, configure:
6. Path: full path to your executable, e.g., ```C:\Path\To\YourProgram.exe```
7. Startup directory: folder containing the exe
8. Service name: ShutdownListener (or another name of your choice)
9. Click Install service.
10. Start the service In CMD, run: ```nssm start ShutdownListener```

✅ Now the program will run automatically in the background, even before login, ready to listen for the magic packet to shut down the PC.

## 💻 Creating the program executable (.exe)

To run the listener without opening Python, you can generate a **Windows executable** using **PyInstaller**.  
This allows you to run the program with **double-click** in hidden background mode.

1. Install dependencies

2. Make sure PyInstaller is installed, Open Command Prompt as Administrator and run: ```pip install pyinstaller getmac```
3. Convert the script to .exe, open PowerShell or Command Prompt and run: ```C:\PYTHONPATH\3.11.2\Scripts\pyinstaller.exe --onefile --noconsole --icon=C:\Path\To\Icon\icon.ico C:\Path\To\Script\Run_script-shutdown_win.py --onefile``` → generates a single .exe file
   - ```--noconsole``` → runs hidden, without opening the black console window
   - ```--icon``` → sets the executable icon (replace with the path to your .ico)
4. The executable will be created in the folder: ```dist\Run_script-shutdown_win.exe```
5. Run the program, double-click the .exe and it will remain continuously listening in the background.

To test without shutting down the PC, run with the --simulate argument: ```Run_script-shutdown_win.exe --simulate```

✅ The listener will continue monitoring UDP packets but will not execute the shutdown command.