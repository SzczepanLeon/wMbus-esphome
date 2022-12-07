#include "esphome.h"

#include "my_config.hpp"

#include "rf_mbus.hpp"
#include "crc.hpp"
#include "mbus_packet.hpp"
#include "utils.hpp"

class WmBusGWComponent : public Component {
  protected:
    HighFrequencyLoopRequester high_freq_;

  public:
    float get_setup_priority() const override { return esphome::setup_priority::LATE; }

    void setup() override {
      this->high_freq_.start();
      memset(MBpacket, 0, sizeof(MBpacket));
      rf_mbus_init(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS, GDO0, GDO2);

      restartInfo = false;
    }

    void loop() override {
      if (restartInfo == false) {
        if (client.connect(CLIENT_RTLWMBUS_IP, CLIENT_RTLWMBUS_PORT)) {
          time_t currTime = id(time_sntp).now().timestamp;
          strftime(telegramTime, sizeof(telegramTime), "%Y-%m-%d %H:%M:%S", localtime(&currTime));
          client.printf("[%s] R E S T A R T\n", telegramTime);
          client.stop();
          restartInfo = true;
        }
      }

      if (rf_mbus_task(MBpacket, rssi, GDO0, GDO2)) {
        uint8_t lenWithoutCrc = crcRemove(MBpacket, packetSize(MBpacket[0]));
        ESP_LOGD("wmBus GW", "T: %s", format_hex_pretty(MBpacket, lenWithoutCrc).c_str());
        // hex format
        if (client.connect(CLIENT_HEX_IP, CLIENT_HEX_PORT)) {
          client.write((const uint8_t *) MBpacket, lenWithoutCrc);
          client.stop();
        }
        // rtlwmbus format
        if (client.connect(CLIENT_RTLWMBUS_IP, CLIENT_RTLWMBUS_PORT)) {
          time_t currTime = id(time_sntp).now().timestamp;
          // T1;1;1;2020-07-19 05:28:57.000;46;95;43410778;0x1944
          strftime(telegramTime, sizeof(telegramTime), "%Y-%m-%d %H:%M:%S.000", localtime(&currTime));
          client.printf("T1;1;1;%s;%d;;;0x", telegramTime, rssi);
          for (int i = 0; i < lenWithoutCrc; i++){
            client.printf("%02X", MBpacket[i]);
          }
          client.print("\n");
          client.stop();
        }
        memset(MBpacket, 0, sizeof(MBpacket));
      }
    }

  private:
    WiFiClient client;
    uint8_t MBpacket[291];
    char telegramTime[24];
    int rssi = 0;
    bool restartInfo = false;
};
