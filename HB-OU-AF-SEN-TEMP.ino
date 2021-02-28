//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2021-02-26 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
// ci-test=yes board=mega128 aes=no

#define HIDE_IGNORE_MSG

#define A4988_DIR           25 // Pin 32
#define A4988_STEP          24 // Pin 31
#define A4988_SLEEP         23 // Pin 30
#define A4988_ENABLE        22 // Pin 29
#define A4988_STOPPER_RIGHT 20 // Pin 27 INT2
#define A4988_STOPPER_LEFT  21 // Pin 28 INT3
#define A4988_MS1           34 // Pin 41
#define A4988_MS2           35 // Pin 42
#define A4988_MS3           36 // Pin 43
#define MOTOR_STEPS         200
#define RPM                 90
#define MICROSTEPS          16UL
#define MOTOR_ACCEL         1000
#define MOTOR_DECEL         500

#define LCD_CS     28
#define LCD_RST    29
#define LCD_DC     30
#define LCD_BLUE   31
#define LCD_RED    32
#define LCD_GREEN  33
#define LCD_UPDATE_CYCLE 1000 //milliseconds refresh time

#define LED            22 //PD4
#define CONFIG_BUTTON   7 //PE7 INT7

#define CC1101_CS   8 //PB0
#define CC1101_GDO0 6 //PE6 INT6

#define MAX6675_SO        42 // Pin 49
#define MAX6675_CS        43 // Pin 50
#define MAX6675_SCK       44 // Pin 51

#define PEERS_PER_CHANNEL                20
#define PEERS_PER_WT_CHANNEL             4
#define TEMPERATURE_MSG_INTERVAL         180

#include <U8g2lib.h>
#include <SPI.h>

#include <AskSinPP.h>
#include <MultiChannelDevice.h>
#include <Register.h>
#include <sensors/Max6675.h>
#include <Dimmer.h>
#include "LCDDisplay.h"
#include "Nema17.h"

using namespace as;

DEFREGISTER(Reg0,DREG_INTKEY,DREG_CYCLICINFOMSG,MASTERID_REGS,DREG_TRANSMITTRYMAX, DREG_LOWBATLIMIT, 0x2f)
class AFTList0 : public RegList0<Reg0> {
public:
  AFTList0(uint16_t addr) : RegList0<Reg0>(addr) {}

  bool releaseAfterMove (bool value) const { return this->writeRegister(0x2f, value & 0xff); }
  bool releaseAfterMove ()           const { return this->readRegister (0x2f, 0);            }

  void defaults () {
    clear();
    releaseAfterMove(true);
    transmitDevTryMax(6);
  }
};

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
  {0xf3, 0x40, 0x01},     // Device ID
  "JPAFT00001",           // Device Serial
  {0xf3, 0x40},           // Device Model Indoor
  0x10,                   // Firmware Version
  as::DeviceType::ClimateControl, // Device Type
  {0x01, 0x00}            // Info Bytes
};

typedef LibSPI<CC1101_CS> SPIType;
typedef Radio<SPIType, CC1101_GDO0> RadioType;
typedef StatusLed<LED> LedType;
typedef AskSin<LedType, NoBattery, RadioType> Hal;
Hal hal;
LCDDisplay lcdDisplay;

class WeatherEventMsg : public Message {
  public:
    void init(uint8_t msgcnt, uint16_t temp, bool batlow) {
      uint8_t t1 = (temp >> 8) & 0x7f;
      uint8_t t2 = temp & 0xff;
      if ( batlow == true ) {
        t1 |= 0x80; // set bat low bit
      }
      Message::init(0xb, msgcnt, 0x70, BIDI | WKMEUP, t1, t2);
    }
};

class WeatherChannelType : public Channel<Hal, List1, EmptyList, List4, PEERS_PER_WT_CHANNEL, AFTList0>, public Alarm {
    WeatherEventMsg     msg;
    bool                sensOK;
    uint16_t            millis;
    MAX6675<MAX6675_SCK, MAX6675_CS, MAX6675_SO> max6675;
    uint8_t             last_flags;
  public:
    WeatherChannelType () : Channel(), Alarm(5), sensOK(true), millis(0), last_flags(0xFF) {}
    virtual ~WeatherChannelType () {}

    virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
      tick = seconds2ticks(TEMPERATURE_MSG_INTERVAL);
      clock.add(*this);
      sensOK = max6675.measure();

      lcdDisplay.setTemperature(max6675.temperature());

      msg.init(device().nextcount(), sensOK ? max6675.temperature() : -10, false);

      device().broadcastEvent(msg);

      if (last_flags != flags()) {
        delay(400);
        last_flags = flags();
        this->changed(true);
      }

    }

    void setup(Device<Hal, AFTList0>* dev, uint8_t number, uint16_t addr) {
      Channel::setup(dev, number, addr);
      sysclock.add(*this);
      sensOK = max6675.init();
    }

    uint8_t status () const { return 0; }

    uint8_t flags  () const { return sensOK ? 0x00 : 0x01 << 1; }
};

class NoPWM {
public:
  void init(uint8_t __attribute__ ((unused)) pwm) {}
  void set(uint8_t __attribute__ ((unused)) pwm) {}
  void param(uint8_t __attribute__ ((unused)) speedMultiplier, uint8_t __attribute__ ((unused)) characteristic) {}
};

template<class HalType,class DimmerType,class PWM>
class StepperControl : public DimmerControl<HalType,DimmerType,PWM>, public Alarm {
  UserStorage     ustore;
  Nema17  nema17;
private:
  uint8_t lastphys;
  bool    first,disableAfterMove,gotNewMessage;

public:
  typedef DimmerControl<HalType,DimmerType,PWM> BaseControl;
  StepperControl (DimmerType& dim) : BaseControl(dim), Alarm(0),ustore(0), lastphys(0), first(true), disableAfterMove(true), gotNewMessage(false) { }

  virtual ~StepperControl () {}


  virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
    if (first == false) {
      lcdDisplay.enableBacklight(LCDDisplay::BLUE);
      nema17.drivePct(lastphys / 2);
    }
  }

  void setUserStorage(const UserStorage& storage) {
    ustore = storage;
  }

  void setNewMessage(bool b) {
    gotNewMessage = b;
  }

  void initNEMA17() {
    nema17.init();

    lcdDisplay.clearBuffer();
    lcdDisplay.setFont(u8g2_font_6x13_tr);
    const char * title PROGMEM = "INIT STEPPER";
    lcdDisplay.setCursor(lcdDisplay.centerPosition(title), 10);
    lcdDisplay.print(title);
    lcdDisplay.setCursor(25, 16);
    lcdDisplay.print("------------");
    lcdDisplay.setFont(u8g2_font_6x12_tr);
    lcdDisplay.setCursor(2, 24);

    if (ustore.getByte(4) == 0xd4) {
      const char * calib_found PROGMEM = "calib. found: ";
      lcdDisplay.print(calib_found);
      DPRINT(F("calibration data found: "));

      uint32_t savedSteps =
        ((uint32_t)(ustore.getByte(0)) << 24) +
        ((uint32_t)(ustore.getByte(1)) << 16) +
        ((uint32_t)(ustore.getByte(2)) << 8) +
        ((uint32_t)(ustore.getByte(3)));
      nema17.setSteps(savedSteps);

      lcdDisplay.print(String(savedSteps).c_str());
      DDECLN(savedSteps);
    } else {
      const char * no_calib_found PROGMEM = "no calib. found";
      const char * run_cal PROGMEM = "running calib... ";
      lcdDisplay.print(no_calib_found);
      lcdDisplay.setCursor(2, 34);
      lcdDisplay.print(run_cal);
      DPRINT(F("no calibration data found. running calibration - "));

      lcdDisplay.setCursor(2, 44);
      uint32_t calibValue = nema17.calibrate();
      if (calibValue != 0xFFFFFFFF) {
        ustore.setByte(0, (calibValue >> 24) & 0xff);
        ustore.setByte(1, (calibValue >> 16) & 0xff);
        ustore.setByte(2, (calibValue >> 8) & 0xff);
        ustore.setByte(3, (calibValue) & 0xff);
        ustore.setByte(4, 0xd4);
        lcdDisplay.print(String(calibValue).c_str());
        DPRINT(F("saving value "));DDECLN(calibValue);
      } else {
        lcdDisplay.print(" TIMEOUT");
        DPRINTLN(F("failed with timeout"));
      }
    }
  }

  bool run() {
    return nema17.run(disableAfterMove);
  }

  virtual void updatePhysical () {
    BaseControl::updatePhysical();
    uint8_t phys = this->physical[0];
    AFTList0    l0 = BaseControl::dimmer.getList0();
    DimmerList1 l1 = BaseControl::dimmer.dimmerChannel(1).getList1();
    disableAfterMove = l0.releaseAfterMove();

    if (lastphys != phys || gotNewMessage) {
      gotNewMessage = false;
      lcdDisplay.enableBacklight(LCDDisplay::BLUE);
      sysclock.cancel(*this);
      Alarm::set(decis2ticks(l1.statusInfoMinDly()*5) + millis2ticks(500));
      sysclock.add(*this);
      first = false;
    }

    lcdDisplay.setPosition(phys / 2);

    lastphys = phys;
  }
};


