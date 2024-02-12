import sys
import board
import busio
import time
import wifi
import json
import socketpool
import displayio
import terminalio
import re
import adafruit_ntp
import adafruit_ds3231
import supervisor
import rgbmatrix
import framebufferio
import adafruit_display_text.label
from adafruit_display_shapes.rect import Rect
from adafruit_display_shapes.circle import Circle
from adafruit_bitmap_font import bitmap_font
from DFPlayer import DFPlayer

bit_depth = 3
base_width = 64
base_height = 64
chain_across = 2
tile_down = 1
serpentine = True

width = base_width * chain_across
height = base_height * tile_down

try:
    dfplayer = DFPlayer(busio.UART(board.TX, board.RX,
                        baudrate=9600), volume=50)
except:
    print("player failed")


dfplayer.play(track=4)


addr_pins = [board.MTX_ADDRA, board.MTX_ADDRB,
             board.MTX_ADDRC, board.MTX_ADDRD, board.MTX_ADDRE]
rgb_pins = [
    board.MTX_R1,
    board.MTX_G1,
    board.MTX_B1,
    board.MTX_R2,
    board.MTX_G2,
    board.MTX_B2
]
clock_pin = board.MTX_CLK
latch_pin = board.MTX_LAT
oe_pin = board.MTX_OE

displayio.release_displays()
matrix = rgbmatrix.RGBMatrix(
    width=width,
    height=height,
    bit_depth=bit_depth,
    rgb_pins=rgb_pins,
    addr_pins=addr_pins,
    clock_pin=clock_pin,
    latch_pin=latch_pin,
    output_enable_pin=oe_pin,
    tile=tile_down, serpentine=serpentine,
)

display = framebufferio.FramebufferDisplay(matrix, auto_refresh=False)
seg7Font = bitmap_font.load_font("/Digital-7Mono-65.bdf")
abcdFont = bitmap_font.load_font("/UpheavalTT-BRK--20.bdf")


counterLine = adafruit_display_text.label.Label(
    seg7Font,
    color=0xffffff,
    text="  0")
counterLine.x = 43
counterLine.y = 17

player1 = adafruit_display_text.label.Label(
    abcdFont,
    color=0x5dade2,
    scale=2,
    text="A")
player1.x = 45
player1.y = 48

passText = adafruit_display_text.label.Label(
    terminalio.FONT,
    color=0xd35400,
    scale=2,
    text="00")
passText.x = 75
passText.y = 55

player2 = adafruit_display_text.label.Label(
    abcdFont,
    color=0x5dade2,
    scale=2,
    text="B")
player2.x = 106
player2.y = 48

pauseInfo = adafruit_display_text.label.Label(
    abcdFont,
    color=0x5dade2,
    scale=2,
    text="PAUSE")
pauseInfo.x = 40
pauseInfo.y = 20


trafficLight = Rect(0, 0, 43, 64, fill=0xff0000)

tournamentGroup = displayio.Group()
tournamentGroup.hidden = False
tournamentGroup.append(counterLine)
tournamentGroup.append(player1)
tournamentGroup.append(passText)
tournamentGroup.append(player2)
tournamentGroup.append(trafficLight)


logoImage = displayio.OnDiskBitmap(open("/logo_small.bmp", "rb"))
logoTile = displayio.TileGrid(
    logoImage,
    pixel_shader=getattr(logoImage, 'pixel_shader',
                         displayio.ColorConverter()),
    tile_width=logoImage.width,
    tile_height=logoImage.height
)

mainLine1 = adafruit_display_text.label.Label(
    terminalio.FONT,
    color=0x5dade2,
    scale=1,
    text="BSV")
mainLine1.x = 58
mainLine1.y = 10

mainLine2 = adafruit_display_text.label.Label(
    terminalio.FONT,
    color=0x5dade2,
    scale=1,
    text="Eppinghoven")
mainLine2.x = 58
mainLine2.y = 22

mainLine3 = adafruit_display_text.label.Label(
    terminalio.FONT,
    color=0x5dade2,
    scale=1,
    text="1743 e.V.")
mainLine3.x = 58
mainLine3.y = 34

mainLine4 = adafruit_display_text.label.Label(
    terminalio.FONT,
    color=0xffffff,
    scale=1,
    text="          ")
mainLine4.x = 58
mainLine4.y = 49

mainLine5 = adafruit_display_text.label.Label(
    terminalio.FONT,
    color=0xffffff,
    scale=1,
    text="        ")
mainLine5.x = 58
mainLine5.y = 59

mainGroup = displayio.Group()
mainGroup.hidden = False
mainGroup.append(logoTile)
mainGroup.append(mainLine1)
mainGroup.append(mainLine2)
mainGroup.append(mainLine3)
mainGroup.append(mainLine4)
mainGroup.append(mainLine5)


