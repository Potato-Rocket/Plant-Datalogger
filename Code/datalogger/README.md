# Plant Datalogger

## Summary

An extensible datalogging project based on the Raspberry Pi Pico W. Currently implements the DHT11 temperature and humidity sensor, and an analog soil moisture sensor. Syncs the RTC using NTP upon startup and then every 24 hours.

Checks WiFi connection every ten seconds. If disconnected, attempt reconnection. Note that reconnection protocol is blocking.If reconnection fails, make repeat attempts with exponantial backoff. Upon a failed NTP request, make repeated attempts with exponential backoff, except during initialization. First NTP request times out very quickly to avoid unecessary wait times resulting from dropped first packages.

Takes a sensor readings every minute. If the DHT11 reading is unsuccessful, make up to ten repeated attempts before waiting for the next reading. Only records soil moisture upon successfyl DHT11 reading. Each soil moisture reading is averaged from 100 readings.

Soil sensor calibration sequence is entered upon startup. Recalibration can occur upon a long button press. The user will be first prompted for a dry reading (0%), then for a wet reading (100%). The calibration will be stored in slope-intercept form, and future measurements will be mapped accordingly.

## To Do

- Tie behavior of red LED to soil sensor as well as error handling.
- Cache calibration of soil sensor in EEPROM.
- Implement data logging to SD card.
- Tie behavior of green LED to SD card status.
- Cache data measurements to EEPROM if SD card unavailable, then flush to SD card when it becomes accessible.
- Implement UI on mini OLED.
- Begin planning and designing PCB with female headers for Pico, OLED, SD reader, and soil moisture amplifier.
