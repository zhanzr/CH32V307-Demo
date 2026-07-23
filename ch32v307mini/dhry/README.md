# CH32V307 Evt board test.

## dhrystone
Execution starts, 5000000 runs through Dhrystone
144 MHz, Nano Lib

### Flash cached
GCC 15.2.0
```
-Ofast
MicroSecond for one run through Dhrystone[19-6034]:      3.008
Dhrystones per Second:  332502.094
DMIPS/MHz:      1.314
```

-O3
```
MicroSecond for one run through Dhrystone[19-5951]:      2.966
Dhrystones per Second:  337154.406
DMIPS/MHz:      1.333
```
