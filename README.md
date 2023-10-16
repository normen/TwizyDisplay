## Twizy ESP Composite Display

A power display for the Renault Twizy based on the ESP32, using composite video for the display. It uses a MCP2515 board to connect to the CAN bus in the Twizy.

![Twizy Display Image](/doc/display.jpg?raw=true "Twizy ESP Composite Display")

### What

I created this because the [rear-view camera](https://www.amazon.de/gp/product/B01KUNL3A2) that I installed on my Twizy has a second video input on the display that is enabled whenever the camera is not in use. So I decided to repurpose that input and create a power display for the Twizy.

### How

##### Needed Parts

- An [ESP32 Board](https://www.amazon.de/AZDelivery-NodeMCU-Development-Nachfolgermodell-ESP8266/dp/B071P98VTG)
- A [MCP2515 CAN Bus Shield](https://www.amazon.de/AZDelivery-MCP2515-Shield-kompatibel-Arduino/dp/B086TXSFD8)
- An [OBD2 Plug](https://www.amazon.de/OBD2-Stecker-Gehäuse-mit-Platine/dp/B01HMKWE7C)
- A [Rear-View Camera](https://www.amazon.de/gp/product/B01KUNL3A2) display with composite video input

##### Installation

- Flash the software to an [ESP32 Board](https://www.amazon.de/AZDelivery-NodeMCU-Development-Nachfolgermodell-ESP8266/dp/B071P98VTG)
- Connect `GPIO 25` of the ESP board to the video input of the display (as well as GND). 
- Connect the MCP2515 board like this:

![ESP32-MCP2515](/doc/connections.jpg?raw=true "ESP32-MCP2515 Connection")

- Connect the `H` and `L` outputs of the MCP board to the pins `6` and `14` of the OBD2 plug:

![OBD2 Plug](/doc/plug.jpg?war=true "OBD2 Plug Pins")

- Optionally connect a button between GPIO 27 and GND to switch the display mode (see below).

##### Power

If you have the sound system installed on the Twizy you should have a USB port that you can power the board from, otherwise you will need a 12V to USB adapter or use a buck converter to get 5V from the 12V line of the OBD2 port.

### Usage

Usage is pretty self-explanatory, the display just displays the given info.

You can attach a switch to cycle the display mode to `GPIO 27` of the ESP32 board (see above).

- Mode 1: Display power usage / recuperation as well as battery and motor temperature
- Mode 2: Additionally shows a large kph display
- Mode 3: Additionally shows a large rpm display
- Mode 4: Additionally displays the max/min power usage
- Mode 5: Additionally displays the charger and inverter temperature

I added the speed display as I am usually putting my phone in front of the original Twizy screen when I am using the navigation on my phone.

The set mode will be stored in EEPROM so on the next boot it will start in the mode you selected.

### Hints

##### MCP2515 board

If you have issues talking to the CAN board, check if the frequency of your MPC2515 board is 8MHz (written on the silver quartz) - don't worry if its not, you only need to change one line in the code, search for `8MHZ` and change it to `16MHZ`.

The MCP2515 board is 5V but you don't need to worry, the ESP32 is 5V tolerant on the inputs.

##### Resources

Generally, if you have display issues, check the [ESP_8BIT_composite Project](https://github.com/Roger-random/ESP_8_BIT_composite) if your issue came up already.

### Thanks

Thanks go to bishibashi for his [Twizy CAN Display](https://github.com/bishibashiB/Twizy_CanDisplay) project, which helped a lot to create this software.
