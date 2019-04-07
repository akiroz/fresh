
local Blynk = require("blynk.nodemcu")
local gpio = require("gpio")
local usrt = require("uart")
local pwm = require("pwm")
local i2c = require("i2c")
local u8g2 = require("u8g2")
local bit = require("bit")

local config = {
  auth = "bff46d9f11234072abb8f08aae805350",
  ssid = "eunis",
  pwd  = "aaaaaaaa",
}

local pin = {
    FAN_PWM     = 5,
    FAN_TACHO   = 6,
    OLED_SDA    = 1,
    OLED_SCL    = 2,
    BTN_UP      = 7,
    BTN_DOWN    = 8,
    BTN_MODE    = 4,
}

local vpin = {
    POWER           = "V0",
    MODE            = "V1",
    PM_1_0          = "V2",
    PM_2_5          = "V3",
    PM_10           = "V4",
    FAN_RPM         = "V5",
    FILTER_LIFE     = "V6",
    MANUAL_SPEED    = "V7",
    ALERT_LEVEL     = "V8",
}

local state = {
    on          = true,
    auto        = false,
    manualSpeed = 0,
    autoSpeed   = 0,
    alarmLevel  = 0,
    alarmSent   = false,
    fanRPM      = 0,
}

blynk = Blynk.new(config.auth, { heartbeat = 10 })

pwm.setup(pin.FAN_PWM, 25000, 100)
pwm.start(pin.FAN_PWM)

local tacho = tmr.create()
local tachoVal = 0
tacho:register(1, tmr.ALARM_AUTO, function() tachoVal = tachoVal+1 end)
gpio.mode(pin.FAN_TACHO, gpio.INT)
gpio.trig(pin.FAN_TACHO, "up", function()
    local started = tacho.state()
    if started then
        state.fanRPM = (60 * 1000) / (2 * tachoVal)
        tachoVal = 0
    else
        tacho.start()
    end
end)

gpio.mode(pin.BTN_UP, gpio.INPUT, gpio.PULLUP)
gpio.mode(pin.BTN_DOWN, gpio.INPUT, gpio.PULLUP)
gpio.mode(pin.BTN_MODE, gpio.INPUT, gpio.PULLUP)

uart.setup(0, 9600, 8, uart.PARITY_NONE, uart.STOPBITS_1, 0)
uart.on("data", 32, function(buf)
    if buf[1] == 0x42 and buf[2] == 0x4d then
        local pm_1_0 = bit.bor(bit.lshift(buf[5], 8), buf[6])
        local pm_2_5 = bit.bor(bit.lshift(buf[7], 8), buf[8])
        local pm_10  = bit.bor(bit.lshift(buf[9], 8), buf[10])
        blynk:virtualWrite(vpin.PM_1_0, pm_1_0)
        blynk:virtualWrite(vpin.PM_2_5, pm_2_5)
        blynk:virtualWrite(vpin.PM_10, pm_10)
    end
end, 0)

-- i2c.setup(0, pin.OLED_SDA, pin.OLED_SCL, i2c.SLOW)
-- local disp = u8g2.ssd1306_i2c_128x64_noname(0, 0x3c)
-- disp:clearBuffer()
-- disp:drawStr(10, 10, "Hello World")

wifi.setmode(wifi.STATION)
wifi.sta.config(config)
wifi.sta.connect(function()
    local sock = net.createConnection(net.TCP)
    sock:on("connection", function(s) blynk:connect(s) end)
    sock:connect(80, "blynk-cloud.com")
end)

local periodic = tmr.create()
periodic:alarm(1000, tmr.ALARM_AUTO, function()
    blynk:virtualWrite(vpin.FAN_RPM, state.fanRPM)
    blynk:run()
end)

