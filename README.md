# UEFI-Utilities-2018

Various UEFI utilities built against UDK2017

To integrate into UDK2017 source tree:

  Create a subdirectory immediately under UDK2017 called MyApps, populate MyApps with one or more of the 
  utilities, fix up MyApps.dsc to build the required utility or utilities by uncommening one or more .inf
  lines.

Or you can just build the tools using docker:
```
make
```

Note these utilities have only been built and tested on an X64 platform.

I now include a 64-bit EFI binary of each utility in each utility subdirectory.

See http://blog.fpmurphy.com for detailed information about the utilities.

Enjoy!
