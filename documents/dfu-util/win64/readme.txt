dfu-util -l
dfu-util 0.11

Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.
Copyright 2010-2021 Tormod Volden and Stefan Schmidt
This program is Free Software and has ABSOLUTELY NO WARRANTY
Please report bugs to http://sourceforge.net/p/dfu-util/tickets/

Found DFU: [dead:ca5d] ver=0200, devnum=47, cfg=1, intf=0, path="1-4.1", alt=0, name="@Internal Flash /0x08000000/4*001Ka,124*001Kg", serial="54ff6a064865844820111767"
--------------------------------------------------------------------------------

4kb:
dfu-util -d -vid:pid,0x0483:0xDF11 -D STM32F1-SL20.bin -s0x08001000
