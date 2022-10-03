#P1 Energy Monitor

In the Netherlands, most "smart" household E-meters must adhere to the DSMR (Dutch Smart Meter Requirements), which standardizes the way how these devices
communicates with the energy providers. Part of the specification is also a local serial port (P1-port) which makes it possible to connect local devices
so the consumer can monitor his energy consumption.
The P1-port offers a simplex serial port (only transmitting), making it possible to read-out the real time values of consumed power, produced power (solar panels etc.), 
actual voltage, current but also the current gas meter value (when there is also an smart gas meter, it can connect to the E-meter for example using the MBUS-protocol).
