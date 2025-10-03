# VGP (GPIO CLI/GUI utility for Vivid Unit)

VGP comes with a CLI utility named "vgp" and a GUI tool named "vgpw". You can use it to check/control the status of GPIO pins, or monitor the ADC pins on Vivid Unit.

vgp is a command line tool that works similar to the "gpio" utility included in wiringPi library. You can find out how to use it [here](https://www.vividunit.com/gpio-and-adc#VGP_Utility).

vgpw provides an intuitive GUI, and it looks like this:

<img src="https://www.vividunit.com/images/f/f8/Vgp-1024x391.png" width="700"></img>

You can click the mode button for each pin to change its mode or alternative function. The possilbe mode/funtion could be IN, OUT, ALT1, ALT2 or ALT3. When the mode is set to OUT, you may click the value button to toggle the pin’s value. When the mode is set to IN, its value will get updated automatically when the pin is pulled up or down.

The value of three ADC channels are also listed at the bottom, including the raw values (0~1023) and the voltage values. These values refresh for every second.

There is a “Flip” button at the bottom right corner, and you can click it to flip the view.

## How to Build
```
cd src
make
```

