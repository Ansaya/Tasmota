
#ifdef USE_AWNING

const char awning_label[] = "AWNING";
const char awning_cmnd_up[] = "UP";
const char awning_cmnd_down[] = "DOWN";
const char awning_cmnd_stop[] = "STOP";

byte awning_up = 99;
byte awning_down = 99;
byte awning_stop = 99;
boolean awning_mqtt_en = false;

int awning_position = 0;
boolean awning_pin_enabled = false;
boolean awning_invert = false;

typedef enum {
    AWNING_UP,
    AWNING_DOWN,
    AWNING_STOP
} awning_direction_t;

awning_direction_t awning_dir = AWNING_STOP;

void awning_init()
{
    awning_up = pin[GPIO_AWNING_UP];
    awning_down = pin[GPIO_AWNING_DOWN];
    awning_stop = pin[GPIO_AWNING_STOP];

    pinMode(awning_up, OUTPUT);
    digitalWrite(awning_up, LOW);
    pinMode(awning_down, OUTPUT);
    digitalWrite(awning_down, LOW);
    if(awning_stop < 99) {
        pinMode(awning_stop, OUTPUT);
        digitalWrite(awning_stop, LOW);
    }

    awning_position = Settings.awning_position;

    snprintf_P(log_data, sizeof(log_data), PSTR("%s: initialized options (UP = %d - DOWN = %d - STOP = %d - POS = %d - TIME = %ds)"), 
        awning_label, awning_up, awning_down, awning_stop, awning_position, AWNING_OPEN_UP_TIME);
    AddLog(LOG_LEVEL_DEBUG);
}

void awning_move(const char* dir) 
{
    snprintf_P(log_data, sizeof(log_data), PSTR("%s: required move %s"), awning_label, dir);
    AddLog(LOG_LEVEL_DEBUG);

    if(!strcasecmp(dir, awning_cmnd_up)) {
        if(awning_dir != AWNING_UP && awning_position > 0) {
            if(awning_dir == AWNING_DOWN) {
                if(awning_stop < 99) {
                    digitalWrite(awning_stop, HIGH);
                }
                awning_invert = true;
            }
            else {
                digitalWrite(awning_up, HIGH);
            }
            awning_dir = AWNING_UP;
            awning_pin_enabled = true;
        }
        snprintf_P(log_data, sizeof(log_data), PSTR("%s: moving up"), awning_label);
        AddLog(LOG_LEVEL_DEBUG);
    }
    else if(!strcasecmp(dir, awning_cmnd_down)) {
        if(awning_dir != AWNING_DOWN && awning_position < AWNING_OPEN_UP_TIME) {
            if(awning_dir == AWNING_UP) {
                if(awning_stop < 99) {
                    digitalWrite(awning_stop, HIGH);
                }
                awning_invert = true;
            }
            else {
                digitalWrite(awning_down, HIGH);
            }
            awning_dir = AWNING_DOWN;
            awning_pin_enabled = true;
        }
        snprintf_P(log_data, sizeof(log_data), PSTR("%s: moving down"), awning_label);
        AddLog(LOG_LEVEL_DEBUG);
    }
    else if(!strcasecmp(dir, awning_cmnd_stop)) {
        if(awning_dir != AWNING_STOP) {
            awning_dir = AWNING_STOP;
            if(awning_stop < 99) {
                digitalWrite(awning_stop, HIGH);
                awning_pin_enabled = true;
            }
            snprintf_P(log_data, sizeof(log_data), PSTR("%s: stopped"), awning_label);
            AddLog(LOG_LEVEL_DEBUG);
        }
    }

    // TODO: implement positional command (receive opening percentage and act consequently)
}

void awning_pin_disable()
{
    if(awning_pin_enabled) {
        awning_pin_enabled = false;
        snprintf_P(log_data, sizeof(log_data), PSTR("%s: pin disable"), awning_label);
        AddLog(LOG_LEVEL_DEBUG);

        switch(awning_dir) {
            case AWNING_UP:
                if(awning_invert) {
                    awning_invert = false;
                    if(awning_stop < 99) {
                        digitalWrite(awning_stop, LOW);
                    }
                    digitalWrite(awning_up, HIGH);
                    awning_pin_enabled = true;
                }
                else {
                    digitalWrite(awning_up, LOW);                    
                }
                break;
            case AWNING_DOWN:
                if(awning_invert) {
                    awning_invert = false;
                    if(awning_stop < 99) {
                        digitalWrite(awning_stop, LOW);
                    }
                    digitalWrite(awning_down, HIGH);
                    awning_pin_enabled = true;
                }
                else {
                    digitalWrite(awning_down, LOW);
                }
                break;
            case AWNING_STOP:
                if(awning_stop < 99) {
                    digitalWrite(awning_stop, LOW);
                }
                break;
            default:
                break;
        }
    }
}

void awning_update_position()
{
    if(awning_dir == AWNING_STOP) {
        return;
    }

    if(awning_dir == AWNING_UP) {
        awning_position--;
    }
    else {
        awning_position++;
    }

    if(awning_position < 0) {
        awning_position = 0;
        awning_move(awning_cmnd_stop);
    }
    else if(awning_position > AWNING_OPEN_UP_TIME) {
        awning_position = AWNING_OPEN_UP_TIME;
        awning_move(awning_cmnd_stop);
    }
    
    awning_mqtt_stat();
}

void awning_mqtt_stat()
{
    if(!awning_mqtt_en) {
        return;
    }

    uint8_t percentage = awning_position > 0 ? 100 : 0;

    if(awning_position > 0 && awning_position < AWNING_OPEN_UP_TIME) {
        percentage = awning_position * 100 / AWNING_OPEN_UP_TIME;
    }

    snprintf_P(log_data, sizeof(log_data), PSTR("%s: %d%"), awning_label, percentage);
    AddLog(LOG_LEVEL_DEBUG);

    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%d"), percentage);
    MqttPublishPrefixTopic_P(STAT, awning_label);
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

#define XDRV_92

boolean Xdrv92(byte function)
{
    boolean result = false;

    if(pin[GPIO_AWNING_UP] < 99 && pin[GPIO_AWNING_DOWN] < 99) {
        switch (function) {
            case FUNC_INIT:
                awning_init();
                break;
            case FUNC_MQTT_INIT:
                awning_mqtt_en = true;
                awning_mqtt_stat();
                break;
            case FUNC_EVERY_SECOND:
                awning_update_position();
                awning_pin_disable();
                break;
            case FUNC_COMMAND:
                if(!strcasecmp(XdrvMailbox.topic, awning_label)) {
                    snprintf_P(log_data, sizeof(log_data), PSTR("%s: COMMAND %s"), awning_label, XdrvMailbox.data);
                    AddLog(LOG_LEVEL_DEBUG);
                    awning_move(XdrvMailbox.data);
                    result = true;
                }
                break;
            case FUNC_FREE_MEM:
                break;
            case FUNC_SAVE_BEFORE_RESTART:
                Settings.awning_position = awning_position;
                break;
            default:
                break;
        }
    }

  return result;
}

#endif