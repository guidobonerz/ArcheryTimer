import os
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
from adafruit_display_shapes.roundrect import RoundRect
from adafruit_display_shapes.circle import Circle
from adafruit_bitmap_font import bitmap_font
from DFPlayer import DFPlayer

# from DFPlayerPro import DFPlayerPro

lang = "en"
dictionary = {"de": {"shoot-in": "Einschiessen",
                     "pause": "PAUSE",
                     "setup": "SETUP"},
              "en": {"shoot-in": "Shoot-In",
                     "pause": "PAUSE",
                     "setup": "SETUP"}}


bit_depth = 2
base_width = 64
base_height = 64
chain_across = 2
tile_down = 1
serpentine = True

width = base_width * chain_across
height = base_height * tile_down

isMaster = os.getenv("MASTER", "false").lower() == "true"
isSoundDisabled = os.getenv("DISABLE_SOUND", "false").lower() == "true"
displayNo = int(os.getenv("DISPLAY_NO", "0"))

shootInTimer = 45

if not isSoundDisabled:
    try:
        dfplayer = DFPlayer(busio.UART(
            board.TX, board.RX, baudrate=9600), volume=30)
        dfplayer.play(track=4)
    except:
        print("player failed")

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
    color=0xf39c12,
    scale=2,
    text=dictionary[lang]["pause"])
pauseInfo.x = 5
pauseInfo.y = 24
pauseBackground = RoundRect(
    0, 15, 128, 30, 2, fill=0x000000, outline=0x505050, stroke=2)

pauseGroup = displayio.Group()
pauseGroup.append(pauseBackground)
pauseGroup.append(pauseInfo)
pauseGroup.hidden = True

trafficLight = Rect(0, 0, 43, 64, fill=0xff0000)

tournamentGroup = displayio.Group()
tournamentGroup.hidden = False
tournamentGroup.append(counterLine)
tournamentGroup.append(player1)
tournamentGroup.append(passText)
tournamentGroup.append(player2)
tournamentGroup.append(trafficLight)
tournamentGroup.append(pauseGroup)

shootInText = adafruit_display_text.label.Label(
    terminalio.FONT,
    color=0x5dade2,
    scale=1,
    text=dictionary[lang]["shoot-in"])
shootInBounds = shootInText.bounding_box
shootInText.x = int(64-shootInBounds[2]/2)
shootInText.y = 54

bitmap = displayio.Bitmap(128,64, 2)
palette = displayio.Palette(2)
palette[0] = 0x000000
palette[1] = 0xff0000
palette.make_transparent(0)

canvas = displayio.TileGrid(bitmap, pixel_shader=palette)


shootInMinuteTimerBox = RoundRect(
    33, 44, 62, 5, 2, fill=0x000000, outline=0x505050, stroke=1)

shootInTime = adafruit_display_text.label.Label(
    seg7Font,
    color=0xffffff,
    text="{:02d}".format(shootInTimer))
shootInTimeBounds = shootInTime.bounding_box
shootInTime.x = int(64-shootInTimeBounds[2]/2)
shootInTime.y = 17

shootInGroup = displayio.Group()

shootInGroup.append(shootInTime)
shootInGroup.append(shootInMinuteTimerBox)
shootInGroup.append(shootInText)
shootInGroup.append(canvas)

setup = adafruit_display_text.label.Label(
    abcdFont,
    color=0xf39c12,
    scale=2,
    text=dictionary[lang]["setup"])
setup.x = 5
setup.y = 24
setupBackground = RoundRect(
    0, 15, 128, 30, 2, fill=0x000000, outline=0x505050, stroke=2)

setupGroup = displayio.Group()
setupGroup.hidden = False
setupGroup.append(setupBackground)
setupGroup.append(setup)

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

print("Connecting to WiFi...")
print("my IP addr:", wifi.radio.ipv4_address)
pool = socketpool.SocketPool(wifi.radio)

udp_host = str(wifi.radio.ipv4_address)
udp_port = 5005

sock = pool.socket(pool.AF_INET, pool.SOCK_DGRAM)
sock.bind((udp_host, udp_port))
sock.settimeout(0)

