# Connect to wifi
`AT+CWJAP="Yer A WiFi Harry","DemandeaLuna"`

# MQTT setup
`AT+MQTTUSERCFG=0,1,"rp2040","","",0,0,""`

`AT+MQTTCONNCFG=0,0,0,"home/nodes/sensor/rp2040/status","offline",0,0`

`AT+MQTTCONN=0,"10.0.0.167",1883,0`

`AT+MQTTCLEAN=0`

`AT+MQTTPUB=0,"home/nodes/sensor/rp2040/status","online",0,0`

`AT+MQTTPUBRAW=0,"homeassistant/sensor/rp2040/monitor/config",57,0,0`

```json
{"dev":{"manufacturer":"Invector Labs","model":"rp2040"}}
```