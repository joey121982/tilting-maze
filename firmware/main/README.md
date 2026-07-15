# mainboard firmware

## Guides

### How to add drivers:
* make a directory inside ```Drivers``` with your driver's name
* add the ```Src``` and ```Inc``` directories of your driver here
* add the source directory to the Makefile on a new line in both the C and C++ sources sections:
```Drivers/YOUR_DRIVER/Src \```
* add the include directory to the Makefile on a new line in the includes section:
```Drivers/YOUR_DRIVER/Inc```
