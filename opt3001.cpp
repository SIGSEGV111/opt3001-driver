#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <math.h>
#include <arpa/inet.h>

#include "opt3001.hpp"

#define SYSERR(expr) (([&](){ const auto r = ((expr)); if( (long)r == -1L ) { throw #expr; } else return r; })())

namespace opt3001
{
	bool DEBUG = false;

	void TOPT3001::WriteRegister(const uint8_t index, const uint16_t value)
	{
		const uint16_t tmp_value = htons(value);
		uint8_t buffer[3];
		buffer[0] = index;
		memcpy(buffer + 1, &tmp_value, 2);
		this->regindex_cache = (uint8_t)-1;

		if(SYSERR(write(this->fd_i2cbus, buffer, 3)) != 3)
			throw "write register failed";

		this->regindex_cache = index;
	}

	uint16_t TOPT3001::ReadRegister(const uint8_t index)
	{
		if(index != this->regindex_cache)
		{
			if(SYSERR(write(this->fd_i2cbus, &index, 1)) != 1)
				throw "failed to set register address";
			this->regindex_cache = index;
		}

		uint16_t value;
		if(SYSERR(read(this->fd_i2cbus, &value, 2)) != 2)
			throw "read register failed";

		return ntohs(value);
	}

	void TOPT3001::Refresh(const bool clear_irq)
	{
		const uint16_t raw_lux = ReadRegister(0x00);
		const uint16_t exponent = (raw_lux & 0xf000) >> 12;
		const uint16_t mantissa = (raw_lux & 0x0fff);

		const double lsb = 0.01 * pow(2, exponent);
		this->lux = lsb * (double)mantissa;

		if(DEBUG) fprintf(stderr, "[DEBUG] raw_lux = %04hx ; exponent = %hu ; mantissa = %hu ; lsb = %lf ; lux = %lf\n", raw_lux, exponent, mantissa, lsb, this->lux);

		if(clear_irq)
			ReadRegister(0x01);
	}

	void TOPT3001::Reset()
	{
		SYSERR(ioctl(this->fd_i2cbus, I2C_SLAVE, this->address));

		const uint16_t man_id = ReadRegister(0x7e);
		const uint16_t dev_id = ReadRegister(0x7f);

		fprintf(stderr, "[INFO] Manufacturer ID = %04hx\n[INFO]       Device ID = %04hx\n", man_id, dev_id);

		const uint8_t reset_code = 0x06;
		if(SYSERR(write(this->fd_i2cbus, &reset_code, 1)) != 1)
			throw "faioled to send device reset command";

		struct
		{
			uint16_t
				 fc : 2,
				 me : 1,
				 pol : 1,
				 l : 1,
				 fl : 1,
				 fh : 1,
				 crf : 1,
				 ovf : 1,
				 m : 2,
				 ct : 1,
			         rn : 4;
		} config;

		config.rn = 0b1100;
		config.ct = 1;
		config.m = 0b11;
		config.l = 0;
		config.pol = 0;
		config.me = 0;
		config.fc = 0;

		WriteRegister(0x01, *(int16_t*)&config);	//	config
		WriteRegister(0x02, 0b1100000000000000);	//	low
		WriteRegister(0x03, 0x0);					//	high
	}

	TOPT3001::TOPT3001(const char* const i2c_bus_device, const uint8_t address) : fd_i2cbus(SYSERR(open(i2c_bus_device, O_RDWR | O_CLOEXEC | O_SYNC))), address(address), regindex_cache((uint8_t)-1), lux(-1.0)
	{
		Reset();
	}

	TOPT3001::TOPT3001(const int fd_i2cbus, const uint8_t address) : fd_i2cbus(fd_i2cbus), address(address), regindex_cache((uint8_t)-1), lux(-1.0)
	{
		Reset();
	}

	TOPT3001::~TOPT3001()
	{
		close(this->fd_i2cbus);
	}
}
