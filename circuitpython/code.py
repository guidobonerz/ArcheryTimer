import sys
import board
import time
import wifi
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
from adafruit_display_shapes.roundrect import RoundRect
from adafruit_display_shapes.circle import Circle
from adafruit_bitmap_font import bitmap_font
from DFPlayer import DFPlayer

PLAYER_VOL   = 90
UNKOWN=-1
MAIN_VIEW=0
PREPARE=10
START=20
STOP=30
BREAK=40
SYNC_TIME=50
TIME=60
TOURNAMENT_VIEW=70
SYSTEM_VIEW=80


bit_depth = 1
base_width = 64
base_height = 64
chain_across = 2
tile_down = 1
serpentine = True

width = base_width * chain_across
height = base_height * tile_down

try:
    dfplayer = DFPlayer(busio.UART(board.TX,board.RX,baudrate=9600),volume=PLAYER_VOL)
except:
    print("player failed")


addr_pins = [board.MTX_ADDRA, board.MTX_ADDRB, board.MTX_ADDRC, board.MTX_ADDRD, board.MTX_ADDRE]
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
    abcdFont,#terminalio.FONT,
    color=0x5dade2,
    scale=2,
    text="A")
player1.x = 45
player1.y = 48

player2 = adafruit_display_text.label.Label(
    abcdFont,#terminalio.FONT,
    color=0x5dade2,
    scale=2,
    text="B")
player2.x = 106
player2.y = 48

trafficLight = RoundRect(0, 0, 43, 64, 3, fill=0xff0000)

tournamentGroup = displayio.Group()
tournamentGroup.hidden=True
tournamentGroup.append(counterLine)
tournamentGroup.append(player1)
tournamentGroup.append(player2)
tournamentGroup.append(trafficLight)



