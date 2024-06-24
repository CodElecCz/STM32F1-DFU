py checksum.py SW_v0101_DFU.bin

    Firmware size 45900
    Firmware size after padding 45900
    Firmware size for checksum purposes 45900

--------------------------------------------------------------------------------

dfu-util -l

    ddfu-util 0.11

    Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
    Copyright 2010-2021 Tormod Volden and Stefan Schmidt
    This program is Free Software and has ABSOLUTELY NO WARRANTY
    Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

    Found DFU: [0483:df11] ver=0200, devnum=29, cfg=1, intf=0, path="1-1.3", 
    alt=0, name="@Internal Flash /0x08000000/8*001Ka,120*001Kg", 
    serial="54FF6C068687545516541567"

--------------------------------------------------------------------------------

dfu-util -d -vid:pid,0x0483:0xDF11 -D SW_v0101_DFU.bin -s0x08002000:leave

    Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
    Copyright 2010-2021 Tormod Volden and Stefan Schmidt
    This program is Free Software and has ABSOLUTELY NO WARRANTY
    Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

    Warning: Invalid DFU suffix signature
    A valid DFU suffix will be required in a future dfu-util release
    Opening DFU capable USB device...
    Device ID 0483:df11
    Device DFU version 011a
    Claiming USB DFU Interface...
    Setting Alternate Interface #0 ...
    Determining device status...
    DFU state(2) = dfuIDLE, status(0) = No error condition is present
    DFU mode device DFU version 011a
    Device returned transfer size 1024
    DfuSe interface name: "Internal Flash "
    Downloading element to address = 0x08002000, size = 45900
    Erase           [=========================] 100%        45900 bytes
    Erase    done.
    Download        [=========================] 100%        45900 bytes
    Download done.
    File downloaded successfully
    Submitting leave request...
    Transitioning to dfuMANIFEST state
