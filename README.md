EZ100PU open-source driver
=========================

***NOT FUNCTIONAL***

EZ100PU smartcard readers are (still?) very popular in Taiwan, and commonly
used to read NHI (health insurance) cards to access services (tax payment,
health insurance data, etc.).

There is (was?) an closed source at this
[URL](https://www.castlestech.com/wp-content/uploads/2016/08/201511920271676073.zip),
but the link is now dead (it's, however, possible to find at least one mirror).

The goal here is to replicate enough of the driver to support the basic
features required by the NHI service [mLNHIICC](https://eservice.nhi.gov.tw/Personal1/System/HealthCard.aspx).

# Status

 - Can power on, and down, the card.
 - Can read ATR.
 - Unable to do anything more complicated.
 - When starting to dig into operations, I realized that the protocol is
   _very_ close to official USB CCID spec.

# Debugging tricks

## Run

This doesn't include an `Info.plist`, so you'll have to get it from the original
closed source driver.

This builds a file `ezusb.so` that you can just swap in
`/usr/lib/pcsc/drivers/ezusb.bundle/Contents/Linux/`.

## Packet capture and debug

```
sudo modprobe usbmon
sudo tcpdump --list-interfaces
# With lsusb, check which bus to monitor
sudo tcpdump -i usbmon3 -s0 -w capture-read.pcap
# I like to use wireshark to look at the pcap.
```
