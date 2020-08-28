# Twigs

Twigs is an alternate firmware for the [Mutable Instruments Branches](http://mutable-instruments.net/modules/branches) Eurorack synthesizer module

Twigs consists of two functions

* **VC Factor** - combination clock/trigger divider & multiplier
* **VC Swing** - musical swing applied to clock/trigger

These functions can be assigned by the user to either or both channels on the module

## Usage
#### Shared Input/Reset

![twigs alt firmware, diagram of inputs](http://i.imgur.com/honssyb.png)

There is one major difference between the layout of Twigs and stock Branches: the channels don't have individual inputs.  

As pictured, top input (**1**) is for reset and the bottom input (**2**) is for trig/clock. *These inputs are shared by both channels*

### Functions

#### VC Factor

VC Factor is a combination clock/trigger divider and multiplier

###### Factor Control

![twigs alt firmware, diagram of factor control](http://i.imgur.com/hEDyddZ.png)

The channel's knob controls the *factor*, which determines whether the input is divided or multiplied and by how much

As illustrated above, turning the knob to the left produces increasing factors of multiplication, and to the right, division

When the knob is at the *1* position, the effect is bypassed the output will be the same as the input

###### Other Controls

![twigs alt firmware, diagram of factor channel](http://i.imgur.com/rmMf5k4.png)

The VC input (**2**) controls the factor in the same manner as the knob

While dividing, tapping the button (**B**) performs a manual reset.  This results in the next input trig being a strike and the counter starting over. This is the same behavior as sending a pulse to the reset input

Both outputs (**1**) produce the same result

#### VC Swing

VC Swing modifies incoming triggers/clock to have musical swing

###### Controls

![twigs alt firmware, diagram of swing channel](http://i.imgur.com/rmMf5k4.png)

The knob (**A**) controls the swing amount with a range of 50% - 70%.

At 50%, the swing effect is essentially bypassed. However, as the knob turns clockwise, every other input trig is delayed by the specified percentage

The VC input (**2**) controls the swing amount in the same manner as the knob

Tapping the button (**B**) performs a manual reset. For the swing function, this means that the next input trig will be the non-swing one.  This is the same behavior as sending a pulse to the reset input

Both outputs (**1**) produce the same result

### Select a Function

By default, Twigs has VC Swing in the top channel and VC Factor in the bottom

Holding the channel's button for a couple of seconds will toggle the function of that channel.  The current functions of the channel are stored and will remain when the module is powered up again

## Video

Here is a short video that gives an overview of the functionality and usage

[![twigs alt firmware, demo video](http://img.youtube.com/vi/lWKPzUoxJjY/0.jpg?1)](http://www.youtube.com/watch?v=lWKPzUoxJjY)

## Installation

Download the latest stable source & dependencies package [here](https://github.com/arirusso/twigs/blob/master/releases/twigs-1.0.2.tar.gz?raw=true)

Mutable Instruments' guide to installing open source firmware on Branches can be found [here](http://mutable-instruments.net/modules/branches/open) under "Sending the firmware to the module"

My notes for uploading Twigs using OSX and an interface I purchased on Amazon are [here](https://gist.github.com/arirusso/9d55c77618bd1195a9fc238ffac47f18)

Also my notes on similarly uploading the stock Branches firmware are [here](https://gist.github.com/arirusso/88e5f4d04e99e3fdf8914225cea74581) in case it's helpful

## Credit

Although heavily modified, Twigs is based on the stock MI Branches firmware.  That project can be [found in the MI Eurorack repository](https://github.com/pichenettes/eurorack) and is copyright 2012 Emilie Gillet, licensed GPL3.0

## Author

* [Ari Russo](http://github.com/arirusso) <ari.russo at gmail.com>

## License

GPL3.0, See the file LICENSE

Copyright (c) 2016 [Ari Russo](http://arirusso.com)
