# WimsConstructionCam
Raspberry Pi program to take pictures between sunrise and sunset and stitch them together into a timelapse video

Runs standard Raspberry Pi camera programs raspistill or libcamera-still with appropriate parameters to take a picture a minute between sunrise and sunset, then optionaly runs ffmpeg to stitch the accumulated still images into a daily and monthly time lapse video.

Calculates sunrise and sunset times based on GPS coordintes. Can take longitude and latitude as parameters or connect to gpsd to request the current gps coordinates. 

If gpsd coordinates are aquired from the gps, will attempt to set the system clock if the results differ by more than an hour. This is useful when running in a remote location without reliable internet access because the raspberry pi has no real time clock and will lose time if it loses power.

If multiple destinations for picture storage are specified, movies are created in the auxilary locations. Useful for recieving pictures from underpowered remote machines.

Runs a daily process of deleting old images to free up an appropriate amount of space for new images. Attempts to delete the oldest images until the specified amount of fee space is created. The free space size is specified by command line parameter. Only the first destination is processed in the free space routine.

The jpeg images stored are the default sensor resolution. 

If one ore more --size options is provided, ffmpeg is run daily to create a daily movie of the stills captured. If no --size option is given, no video file is created. This is useful on the Pi ZeroW where runnning ffmpeg appears to overload the processor.

If daily movies are created, ffmpeg is run afterwards to concatenate daily movies into a monthly movie. 

```
Usage: /usr/local/bin/wimsconstructioncam [options]
  WimsConstructionCam 1.20230327-1 Built Mar 27 2023 at 15:30:04
  Options:
    -h | --help          Print this message
    -v | --verbose level stdout verbosity level [1]
    -d | --destination location pictures will be stored []
    -f | --freespace gigabytes free space per day [3]
    -t | --time minutes of stills to capture [0]
    -l | --lat latitude for sunrise/sunset [0]
    -L | --lon longitude for sunrise/sunset [0]
    -G | --gps prefer gpsd lat/lon, if available, to command line
    -s | --size video size. Valid options are 1080p and 2160p. Option may be repeated.
    -n | --name Text to display on the bottom right of the video
    -R | --runonce Run a single capture session and exit
    -r | --rotate rotate all still pictures 180 degrees if camera is upside down
    -H | --hdr run hdr image processing on all captured images
    -2 | --24hour capture images around the clock. HDR mode before sunrise, normal during daylight, HDR mode after sunset
    -T | --tuning-file camera module tuning file
```

## To build on a fresh Raspian platform:
```
sudo apt install -y build-essential git cmake libgps-dev libgd-dev libexif-dev 
git clone https://github.com/wcbonner/WimsConstructionCam.git
cmake -S WimsConstructionCam -B WimsConstructionCam/build
cmake --build WimsConstructionCam/build
pushd WimsConstructionCam/build && cpack . && popd
```
