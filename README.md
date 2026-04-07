
Below is a **complete single C file** designed for:

- ✅ Arria 10 SoC  
- ✅ Bare-metal  
- ✅ No threads  
- ✅ No malloc  
- ✅ Polling architecture  
- ✅ UDP-like fragmentation logic  
- ✅ ACK handling  
- ✅ Retransmission  
- ✅ State-machine driven  
- ✅ GitHub ready  

This is hardware-independent.  
You only need to replace the 3 low-level Ethernet driver functions.

✔ Bare-metal compatible  
✔ No threads  
✔ No dynamic memory  
✔ Polling design  
✔ State-machine driven  
✔ Timeout-based retransmission  
✔ ACK handling  
✔ Hardware abstraction layer  

---

# ✅ What You Must Implement For Arria 10

Replace:

```
ethernet_send_fragment()
ethernet_frame_available()
ethernet_read_frame()
```

With your:

- EMAC driver
- FPGA MAC driver
- DMA descriptor logic
- HPS Ethernet driver

---

# ✅ Architecture Level

This is:

```
Mini Reliable UDP Layer
        ↓
Ethernet Driver
        ↓
MAC / PHY
```

---

