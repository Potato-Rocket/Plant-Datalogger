# Plant Datalogger

## Summary

An extensible datalogging project based on the Raspberry Pi Pico W. Currently implements the DHT11 temperature and humidity sensor, and an analog soil moisture sensor. Syncs the RTC using NTP upon startup and then every 24 hours.

Checks WiFi connection every hour, or before sending an NTP request. If disconnected, attempt reconnection. Note that reconnection protocol is blocking. If reconnection fails, the system makes repeated attempts with exponantial backoff. Upon a failed NTP request, the system again makes repeated attempts with exponential backoff, except during initialization. If the RTC or WiFi fails to initialize and connect properly during startup, the program will restart. The first NTP request in each sync event times out very quickly to avoid unecessary wait times resulting from dropped first packages.

Takes sensor readings every minute. If the DHT11 reading is unsuccessful, it will make up to ten repeated attempts before waiting for the next reading. Only records soil moisture upon successful DHT11 reading. Each soil moisture reading is averaged from 100 readings.

The soil sensor calibration sequence is entered upon startup. Recalibration can also be entered during runtime upon a long button press (3s-10s). The user will be first prompted for a dry reading (0%), then for a wet reading (100%). If the two readings are too similar, the user will be prompted to try again. The calibration will be stored in slope-intercept form, and future measurements will be mapped accordingly.

The red indicator LED varies behavior depending on the state of the dataloggers systems. Off means that everything is nominal. On but steady means that the soil is dry and watering is needed. Flashing at roughly 1Hz means that there is some error--either with the WiFi, the NTP sync, or the DHT11, which demands user attention. If the system is in a blocking startup state, or is recalibrating, the indicator will flicker at roughly 10Hz.
