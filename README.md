# Smart Wand Updates
#### Client Name: Brendan Rose

## Setup Instructions
1. Download & install KEIL IDE from [download link](https://www.keil.com/demo/eval/arm.htm).
2. Open your project, select target `DA1453` then go to `Projet -> Options for Target DA14531 -> C/C++ (AC6)`, uncheck the LTO (Link-Time Optimization) if enabled.
3. Build project using `F7`.
4. Start running the program using `CTRL + F7`.
5. To start `Run`, press `F5`.
6. Now, you can connect & receive notifications on characteristic UUID `15005991-b131-3396-014c-664c9867b917`. 
**Note: Enable the notifications from the app. Set MTU size to 70.**

## Flashing Guide
1. Open JlinkCommander.
2. Type "connect" and hit enter.
3. If `DA14531` is already written, then press enter. Otherwise type `?` and hit Enter. Then select the MCU `DA14531.
4. Type `s` for SWD and hit enter.
5. Select the default by pressing Enter.
6. Go to binary file. Hold `Shift` key + Right Click. Select `Copy as Path`.
7. Enter this command in jlink window `loadfile "Paste the path here"`. Hit Enter.
8. Type `go` and hit enter.
