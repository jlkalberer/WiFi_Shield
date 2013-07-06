#include <string.h>
#include "WiFly.h"
#include "Debug.h"

WiFly* WiFly::instance;

#define NEWLINE "\r\n"
#define AOK "AOK"
#define ERR "ERR"
#define SET "set "
#define END "\r"
#define IP2 "i p 2"
#define CR0 "c r 0"

WiFly::WiFly(uint8_t rx, uint8_t tx) : SoftwareSerial(rx, tx)
{
	instance = this;

	SoftwareSerial::begin(DEFAULT_BAUDRATE);
	SoftwareSerial::
	setTimeout(DEFAULT_WAIT_RESPONSE_TIME);

	command_mode = false;
	associated = false;
}
//#ifdef _DEBUG
size_t WiFly::print(unsigned int val, int dec)
{
	Serial.print(val);
	return SoftwareSerial::print(val, dec);
}
size_t WiFly::print(const Printable& p)
{
	return SoftwareSerial::print(p);
}
size_t WiFly::print(const char* val)
{
	Serial.print(val);
	return SoftwareSerial::print(val);
}
size_t WiFly::println(const char* val)
{
	Serial.print(val);
	Serial.print(NEWLINE);
	return SoftwareSerial::println(val);
}
size_t WiFly::println(unsigned int val, int dec)
{
	Serial.print(val);
	Serial.print(NEWLINE);
	return SoftwareSerial::println(val, dec);
}
size_t WiFly::println(void)
{
	Serial.print(NEWLINE);
	return SoftwareSerial::println();
}
//#endif

boolean WiFly::reset()
{
	sendCommand("close" END);
	return sendCommand("factory R" END, "Defaults");
}

boolean WiFly::reboot()
{
  sendCommand("reboot" END);
  command_mode = false;
  return true;
}

boolean WiFly::init()
{
	boolean result = true;

#if 0
	// set time
	result = result & sendCommand(SET "c t 20" END, AOK);

	// set size
	result = result & sendCommand(SET "c s 128" END, AOK);

	// red led on when tcp connection active
	result = result & sendCommand(SET "s i 0x40" END, AOK);

	// no string sent to the tcp client
	result = result & sendCommand(SET CR0 END, AOK);

	// tcp protocol
	result = result & sendCommand(SET IP2 END, AOK);

	// tcp retry
	result = result & sendCommand(SET "i f 0x7" END, AOK);
	//no echo
	result = result & sendCommand(SET "u m 1" END, AOK);
	// no auto join
	result = result & sendCommand(SET "w j 0" END, AOK);

	// DHCP on
	result = result & sendCommand(SET "i d 1" END, AOK);
#endif

	return result;
}

boolean WiFly::staticIP(const char *ip, const char *mask, const char *gateway)
{
	boolean result = true;
	char cmd[MAX_CMD_LEN];

	result = sendCommand(SET "i d 0" END, AOK);

	snprintf(cmd, MAX_CMD_LEN, SET "i a %s" END, ip);
	result = result & sendCommand(cmd, AOK);

	snprintf(cmd, MAX_CMD_LEN, SET "i n %s" END, mask);
	result = result & sendCommand(cmd, AOK);

	snprintf(cmd, MAX_CMD_LEN, SET "i g %s" END, gateway);
	result = result & sendCommand(cmd, AOK);

	return result;
}

boolean WiFly::join(const char *ssid, const char *phrase, int auth)
{
	char cmd[MAX_CMD_LEN];

	// ssid
	snprintf(cmd, MAX_CMD_LEN, SET "w s %s" END, ssid);
	sendCommand(cmd, AOK);

	//auth
	snprintf(cmd, MAX_CMD_LEN, SET "w a %d" END, auth);
	sendCommand(cmd, AOK);

	//key
	if (auth != WIFLY_AUTH_OPEN) {
		if (auth == WIFLY_AUTH_WEP)
			snprintf(cmd, MAX_CMD_LEN, SET "w k %s" END, phrase);
		else
			snprintf(cmd, MAX_CMD_LEN, SET "w p %s" END, phrase);

		sendCommand(cmd, AOK);
	}


	//join the network
	if (!sendCommand("join" END, "ssociated")) {
		return false;
	}

	clear();

	associated = true;
	return true;
}

boolean WiFly::leave()
{
	if (sendCommand("leave" END, "DeAuth")) {
		associated = false;
		return true;
	}
	return false;
}

