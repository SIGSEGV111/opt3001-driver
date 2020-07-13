#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/file.h>
#include <stdarg.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include "opt3001.hpp"

static volatile bool do_run = true;

static void OnSignal(int)
{
	do_run = false;
}

#define SYSERR(expr) (([&](){ const auto r = ((expr)); if( (long)r == -1L && errno != EINTR && errno != EAGAIN) throw #expr; else return r; })())

static void WriteFile(const char* const file, const char* const data, ...)
{
	va_list l;
	va_start(l, data);

	char __file[256];
	vsnprintf(__file, sizeof(__file), file, l);

	char __data[256];
	vsnprintf(__data, sizeof(__data), data, l);

	va_end(l);

	const int fd = SYSERR(open(__file, O_WRONLY|O_CLOEXEC|O_NOCTTY|O_SYNC));
	try
	{
		const ssize_t len = strnlen(__data, sizeof(__data));
		if(SYSERR(write(fd, __data, len)) != len)
			throw "write failed";
	}
	catch(...)
	{
		close(fd);
		throw;
	}

	close(fd);
}

static pollfd OpenIrqPin(const int pin)
{
	pollfd pfd;
	memset(&pfd, 0, sizeof(pfd));

	if(pin == -1)
	{
		pfd.fd = -1;
		return pfd;
	}

	try { WriteFile("/sys/class/gpio/export", "%d", pin); } catch(...) {}
	WriteFile("/sys/class/gpio/gpio%d/direction", "in", pin);
	WriteFile("/sys/class/gpio/gpio%d/edge", "falling", pin);

	char buffer[64];
	snprintf(buffer, sizeof(buffer), "/sys/class/gpio/gpio%d/value", pin);

	pfd.fd = SYSERR(open(buffer, O_RDONLY|O_CLOEXEC|O_NOCTTY|O_SYNC));
	pfd.events = POLLPRI|POLLERR;

	return pfd;
}

int main(int argc, char* argv[])
{
	signal(SIGINT,  &OnSignal);
	signal(SIGTERM, &OnSignal);
	signal(SIGHUP,  &OnSignal);
	signal(SIGQUIT, &OnSignal);

	try
	{
		if(argc < 3)
			throw "need at least two arguments: <i2c bus device> <location> [<irq pin>]";

		using namespace opt3001;
		::opt3001::DEBUG = false;

		TOPT3001 opt3001(argv[1], 0x44);

		pollfd pfd_irq = OpenIrqPin(argc > 3 ? atoi(argv[3]) : -1);

		while(do_run)
		{
			timeval ts;

			opt3001.Refresh(pfd_irq.fd != -1);
			SYSERR(gettimeofday(&ts, NULL));

			SYSERR(flock(STDOUT_FILENO, LOCK_EX));
			printf("%ld.%06ld;\"%s\";\"OPT3001\";\"LUX\";%f\n", ts.tv_sec, ts.tv_usec, argv[2], opt3001.Lux());
			fflush(stdout);
			SYSERR(flock(STDOUT_FILENO, LOCK_UN));

			if(pfd_irq.fd == -1)
				usleep(1 * 1000 * 1000);
			else
			{
				char discard[16];
				SYSERR(read(pfd_irq.fd, discard, sizeof(discard)));
				SYSERR(poll(&pfd_irq, 1, -1));
			}
		}

		if(pfd_irq.fd != -1) close(pfd_irq.fd);

		if(argc > 3)
			try { WriteFile("/sys/class/gpio/unexport", "%u", atoi(argv[3])); } catch(...) {}

		fprintf(stderr,"\n[INFO] bye!\n");
		return 0;
	}
	catch(const char* const err)
	{
		fprintf(stderr, "[ERROR] %s\n", err);
		perror("[ERROR] kernel message");
		return 1;
	}
	return 2;
}
