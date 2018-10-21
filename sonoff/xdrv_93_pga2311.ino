
#ifdef USE_PGA2311

#define PGA2311_MAX_VOLUME 100

const char pga2311_label[] = "VOLUME";
boolean pga2311_mqtt_en = false;

uint8_t pga2311_pin_mute = 99;
uint8_t pga2311_pin_cs = 99;
uint8_t pga2311_volume = 0;

#if defined(PGA2311_NON_LINEAR_1) || defined(PGA2311_NON_LINEAR_2)
#if defined(PGA231_NON_LINEAR_2)
uint8_t volumeToValue[PGA2311_MAX_VOLUME+1] = {
    0,12,21,31,40,50,58,65,71,77,77,79,79,81,83,83,84,86,
    88,88,90,92,94,96,96,98,100,102,102,104,106,108,109,109,
    111,113,113,115,117,119,119,121,123,123,125,127,129,129,
    131,132,132,134,136,136,138,140,140,142,144,144,146,148,
    148,150,150,152,154,154,156,157,157,159,159,161,163,163,
    165,167,167,169,169,171,173,173,175,175,177,179,179,180,
    180,182,182,184,186,186,188,188,190,190,192
};
#else
uint8_t volumeToValue[PGA2311_MAX_VOLUME+1] = {
    0,13,19,23,25,27,29,33,35,36,38,40,42,44,46,50,52,
    54,56,58,60,61,63,65,67,69,71,73,75,77,79,81,83,83,
    84,86,88,90,92,94,96,98,100,102,104,104,106,108,109,
    111,113,115,117,117,119,121,123,125,127,129,129,131,
    132,134,136,138,140,140,142,144,146,148,150,150,152,
    154,156,157,157,159,161,163,165,165,167,169,171,173,
    175,175,177,179,180,180,182,184,186,188,188,190,192
};
#endif
#endif

void pga2311_write(uint8_t data)
{
    if(data > PGA2311_MAX_VOLUME) {
        data = PGA2311_MAX_VOLUME;
    }

    #if defined(PGA2311_NON_LINEAR_1) || defined(PGA2311_NON_LINEAR_2)
    uint8_t volume = volumeToValue[data];
    #else
    uint8_t volume = (uint8_t)(255 - 2 * (31,5 - (data * 0,96 - 96)));
    #endif

    digitalWrite(pga2311_pin_cs, LOW);
    SPI.write16((volume << 8) | volume);
    digitalWrite(pga2311_pin_cs, HIGH);
}

void pga2311_init()
{
    pga2311_pin_mute = pin[GPIO_PGA2311_MUTE];
    pinMode(pga2311_pin_mute, OUTPUT);
    digitalWrite(pga2311_pin_mute, HIGH);

    pga2311_pin_cs = pin[GPIO_SPI_CS];
    pinMode(pga2311_pin_cs, OUTPUT);
    digitalWrite(pga2311_pin_cs, HIGH);
    SPI.begin();

    pga2311_write(Settings.pga2311_volume);

    snprintf_P(log_data, sizeof(log_data), PSTR("%s: pga2311 initialized (MOSI = %d - SCLK = %d - CS = %d)"), 
        pga2311_label, pin[GPIO_SPI_MOSI], pin[GPIO_SPI_CLK], pga2311_pin_cs);
    AddLog(LOG_LEVEL_DEBUG);
}

void pga2311_command(const char* data)
{
    pga2311_volume = atoi(data);

    snprintf_P(log_data, sizeof(log_data), PSTR("%s: pga2311 required volume %d"), pga2311_label, pga2311_volume);
    AddLog(LOG_LEVEL_DEBUG);

    pga2311_write(pga2311_volume);
    pga2311_mqtt_stat();
}

void pga2311_mqtt_stat()
{
    if(pga2311_mqtt_en) {
        snprintf_P(log_data, sizeof(log_data), PSTR("%s: %d"), pga2311_label, pga2311_volume);
        AddLog(LOG_LEVEL_DEBUG);

        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%d"), pga2311_volume);
        MqttPublishPrefixTopic_P(STAT, pga2311_label);
    }
}

#define XDRV_93

boolean Xdrv93(byte function) 
{
    boolean result = false;

    if(spi_flg && pin[GPIO_PGA2311_MUTE] < 99) {
        switch(function) {
            case FUNC_INIT:
                pga2311_init();
                break;
            case FUNC_MQTT_INIT:
                pga2311_mqtt_en = true;
                pga2311_mqtt_stat();
                break;
            case FUNC_COMMAND:
                if(!strcasecmp(XdrvMailbox.topic, pga2311_label)) {
                    snprintf_P(log_data, sizeof(log_data), PSTR("%s: COMMAND %s"), pga2311_label, XdrvMailbox.data);
                    AddLog(LOG_LEVEL_DEBUG);
                    pga2311_command(XdrvMailbox.data);
                    result = true;
                }
                break;
            case FUNC_FREE_MEM:
                break;
            case FUNC_SAVE_BEFORE_RESTART:
                Settings.pga2311_volume = pga2311_volume;
                break;
            default:
                break;
        }
    }

    return result;
}

#endif