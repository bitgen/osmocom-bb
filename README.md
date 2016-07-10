# SDR based PHY for OsmocomBB

It's no secret for everyone, that today OsmocomBB is not actively maintained as well as OpenBSC, for example. I think it's mostly due to supported hardware limitations. Currently supported platforms is still a bit of 'black box'. Moreover every day it's harder to find and buy a new one.

Fortunately, there are many SDR platforms available now. Especially interesting devices are USRP, UmTRX, bladeRF, and recently introduced LimeSDR. They can be easily programmed to support just about any type of wireless standard, of course, including some mobile telecommunication stacks. As well as for network side back-end, they can be used to perform MS side operations, excepting frequency-hopping and some phone specific features (like SIM I/O).

So, I think there is a way to bring a new live to OsmocomBB project. We can make one work on SDR hardware platforms implementing the 'bridge' between both already implemented L2/L3 and OsmoTRX. There already was some attempts (see sylvain/ms-sdr branch) to make described dreams come true, but development was stopped. And now I am going to start to work around this direction.

#### Why?

+ Currently it's hard to find/buy hardware that is supported by OsmocomBB
+ There are lots of restrictions of supported hardware platforms (Calypso based phones)
+ Some software/hardware parts (mostly DSP) are still work as a black box

#### What for?

+ This is a way to bring OsmocomBB back to life
+ New opportunities for education and research
+ Next generation networks support (UMTS)
+ More flexible voice routing
+ GPRS and EDGE support
+ Multi SIM support

Any help and contributions are welcome!

## The road map (draft)

1. Implement UDP connection between both osmo-trx and trxcon
2. Clock indication (debug print only for now)
3. CTRL commands implementation
4. GSM L1 implementation (TS 05.03)
5. OsmoBTS scheduler integration
6. The '/tmp/osmocom_l2' socket handlers