try:
    pool = socketpool.SocketPool(wifi.radio)
    ntp = adafruit_ntp.NTP(pool, tz_offset=1)
    i2c = board.I2C()
    rtc = adafruit_ds3231.DS3231(i2c)
    rtc.datetime = ntp.datetime
    print("time synced.")

except:
    print("unable to sync time.")


pattern = re.compile(r'(archery_timer_control)!(.*)')
signalColors = [0xffffff, 0xf4d03f, 0xff0000, 0x00ff00]

MAIN_VIEW = 10
TOURNAMENT_VIEW = 20
SHOOTIN_VIEW = 30
SETUP_VIEW = 40
RELAX_VIEW = 50

view = MAIN_VIEW

PHASE_RUN = 10
PHASE_STOP = 20
PHASE_IDLE = -1

phase = PHASE_IDLE

firstNumber = True
prepare = False
pause = False

setRemainingTime = False
remainingTime = 0
_prepareTime = 0
_actionTime = 0
_shootInTime = 45
warnTime = 0
timer = 0
participantGroups = 1
currentParticipantGroup = 1
passe = 0
currentTime = 0
targetTime = 0
direction = 1
actionCount = 0

signalVolume = 30
senderIp = ""
groupCount = 0
topic = ""


def showMainView():
    global view
    view = MAIN_VIEW
    display.root_group = mainGroup
    display.auto_refresh = True


def showTournamentView():
    global view
    view = TOURNAMENT_VIEW
    display.root_group = tournamentGroup
    display.auto_refresh = True


def showShootInView():
    global view
    view = SHOOTIN_VIEW
    display.root_group = shootInGroup
    display.auto_refresh = True


def showSetupView():
    global view
    view = SETUP_VIEW
    display.root_group = setupGroup
    display.auto_refresh = True


def showRelaxView():
    global view
    view = RELAX_VIEW
    display.root_group = sysGroup
    display.auto_refresh = True

def fillTimer():
    for x in range(34, 94):
        for y in range(45, 48):
            bitmap[x,y] = 1

def drawCountDown(sec):
    for y in range(45, 48):
        bitmap[94-(60-sec),y] = 0            

def sendResponse(command, displayNo, isMaster, values=None):
    global senderIp
    if isMaster:
        if (values):
            jsonPayload = {"name": command,
                           "displayNo": displayNo,
                           "values": values}
            udp_command = f"archery_timer_display!{json.dumps(jsonPayload)}"
            sock.sendto(udp_command, ("255.255.255.255", udp_port))
        else:
            jsonPayload = {"name": command,
                           "displayNo": displayNo}
            udp_command = f"archery_timer_display!{json.dumps(jsonPayload)}"
            sock.sendto(udp_command, (senderIp, udp_port))


def sendStatus(displayNo, isMaster, groupCurrent, passCurrent, groupCount, prepareCurrent, actionCurrent):
    if isMaster:
        s = f"archery_timer_display!status:{groupCurrent}:{
            passCurrent}:{groupCount}:{prepareCurrent}:{actionCurrent}"
        sock.sendto(s, (senderIp, udp_port))
fillTimer()

showMainView()

