#include <inttypes.h>

// Vin     R1      V      R2      A      R3
//  L----/\/\/\----+----/\/\/\----+----/\/\/\---- GND
//values in 1/10 ohm
//#define R1 6000000
#define R1  9380000
#define R2    99800
#define R3      10
//#define Vdiv 1087 //((R1+R2+R3)/(R2+R3))/10 //108.7
#define Vdiv 97 //((R1+R2+R3)/(R2+R3))/10 //108.7
#define vref 2036

class PGA
{
	private:
	
	
	public:
	uint8_t (*EEreadbyte)(long);
	void (*EEreadmem)(long, uint8_t *, long);
	void (*EEwritemem)(long, uint8_t *, int);

	char gain;
	void init(void);
	long MeasureCurrent(void);
	long MeasureVoltage(long Current);
	double MeasureRes(uint8_t lowMode);
	void SetPGA(uint8_t G, uint8_t Ch);
};