colorConverter = displayio.ColorConverter()
targetImage1 = displayio.OnDiskBitmap(open("/target.bmp", "rb"))
targetTile1 = displayio.TileGrid(
    targetImage1,
    pixel_shader=getattr(targetImage1, 'pixel_shader', colorConverter),
    tile_width=targetImage1.width,
    tile_height=targetImage1.height
)


targetImage2 = displayio.OnDiskBitmap(open("/target.bmp", "rb"))
targetTile2 = displayio.TileGrid(
    targetImage2,
    pixel_shader=getattr(targetImage2, 'pixel_shader', colorConverter),
    tile_width=targetImage2.width,
    tile_height=targetImage2.height
)
targetTile1.x = 10
targetTile1.y = 10
targetTile2.x = 70
targetTile2.y = 10


xdir1 = -1
ydir1 = 1
xdir2 = -1
ydir2 = -1
xdir3 = 1
ydir3 = -1

sysGroup = displayio.Group()
sysGroup.hidden = False
sysGroup.append(targetTile1)
sysGroup.append(targetTile2)
# sysGroup.append(ball3)


display.root_group = mainGroup
display.auto_refresh = True


print("Connecting to WiFi...")
print("my IP addr:", wifi.radio.ipv4_address)
pool = socketpool.SocketPool(wifi.radio)

udp_host = str(wifi.radio.ipv4_address)
udp_port = 5005
# udp_buffer = bytearray(100)


sock = pool.socket(pool.AF_INET, pool.SOCK_DGRAM)
sock.bind((udp_host, udp_port))
sock.settimeout(0)
# pattern = re.compile(r'!([a-z]+)(:([A-D\-0]*))?(:([0-9]*))?(:([0-9]*))?!')
pattern = re.compile(r'(archery_timer)!(.*)')
signalColors = [0xffffff, 0xf4d03f, 0xff0000, 0x00ff00]


UNKOWN = -1
MAIN_VIEW = 0
PREPARE = 10
START_STOP = 20
PAUSE = 30
BREAK = 40
SYNC_TIME = 50
TOURNAMENT_VIEW = 60
RELAX_VIEW = 70

action = SYNC_TIME

preparePhase = False
runPhase = False
pausePhase = False
setRemainingTime = False
remainingTime = 0
prepareTimer = 0
timer = 0
participantGroups = 1
currentParticipantGroup = 1
passe = 0
currentTime = 0
targetTime = 0
firstNumber = True
direction = 1
actionCount = 0


def showMainView():
    display.root_group = mainGroup
    display.auto_refresh = True

