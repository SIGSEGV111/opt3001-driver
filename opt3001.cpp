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

	void TOPT3001::Refresh()
	{
		const uint16_t raw_lux = ReadRegister(0x00);
		const uint16_t exponent = raw_lux & 0x000f;
		const uint16_t mantissa = (raw_lux & 0xfff0) >> 4;

		const double lsb = 0.01 * pow(2, exponent);
		this->lux = lsb * (double)mantissa;

		if(DEBUG) fprintf(stderr, "raw_lux = %04hx\nexponent = %hu\nmantissa = %hu\nlsb = %lf\nlux = %lf\n", raw_lux, exponent, mantissa, lsb, this->lux);
	}

	TOPT3001::TOPT3001(const char* const i2c_bus_device, const uint8_t address) : fd_i2cbus(SYSERR(open(i2c_bus_device, O_RDWR | O_CLOEXEC | O_SYNC))), address(address), regindex_cache((uint8_t)-1), lux(-1.0)
	{
		SYSERR(ioctl(this->fd_i2cbus, I2C_SLAVE, this->address));
	}

	TOPT3001::TOPT3001(const int fd_i2cbus, const uint8_t address) : fd_i2cbus(fd_i2cbus), address(address), regindex_cache((uint8_t)-1), lux(-1.0)
	{
		SYSERR(ioctl(this->fd_i2cbus, I2C_SLAVE, this->address));
	}

	TOPT3001::~TOPT3001()
	{
		close(this->fd_i2cbus);
	}
}
