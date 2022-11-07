# WimsConstructionCam
Raspberry Pi program to take pictures between sunrise and sunset and stitch them together into a timelapse video

Runs standard Raspberry Pi camera programs raspistill or libcamera-still with appropriate parameters to take a picture a minute between sunrise and sunset, then runs ffmpeg to stitch the accumulated still images into a daily time lapse video.

Calculates sunrise and sunset times based on GPS coordintes. Can take longitude and latitude as parameters or connect to gpsd to request the current gps coordinates. 

If gpsd coordinates are aquired from the gps, will attempt to set the system clock if the results differ by more than an hour. This is useful when running in a remote location without reliable internet access because the raspberry pi has no real time clock and will lose time if it loses power.

Runs a daily process of deleting old images to free up an appropriate amount of space for new images. Attempts to delete the oldest images until the specified amount of fee space is created. The free space size is specified by command line parameter.

Can either crop still pictures to 1920x1080 to reduce still image storage space, or store still images at full resolution and let ffmpeg crop images when it converts to a 1920x1080p30fps video. The latter produces a higher quality video and may have a wider field of view depending on the image sensor used on the Raspberry Pi.

```
Usage: /usr/local/bin/wimsconstructioncam [options]
  WimsConstructionCam 1.20221103-3 Built Nov  3 2022 at 20:36:31
  Options:
    -h | --help          Print this message
    -v | --verbose level stdout verbosity level [1]
    -d | --destination location pictures will be stored []
    -f | --freespace gigabytes free space per day [3]
    -t | --time minutes of stills to capture [0]
    -l | --lat latitude for sunrise/sunset [0]
    -L | --lon longitude for sunrise/sunset [0]
    -G | --gps prefer gpsd lat/lon, if available, to command line
    -n | --name Text to display on the bottom right of the video
    -R | --runonce Run a single capture session and exit
    -r | --rotate rotate all still pictures 180 degrees if camera is upside down
    -F | --fullsensor use the default camera size for still capture
    -T | --tuning-file camera module tuning file
```