logoImage = displayio.OnDiskBitmap(open("/logo_small.bmp", "rb"))
logoTile = displayio.TileGrid(
    logoImage,
    pixel_shader=getattr(logoImage, 'pixel_shader', displayio.ColorConverter()),
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
mainGroup.hidden=False
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
targetTile1.x=10
targetTile1.y=10
targetTile2.x=70
targetTile2.y=10



xdir1=-1
ydir1=1
xdir2=-1
ydir2=-1
xdir3=1
ydir3=-1

sysGroup = displayio.Group()
sysGroup.hidden=False
sysGroup.append(targetTile1)
sysGroup.append(targetTile2)
#sysGroup.append(ball3)


display.root_group = mainGroup
display.auto_refresh = True

i2c = board.I2C()
rtc = adafruit_ds3231.DS3231(i2c)

print("Connecting to WiFi...")
print("my IP addr:", wifi.radio.ipv4_address)
pool = socketpool.SocketPool(wifi.radio)

udp_host = str(wifi.radio.ipv4_address)
udp_port = 5005
udp_buffer = bytearray(64)

sock = pool.socket(pool.AF_INET, pool.SOCK_DGRAM)
sock.bind((udp_host, udp_port))
sock.settimeout(0)
pattern = re.compile(r'!([a-z]+)(:([A-D\-0]*))?(:([0-9]*))?(:([0-9]*))?!')
signalColors = [0xffffff, 0xf4d03f, 0xff0000, 0x00ff00]

action=SYNC_TIME
prepareTimer=0
timer=0
rounds=0
passe=0
currentRound=0
currentTime=0
targetTime=0
isRunning=False
firstNumber=True

while True:
    try:
        size, addr = sock.recvfrom_into(udp_buffer)
        msg = udp_buffer.decode('utf-8')
        groups=pattern.match(msg)
        command=groups.group(1)
        print(command)
        if command=="main" and isRunning==False:
            action=MAIN_VIEW
            mainGroup.hidden=False
            tournamentGroup.hidden=True
            sysGroup.hidden=True
            display.root_group = mainGroup
            display.auto_refresh = True
        elif command=="tournament" and isRunning==False:
            action=TOURNAMENT_VIEW
            mainGroup.hidden=True
            tournamentGroup.hidden=False
            sysGroup.hidden=True
            display.root_group = tournamentGroup
            display.auto_refresh = True
        elif command=="system" and isRunning==False:
            action=SYSTEM_VIEW
            mainGroup.hidden=True
            tournamentGroup.hidden=True
            sysGroup.hidden=False
            display.root_group = sysGroup
            display.auto_refresh = True


        elif command=="start" and action==TOURNAMENT_VIEW and isRunning==False:
            mode = groups.group(3)
            if mode=="A-B":
                rounds=1
            elif mode=="A-B-C-D":
                rounds=2
            else:
                rounds=-1
            prepareTimer=int(groups.group(5))
            timer=int(groups.group(7))
            action=PREPARE
            isRunning=True
        elif command=="stop" and isRunning==True:
            isRunning=False
            action=STOP
        elif command=="break" and isRunning==False:
            action=BREAK
        elif command=="sync" and isRunning==False:
            action=SYNC_TIME
        elif command=="time":
            action=TIME
        elif command=="system":
            action=SYSTEM_VIEW
        else:
            action=MAIN_VIEW
    except:
        isTimeout=True

    if action==SYNC_TIME:
        pool = socketpool.SocketPool(wifi.radio)
        ntp = adafruit_ntp.NTP(pool, tz_offset=1)
        rtc.datetime=ntp.datetime
        action=MAIN_VIEW
    elif action==MAIN_VIEW and isRunning==False:
        dateText ="{:02d}:{:02d}:{:02d}".format(rtc.datetime_register.tm_hour,rtc.datetime_register.tm_min,rtc.datetime_register.tm_sec)
        timeText ="{:02d}:{:02d}:{:02d}".format(rtc.datetime_register.tm_mday,rtc.datetime_register.tm_mon,rtc.datetime_register.tm_year)
        mainLine4.text=dateText
        mainLine5.text=timeText
        action=MAIN_VIEW
    elif action==TOURNAMENT_VIEW:
        x=1
    elif action==SYSTEM_VIEW:
        if firstNumber==True :
            targetTime=supervisor.ticks_ms()+20
            firstNumber=False
        if supervisor.ticks_ms()>targetTime:
            targetTime=supervisor.ticks_ms()+20
            firstNumber=True
            targetTile1.x=targetTile1.x+xdir1
            targetTile1.y=targetTile1.y+ydir1
            if targetTile1.x<1 or targetTile1.x>88:
                xdir1=xdir1*-1
            if targetTile1.y<1 or targetTile1.y>24:
                ydir1=ydir1*-1

            targetTile2.x=targetTile2.x+xdir2
            targetTile2.y=targetTile2.y+ydir2
            if targetTile2.x<1 or targetTile2.x>88:
                xdir2=xdir2*-1
            if targetTile2.y<1 or targetTile2.y>24:
                ydir2=ydir2*-1





    elif action==PREPARE:
        if firstNumber==True :
						dfplayer.play(track=1)
            trafficLight.fill=0xff0000
            targetTime=time.monotonic()+1
            tv ="{:3d}".format(prepareTimer)
            passe=passe+1
            counterLine.color=signalColors[0]
            counterLine.text=tv

            if currentRound==0:
                x=1
                #matrixportal.set_text("A",2)
                #matrixportal.set_text("C",3)
            else:
                x=2
                #matrixportal.set_text("B",2)
                #matrixportal.set_text("D",3)

            firstNumber=False
        if time.monotonic()>targetTime:
            targetTime=time.monotonic()+1
            prepareTimer=prepareTimer-1
            tv ="{:3d}".format(prepareTimer)
            counterLine.text=tv
            if prepareTimer==0:
                action=START
                firstNumber=True
    elif action==START:
        if firstNumber==True:
						dfplayer.play(track=2)
            trafficLight.fill=0x00ff00
            targetTime=time.monotonic()+1
            tv ="{:3d}".format(timer)
            counterLine.color=signalColors[0]
            counterLine.text=tv
            if currentRound==0:
                x=1
                #matrixportal.set_text("A",2)
                #matrixportal.set_text("C",3)
            else:
                x=2
                #matrixportal.set_text("B",2)
                #matrixportal.set_text("D",3)

            firstNumber=False
        if time.monotonic()>targetTime:
            targetTime=time.monotonic()+1
            timer=timer-1
            tv ="{:3d}".format(timer)
            if timer<=30:
                trafficLight.fill=0xf1c40f
            counterLine.text=tv
            if timer==0:
								dfplayer.play(track=3)
                trafficLight.fill=0xff0000
                action=STOP
                isRunning=False
                firstNumber=True
                prepareTimer=int(groups.group(5))
                timer=int(groups.group(7))
    elif action==STOP:
        timer==0
        isRunning=False
        firstNumber=True
        action=TOURNAMENT_VIEW
    elif action==TIME and isRunning==False:
        print("clock")
    else:
        action=UNKOWN

    #asyncio.run(main())
