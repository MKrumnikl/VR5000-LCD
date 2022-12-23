# VR5000 LCD Display Replacement
Older Yaesu VR-5000 LCD displays often suffer from loss of vertical LCD lines. Since the original displays are not available on eBay or other sources, we have to create another replacement with available parts. Inspired by *kakiralime* [Youtube video](https://www.youtube.com/watch?v=UIxD9E-iZ5w) I have implemented similar solution. Unfortunately I was unable to contact the author, so I had to re-implement it myself. The difference is in the use of the SPI connection to the TFT display compared to the parallel controlled IPS. The advantage is simpler wiring, but at the cost of slower image transmission on the TFT.

## Hardware
- STM32F407 (on MCUDev black STM32F407VGT6 mini dev board)
- SPI TFT 320x480 with ILI9488 

# TODO
- Missing some symbols/icons
- Missing decoding of seven-segment displays (~~step~~, bank)
- Speed improvement

Binaries are available in __/build__ directory.

## Construction

MCUDev Black STM32F407VET6 is connected with SPI TFT 320x480 with ILI9488 via J4 pins. Yaesu LCD signals are going to PORTD (D0-D7) to (PD0-PD7), and PD8 - 10 is connected to RD, WR, A and CS signal. Only WR and A is currently used by the firmware. Due to tight timing these signals are also connected to PA0 and PA2, triggering `EXTI0_IRQHandler` and `EXTI2_IRQHandler` interrupt handlers. 

![LCD with STM32F407](img/conn1.jpg?raw=true "Construction display")
![PCB of VR5000](img/conn2.jpg?raw=true "Construction board")

## Demo Screen
More photos can be found in __/img__ directory.

![LCD Screen](img/vr5000-1.jpg?raw=true "Screen1")
