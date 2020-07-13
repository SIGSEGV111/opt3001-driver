#ifndef _include_opt3001_opt3001_hpp_
#define _include_opt3001_opt3001_hpp_

#include <stdint.h>

namespace opt3001
{
	extern bool DEBUG;

	class TOPT3001
	{
		protected:
			const int fd_i2cbus;
			const uint8_t address;
			uint8_t regindex_cache;
			double lux;

			void WriteRegister(const uint8_t index, const uint16_t value);
			uint16_t ReadRegister(const uint8_t index);

		public:
			double Lux() const { return lux; }
			void Refresh(const bool clear_irq = false);

			void Reset();

			TOPT3001(const char* const i2c_bus_device, const uint8_t address);
			TOPT3001(const int fd_i2cbus, const uint8_t address);
			~TOPT3001();
	};
}

#endif
