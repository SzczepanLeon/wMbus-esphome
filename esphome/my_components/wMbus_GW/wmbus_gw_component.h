#include "esphome.h"

#include "my_config.hpp"

#include "rf_mbus.hpp"
#include "crc.hpp"
#include "mbus_packet.hpp"
#include "utils.hpp"

#define TAG "wmBus GW"

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
      this->last_connected_ = millis();
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

      if ((millis() - this->last_connected_) > 180000) {
        ESP_LOGE(TAG, "Can't send to wmbusmeters... Restarting...");
        App.reboot();
      }

      if (rf_mbus_task(MBpacket, rssi, GDO0, GDO2)) {
        uint8_t lenWithoutCrc = crcRemove(MBpacket, packetSize(MBpacket[0]));
        ESP_LOGD(TAG, "T: %s", format_hex_pretty(MBpacket, lenWithoutCrc).c_str());
        // hex format
        if (client.connect(CLIENT_HEX_IP, CLIENT_HEX_PORT)) {
          client.write((const uint8_t *) MBpacket, lenWithoutCrc);
          client.stop();
          this->last_connected_ = millis();
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
    uint32_t last_connected_{0};
};