while True:
    try:
        udp_buffer = bytearray(100)
        size, addr = sock.recvfrom_into(udp_buffer)
        msg = udp_buffer.decode('utf-8')
        groups = pattern.match(msg)
        payload = groups.group(2)
        control = json.loads(payload)
        command = control["name"]
        if command == "home" and not runPhase:
            action = MAIN_VIEW
            showMainView()
        elif command == "tournament" and not runPhase:
            dfplayer.set_volume(100)
            counterLine.text = "{:3d}".format(prepareTimer)
            display.root_group = tournamentGroup
            display.auto_refresh = True
            action = TOURNAMENT_VIEW
        elif command == "prepare" and not runPhase:
            prepareTimer = control["values"]["prepareTime"]
            timer = control["values"]["actionTime"]
            mode = control["values"]["mode"]
        elif command == "relax" and not runPhase:
            action = RELAX_VIEW
            display.root_group = sysGroup
            display.auto_refresh = True
        elif command == "start" and action != MAIN_VIEW and action != RELAX_VIEW:
            if not runPhase:
                if mode == "ABCD":
                    participantGroups = 2
                else:
                    participantGroups = 1
                action = START_STOP
                runPhase = True
                preparePhase = True
                firstNumber = True
            else:
                pausePhase = not pausePhase
        elif command == "pause" and action != MAIN_VIEW and action != RELAX_VIEW and runPhase:
            action = START_STOP
            pausePhase = True
        elif command == "stop" and action != MAIN_VIEW and action != RELAX_VIEW and runPhase:
            action = START_STOP
            pausePhase = False
            runPhase = False
            preparePhase = False
        elif command == "reset" and action != MAIN_VIEW and action != RELAX_VIEW:
            action = MAIN_VIEW
            preparePhase = False
            runPhase = False
            pausePhase = False
            timeDiff = 0
            prepareTimer = 0
            timer = 0
            participantGroups = 1
            currentParticipantGroup = 1
            passe = 0
            currentTime = 0
            targetTime = 0
            firstNumber = True
            direction = 1
            actionCount = 0
            showMainView()
        elif command == "volume":
            dfplayer.set_volume(control["values"]["value"])
        elif command == "testsignal" and action == TOURNAMENT_VIEW:
            dfplayer.play(track=3)
        else:
            pass
    except:
        isTimeout = True
    if action == SYNC_TIME and not runPhase:
        try:
            pool = socketpool.SocketPool(wifi.radio)
            ntp = adafruit_ntp.NTP(pool, tz_offset=1)
            i2c = board.I2C()
            rtc = adafruit_ds3231.DS3231(i2c)
            rtc.datetime = ntp.datetime
        except:
            print("unable to sync time")

        action = MAIN_VIEW
    if action == MAIN_VIEW and not runPhase:
        dateText = "{:02d}:{:02d}:{:02d}".format(
            rtc.datetime_register.tm_hour, rtc.datetime_register.tm_min, rtc.datetime_register.tm_sec)
        timeText = "{:02d}:{:02d}:{:02d}".format(
            rtc.datetime_register.tm_mday, rtc.datetime_register.tm_mon, rtc.datetime_register.tm_year)
        mainLine4.text = dateText
        mainLine5.text = timeText
    elif action == RELAX_VIEW and not runPhase:
        if firstNumber:
            targetTime = supervisor.ticks_ms()+5
            firstNumber = False
        if supervisor.ticks_ms() > targetTime:
            targetTime = supervisor.ticks_ms()+5
            firstNumber = True
            targetTile1.x = targetTile1.x+xdir1
            targetTile1.y = targetTile1.y+ydir1
            if targetTile1.x < 1 or targetTile1.x > 88:
                xdir1 = xdir1*-1
            if targetTile1.y < 1 or targetTile1.y > 24:
                ydir1 = ydir1*-1

            targetTile2.x = targetTile2.x+xdir2
            targetTile2.y = targetTile2.y+ydir2
            if targetTile2.x < 1 or targetTile2.x > 88:
                xdir2 = xdir2*-1
            if targetTile2.y < 1 or targetTile2.y > 24:
                ydir2 = ydir2*-1
    elif action == START_STOP:
        if runPhase:
            if not pausePhase:
                setRemainingTime = False
                if preparePhase:
                    if firstNumber:
                        if actionCount % participantGroups == 0:
                            passe = passe+1
                        dfplayer.play(track=1)
                        trafficLight.fill = 0xff0000
                        targetTime = supervisor.ticks_ms()+1000
                        tv = "{:3d}".format(prepareTimer)
                        passText.text = "{:02d}".format(passe)
                        counterLine.color = signalColors[0]
                        counterLine.text = tv
                        if currentParticipantGroup == 1:
                            player1.text = "A"
                            player2.text = "B"
                        elif currentParticipantGroup == 2:
                            player1.text = "C"
                            player2.text = "D"
                        firstNumber = False
                    if supervisor.ticks_ms() > targetTime:
                        targetTime = supervisor.ticks_ms()+1000
                        prepareTimer = prepareTimer-1
                        tv = "{:3d}".format(prepareTimer)
                        counterLine.text = tv
                        passText.text = "{:02d}".format(passe)
                        if prepareTimer == 0:
                            preparePhase = False
                            firstNumber = True
                            time.sleep(1)
                else:  # start phase
                    if timer <= 30:
                        trafficLight.fill = 0xf1c40f
                    else:
                        trafficLight.fill = 0x00ff00
                    if firstNumber:
                        dfplayer.play(track=2)
                        targetTime = supervisor.ticks_ms()+1000
                        tv = "{:3d}".format(timer)
                        passText.text = "{:02d}".format(passe)
                        counterLine.color = signalColors[0]
                        counterLine.text = tv
                        firstNumber = False
                    if supervisor.ticks_ms() > targetTime:
                        targetTime = supervisor.ticks_ms()+1000
                        timer = timer-1
                        tv = "{:3d}".format(timer)
                        passText.text = "{:02d}".format(passe)
                        counterLine.text = tv
                        if timer == 0:
                            runPhase = False
            else:
                if not setRemainingTime:
                    remainingTime = targetTime - supervisor.ticks_ms()
                    setRemainingTime = True
                targetTime = supervisor.ticks_ms()+remainingTime
                print("pause")
        if not runPhase:  # stop phase
            trafficLight.fill = 0xff0000
            dfplayer.play(track=3)
            actionCount = actionCount+1
            if participantGroups == 2:
                currentParticipantGroup = currentParticipantGroup+direction
                if currentParticipantGroup > 2:
                    currentParticipantGroup = 2
                if currentParticipantGroup < 1:
                    currentParticipantGroup = 1
                if actionCount % 2 == 0:
                    direction = direction*-1
            action = TOURNAMENT_VIEW
    else:
        pass