class StepperChannelType : public ActorChannel<Hal,DimmerList1,DimmerList3,PEERS_PER_CHANNEL,AFTList0,DimmerStateMachine> {
  uint8_t* phys;
protected:
  typedef ActorChannel<Hal,DimmerList1,DimmerList3,PEERS_PER_CHANNEL,AFTList0,DimmerStateMachine> BaseChannel;
private:
  bool gotNewMessage;
public:
  StepperChannelType () : BaseChannel(), phys(0), gotNewMessage(false) {}

  bool process (const ActionSetMsg& msg) {
    gotNewMessage = true;
    BaseChannel::set(msg.value(), msg.ramp(), msg.delay());
    return true;
  }

  bool process (const ActionCommandMsg& msg) {
    gotNewMessage = true;
    BaseChannel::process(msg);
    return true;
  }

  bool process (const RemoteEventMsg& msg) {
    gotNewMessage = true;
    BaseChannel::process(msg);
    return true;
  }

  void setPhysical(uint8_t& p) {
    phys = &p;
  }

  void patchStatus (Message& msg) {
    if( msg.length() == 0x0e ) {
      msg.length(0x0f);
      if( phys != 0 ) {
        msg.data()[3] = *phys;
      }
    }
  }

  bool checkNewMessage() {
    bool m = gotNewMessage;
    gotNewMessage = false;
    return m;
  }
};

class StepperWeatherDevice : public ChannelDevice<Hal, VirtBaseChannel<Hal, AFTList0>, 2, AFTList0> {
public:
  VirtChannel<Hal, StepperChannelType, AFTList0> ch1;
  VirtChannel<Hal, WeatherChannelType, AFTList0> ch2;
  public:
    typedef  ChannelDevice<Hal, VirtBaseChannel<Hal, AFTList0>, 2, AFTList0> DeviceType;
    StepperWeatherDevice (const DeviceInfo& info, uint16_t addr) : DeviceType(info, addr) {
      DeviceType::registerChannel(ch1, 1);
      DeviceType::registerChannel(ch2, 2);
    }
    virtual ~StepperWeatherDevice () {}

  static int const channelCount = 1;
  static int const virtualCount = 1;
  StepperChannelType& dimmerChannel(__attribute__ ((unused)) uint8_t c) { return ch1; }
  WeatherChannelType& weatherChannel() { return ch2; }
};

StepperWeatherDevice sdev(devinfo, 0x20);
ConfigButton<StepperWeatherDevice> cfgBtn(sdev);
StepperControl<Hal,StepperWeatherDevice,NoPWM> stepperControl(sdev);

void setup(void) {
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);

  stepperControl.init(hal,0);

  buttonISR(cfgBtn, CONFIG_BUTTON);
  sdev.initDone();

  bool hasMaster = (sdev.getList0().masterid().valid() == true);
  bool scValid   = (sdev.getConfigArea()      .valid() == true);
  lcdDisplay.startupScreen(hasMaster, scValid);

  _delay_ms(1500);

  stepperControl.setUserStorage(sdev.getUserStorage());
  stepperControl.initNEMA17();

  _delay_ms(1000);

  lcdDisplay.startCyclicUpdate();
}

void loop(void) {
  if (stepperControl.run() == false) {
    if (sdev.dimmerChannel(1).checkNewMessage() == true) {
      stepperControl.setNewMessage(true);
    }
    hal.runready();
    sdev.pollRadio();
  }
}