boolean WiFly::connect(const char *host, uint16_t port, int timeout)
{
	char cmd[MAX_CMD_LEN];
	snprintf(cmd, sizeof(cmd), SET "d n %s" END, host);
	sendCommand(cmd, AOK);
	snprintf(cmd, sizeof(cmd), SET "i r %d" END, port);
	sendCommand(cmd, AOK);
	sendCommand(SET IP2 END, AOK);
	sendCommand(SET "i h 0" END, AOK);
	sendCommand(SET CR0 END, AOK);
	if (!sendCommand("open" END, "*OPEN*", timeout)) {
		sendCommand("close" END);
		clear();
		return false;
	}

	command_mode = false;
	return true;
}
boolean WiFly::connect(const IPAddress host, uint16_t port, int timeout)
{
	char cmd[MAX_CMD_LEN];
	snprintf(cmd, sizeof(cmd), SET "d n %s" END, host);
	sendCommand(cmd, AOK);
	snprintf(cmd, sizeof(cmd), SET "i r %d" END, port);
	sendCommand(cmd, AOK);
	sendCommand(SET IP2 END, AOK);
	sendCommand(SET "i h 0" END, AOK);
	sendCommand(SET CR0 END, AOK);
	if (!sendCommand("open" END, "*OPEN*", timeout)) {
		sendCommand("close" END);
		clear();
		return false;
	}

	command_mode = false;
	return true;
}


int WiFly::send(const uint8_t *data, int len, int timeout)
{
	int write_bytes = 0;
	boolean write_error = false;
	unsigned long start_millis;

	if (data == NULL) {
		return 0;
	}
	while (write_bytes < len) {
		if (write(data[write_bytes]) == 1) {
			write_bytes++;
			write_error = false;
		} else {         // failed to write, set timeout
			if (write_error) {
				if ((millis() - start_millis) > timeout) {
					DBG("Send data. Timeout!\r\n");
					break;
				}
			} else {
				write_error = true;
				start_millis = millis();
			}
		}
	}

	return write_bytes;
}

int WiFly::send(const char *data, int timeout)
{
	send((uint8_t*)data, strlen(data), timeout);
}

boolean WiFly::ask(const char *q, const char *a, int timeout)
{
	unsigned long start;
	unsigned long end;
	int q_len = strlen(q);
	send((uint8_t *)q, q_len, timeout); 

	if (a != NULL) {
		setTimeout(timeout);
		start = millis();
		boolean found = find((char *)a);
		if (!found) {
			end = millis();
			if ((end - start) < timeout) {
				DBG(NEWLINE);
				DBG(q);
				DBG("\r\nTry to find: ");
				DBG(a);
				DBG("\r\nTimeout: ");
				DBG(timeout);
				DBG("\r\nStart time: ");
				DBG(start);
				DBG("\r\nEnd time: ");
				DBG(end);
				DBG("\r\n***** Probably ot enough memory *****\r\n");
			}

			return false;
		}
	}

	return true;
}

boolean WiFly::sendCommand(const char *cmd, const char *ack, int timeout)
{
	clear();

	if (!command_mode) {
		commandMode();
	}

	if (!ask(cmd, ack, timeout)) {
		DBG("Failed to run: ");
		DBG(cmd);
		DBG(NEWLINE);
		return false;
	}
	return true;
}

boolean WiFly::commandMode()
{
	if (command_mode)
		return true;

	if (!ask("$$$", "CMD")) {
		if (!ask("" END, ERR)) {
			DBG("Failed: command mode\r\n");
			return false;
		}
	}
	command_mode = true;
	return true;
}

boolean WiFly::dataMode()
{
	if (command_mode) {
		if (!ask("exit" END, "EXIT")) {
			if (ask("" END, ERR)) {
				DBG("Failed: data mode\r\n");
				return false;
			}
		}
		command_mode = false;
	}
	return true;
}

void WiFly::clear()
{
	char r;
	while (receive((uint8_t *)&r, 1, 10) == 1) {
	}
}

float WiFly::version()
{
	if (!sendCommand("ver" END, "Ver ")) {
		return -1;
	}

	char buf[48];
	int read_bytes;
	read_bytes = receive((uint8_t *)buf, 48 -1);
	buf[read_bytes] = '\0';

	float version;
	version = atof(buf);
	if (version == 0) {
		char *ptr = strchr(buf, '<');
		if (ptr != NULL) {
			version = atof(ptr + 1);
		}
	}

	return version;
}



int WiFly::receive(uint8_t *buf, int len, int timeout)
{
	int read_bytes = 0;
	int ret;
	unsigned long end_millis;

	while (read_bytes < len) {
		end_millis = millis() + timeout;
		do {
			ret = read();
			if (ret >= 0) {
				break;
			}
		} while (millis() < end_millis);

		if (ret < 0) {
			return read_bytes;
		}
		buf[read_bytes] = (char)ret;
		read_bytes++;
	}

	return read_bytes;
}
