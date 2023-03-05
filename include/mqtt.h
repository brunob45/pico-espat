#ifndef PICO_ESPAT_MQTT
#define PICO_ESPAT_MQTT

#include "pico/stdlib.h"

bool mqtt_usercfg(const char *scheme, const char *client_id);
bool mqtt_conncfg(const char *keep_alive, const char *lwt_topic, const char *lwt_data, const bool retain);
bool mqtt_conn(const char *host, const char *port);
bool mqtt_clean();
bool mqtt_pub(const char *topic, const char *data, const bool retain);
bool mqtt_reset(uint pin);

#endif // PICO_ESPAT_MQTT