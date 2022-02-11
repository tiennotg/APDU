# APDU
Example program to communicate with credit cards in NFC

__Disclaimer: This program is provided as an example of APDU protocol usage. I am not responsible for any misuse of it. Please use it only on your own credit card!__

# Build

  - First, compile and upload the `APDU_TERMINAL.ino` file on your Arduino board using the Arduino IDE.
  - Then, run the `make` command.
  - You should see an `APDU` file in the source directory. Plug your Arduino board with the NFC reader in your computer, and run the executable with one argument: the serial port of your board. For example: `./APDU /dev/ttyACM0`
  - Get your card close to the NFC reader. You should see your card number and expiration date on the screen.

Before running this program, make sure you have sufficient permissions to read and write the serial port of your board! If you have not, you can run the program as root: `sudo ./APDU /dev/ttyACM0`. Or, better, add yourself to the `uucp` group: `sudo usermod -aG uucp your_user_name`. Then, log out, and log in back.
