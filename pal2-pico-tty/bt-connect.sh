TESTDEV=2C:CF:67:97:CA:9F

# PAL-2 adaptor bard
#TESTDEV=28:CD:C1:0E:9E:BF

#sudo killall rfcomm
#sleep 10
#sudo killall -9 rfcomm

sudo rfcomm release /dev/rfcomm0

#sudo fuser -v /dev/rfcomm0
   # or:  lsof /dev/rfcomm0

#sudo rfcomm connect /dev/rfcomm0 ${TESTDEV} 

sudo rfcomm bind /dev/rfcomm0 ${TESTDEV}

rfcomm

#minicom bt


