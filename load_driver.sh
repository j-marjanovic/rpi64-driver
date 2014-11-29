sudo modprobe spi_bcm2708
sudo modprobe spi-config devices=bus=0:cs=0:modalias=pi64_dev:speed=1000000:force_release
sudo insmod pi64_dev.ko
