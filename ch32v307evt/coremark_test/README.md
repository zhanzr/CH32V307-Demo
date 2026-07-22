# CH32V307 Evt board test.

## dhrystone
Execution starts, 5000000 runs through Dhrystone
144 MHz, Nano Lib

### Flash cached
GCC 15.2.0
```
-Ofast
2K performance run parameters for coremark.
CoreMark Size    : 666
Total ticks      : 10101
Total time (secs): 10.101000
Iterations/Sec   : 396.000396
Iterations       : 4000
Compiler version : GCC15.2.0
Compiler flags   : -Ofast
Memory location  : Static
seedcrc          : 0xe9f5
[0]crclist       : 0xe714
[0]crcmatrix     : 0x1fd7
[0]crcstate      : 0x8e3a
[0]crcfinal      : 0x65c5
Correct operation validated. See readme.txt for run and reporting rules.
CoreMark 1.0 : 396.000396 / GCC15.2.0 -Ofast / Static
```

-O3
```
2K performance run parameters for coremark.
CoreMark Size    : 666
Total ticks      : 10102
Total time (secs): 10.102000
Iterations/Sec   : 395.961196
Iterations       : 4000
Compiler version : GCC15.2.0
Compiler flags   : -O3
Memory location  : Static
seedcrc          : 0xe9f5
[0]crclist       : 0xe714
[0]crcmatrix     : 0x1fd7
[0]crcstate      : 0x8e3a
[0]crcfinal      : 0x65c5
Correct operation validated. See readme.txt for run and reporting rules.
CoreMark 1.0 : 395.961196 / GCC15.2.0 -O3 / Static
```
