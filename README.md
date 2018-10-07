# Jcm_Server


The JCM (Jtag Configuration Manager) is an embedded system running Arch Linux. It is used in the Configurable Computing Lab at BYU, and its purpose is to monitor the response that FPGA devices have when exposed to radiation. The JCM can also emulate the behavior of radiation and inject faults into the FPGA configuration memory, providing a safe, efficient way to do radation tests.

The Jcm Server is a program that allows a researcher to control the JCM (which connects to their FPGA) remotely and with ease. It provides an API for the researcher to use and automatically connects to and configures the JCM to the FPGA settings, so the user doesn't have to. This is an early version of the JCM server written entirely in C++, that uses a direct socket interface rather than HTTP. As such, it is deprecated, and I have written a much better version in Python, with a RESTFUL http api instead. However, this early (unused) version showcases some of my skills with networking and low-level C/C++ programming.