while True:
    try:
        udp_receivebuffer = bytearray(300)
        size, addr = sock.recvfrom_into(udp_receivebuffer)
        msg = udp_receivebuffer[:size].decode('utf-8')
        groups = pattern.match(msg)
        payload = groups.group(2)
        control = json.loads(payload)
        command = control["name"]
        senderIp = control["ip"]
        if command == "home" and phase == PHASE_IDLE:
            showMainView()
        elif command == "tournament" and phase == PHASE_IDLE:
            if not isSoundDisabled:
                dfplayer.set_volume(signalVolume)
            counterLine.text = "{:3d}".format(timer)
            showTournamentView()
        elif command == "setup" and phase == PHASE_IDLE:
            showSetupView()
        elif command == "relax" and phase == PHASE_IDLE:
            showRelaxView()
        elif command == "changeTopic" and phase == PHASE_IDLE:
            _prepareTime = control["values"]["prepareTime"]
            _arrowTime = control["values"]["arrowTime"]
            _shootInTime = 45#control["values"]["shootInTime"]
            _tournamentArrowCount = control["values"]["tournamentArrowCount"]
            _reshootArrowCount = control["values"]["reshootArrowCount"]
            warnTime = control["values"]["warnTime"]
            passes = control["values"]["passes"]
            participantGroups = control["values"]["mode"]
            topic = control["topic"]

            if topic == "shoot-in":
                showShootInView()
            elif topic == "tournament":
                _actionTime = _tournamentArrowCount*_arrowTime
                counterLine.text = "{:3d}".format(_actionTime)
                showTournamentView()
            elif topic == "re-shoot":
                _actionTime = _reshootArrowCount*_arrowTime
                counterLine.text = "{:3d}".format(_actionTime)
                showTournamentView()
            else:
                pass
        elif command == "toggle_action" and (view == TOURNAMENT_VIEW or view == SHOOTIN_VIEW):
            if phase == PHASE_IDLE:
                phase = PHASE_RUN
                prepare = True
                firstNumber = True
                groupCount = 1
             #   sendResponse("toggle_action", displayNo, isMaster)
            else:
                pause = not pause
            sendResponse("toggle_action", displayNo, isMaster)
        elif command == "stop" and (view == TOURNAMENT_VIEW or view == SHOOTIN_VIEW) and phase == PHASE_RUN:
            pause = False
            prepare = False
            phase = PHASE_STOP
        elif command == "emergency" and (view == TOURNAMENT_VIEW or view == SHOOTIN_VIEW) and phase == PHASE_RUN:
            pause = False
            prepare = False
            phase = PHASE_STOP
            # sendResponse("emergency", displayNo, isMaster)
        elif command == "reset" and (view == TOURNAMENT_VIEW or view == SHOOTIN_VIEW) and phase == PHASE_IDLE:
            phase = PHASE_IDLE
            prepare = True
            pause = False
            firstNumber = True
            timeDiff = 0
            timer = _prepareTime
            currentParticipantGroup = 1
            passe = 0
            groupCount = 1
            currentTime = 0
            targetTime = 0
            direction = 1
            actionCount = 0
            player1.text = "A"
            player2.text = "B"
            passText.text = "{:02d}".format(passe)
            counterLine.text = "{:3d}".format(_prepareTime)
            sendResponse("reset", displayNo, isMaster)
        elif command == "volume":
            signalVolume = control["values"]["value"]
            if not isSoundDisabled:
                dfplayer.set_volume(signalVolume)
        elif command == "testsignal" and view == SETUP_VIEW:
            if not isSoundDisabled:
                dfplayer.play(track=3)
        else:
            pass
    except:
        isTimeout = True

    if view == MAIN_VIEW:
        dateText = "{:02d}:{:02d}:{:02d}".format(
            rtc.datetime_register.tm_hour, rtc.datetime_register.tm_min, rtc.datetime_register.tm_sec)
        timeText = "{:02d}.{:02d}.{:02d}".format(
            rtc.datetime_register.tm_mday, rtc.datetime_register.tm_mon, rtc.datetime_register.tm_year)
        mainLine4.text = dateText
        mainLine5.text = timeText
    elif view == RELAX_VIEW:
        if firstNumber:
            targetTime = supervisor.ticks_ms()+5
            firstNumber = False
        if supervisor.ticks_ms() >= targetTime:
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
    elif view == SHOOTIN_VIEW:
        if phase == PHASE_RUN:
            if not pause:
                pauseGroup.hidden = True
                setRemainingTime = False
                if firstNumber:
                    timer = 60
                    firstNumber = False
                    targetTime = supervisor.ticks_ms()+1000
                    drawCountDown(timer)
                    shootInTime.text = "{:02d}".format(_shootInTime)
                else:
                    if supervisor.ticks_ms() >= targetTime:
                        targetTime = supervisor.ticks_ms()+1000
                        timer = timer-1
                        drawCountDown(timer)
                        if timer == 0:
                            _shootInTime -= 1
                            shootInTime.text = "{:02d}".format(_shootInTime)
                            fillTimer()
                            timer = 60
            else:
                pass
        elif phase == PHASE_STOP:  # stop phase
            pass
        else:
            pass
    elif view == TOURNAMENT_VIEW:
        if phase == PHASE_RUN:
            if not pause:
                pauseGroup.hidden = True
                setRemainingTime = False
                if prepare:
                    if firstNumber:
                        timer = _prepareTime
                        if actionCount % participantGroups == 0:
                            passe = passe+1
                        if not isSoundDisabled:
                            dfplayer.play(track=1)
                        trafficLight.fill = 0xff0000
                        tv = "{:3d}".format(timer)
                        passText.text = "{:02d}".format(passe)
                        counterLine.color = signalColors[0]
                        counterLine.text = tv
                        sendStatus(displayNo, isMaster, currentParticipantGroup,
                                   passe, groupCount, _prepareTime, _actionTime)
                        if currentParticipantGroup == 1:
                            player1.text = "A"
                            player2.text = "B"
                        elif currentParticipantGroup == 2:
                            player1.text = "C"
                            player2.text = "D"
                        firstNumber = False
                        time.sleep(1)
                        targetTime = supervisor.ticks_ms()+1000
                    else:
                        if supervisor.ticks_ms() >= targetTime:
                            targetTime = supervisor.ticks_ms()+1000
                            timer = timer-1
                            sendStatus(displayNo, isMaster, currentParticipantGroup,
                                       passe, groupCount, timer, _actionTime)
                            if timer < 0:
                                prepare = False
                                firstNumber = True
                                timer = _actionTime
                                sendStatus(
                                    displayNo, isMaster, currentParticipantGroup, passe, groupCount, 0, _actionTime)
                                if not isSoundDisabled:
                                    dfplayer.play(track=3)
                            else:
                                tv = "{:3d}".format(timer)
                                counterLine.text = tv
                else:  # action phase
                    if firstNumber:
                        if timer <= warnTime:
                            trafficLight.fill = 0xf1c40f
                        else:
                            trafficLight.fill = 0x00ff00
                        tv = "{:3d}".format(timer)
                        passText.text = "{:02d}".format(passe)
                        counterLine.color = signalColors[0]
                        counterLine.text = tv
                        firstNumber = False
                        time.sleep(1)
                        targetTime = supervisor.ticks_ms()+1000
                    else:
                        if supervisor.ticks_ms() >= targetTime:
                            if timer <= warnTime:
                                trafficLight.fill = 0xf1c40f
                            else:
                                trafficLight.fill = 0x00ff00
                            targetTime = supervisor.ticks_ms()+1000
                            timer = timer-1
                            tv = "{:3d}".format(timer)
                            counterLine.text = tv
                            if timer == 0:
                                phase = PHASE_STOP
                            sendStatus(
                                displayNo, isMaster, currentParticipantGroup, passe, groupCount, 0, timer)

            else:
                pauseGroup.hidden = False
                if not setRemainingTime:
                    remainingTime = targetTime - supervisor.ticks_ms()
                    setRemainingTime = True
                targetTime = supervisor.ticks_ms()+remainingTime

        elif phase == PHASE_STOP:  # stop phase
            trafficLight.fill = 0xff0000
            actionCount = actionCount+1
            if participantGroups == 2:
                currentParticipantGroup = currentParticipantGroup+direction
                if currentParticipantGroup > 2:
                    currentParticipantGroup = 2
                if currentParticipantGroup < 1:
                    currentParticipantGroup = 1
                if actionCount % 2 == 0:
                    direction = direction*-1

            if ((participantGroups == 2 and groupCount == participantGroups) or participantGroups == 1):
                phase = PHASE_IDLE
                if not isSoundDisabled:
                    dfplayer.play(track=2)
                groupCount = 1
                sendResponse("stop", displayNo, isMaster)
            else:
                phase = PHASE_RUN
                if not isSoundDisabled:
                    dfplayer.play(track=3)
                prepare = True
                firstNumber = True
                if (participantGroups == 2):
                    groupCount += 1
        else:
            pass
    else:
        pass
