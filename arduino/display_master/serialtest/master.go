package main

import (
	"log"
	"unsafe"

	"go.bug.st/serial"
)

type command struct {
	source               uint8
	command              uint8
	view                 uint8
	passes               uint8
	prepareTime          uint8
	arrowCount           uint8
	reshootArrowCount    uint8
	warnTime             uint8
	groups               uint8
	flashingPrepareLight bool
}

func main() {

	mode := &serial.Mode{
		BaudRate: 9600,
	}

	port, err := serial.Open("COM26", mode)
	if err != nil {
		log.Fatal(err)
	}

	start := command{source: 0, command: 1, view: 0, prepareTime: 10, arrowCount: 3, reshootArrowCount: 1, warnTime: 30, groups: 1, flashingPrepareLight: true, passes: 20}
	const sz = int(unsafe.Sizeof(command{}))
	var asByteSlice []byte = (*(*[sz]byte)(unsafe.Pointer(&start)))[:]
	//_, err := port.Write([]byte("start\x00"))
	_, err = port.Write(asByteSlice)
	if err != nil {
		log.Fatal(err)
	}
	/*
			var app *tview.Application

			ports, err := serial.GetPortsList()
			if err != nil {
				log.Fatal(err)
			}
			if len(ports) == 0 {
				log.Fatal("No serial ports found!")
			}
			for _, port := range ports {
				fmt.Printf("Found port: %v\n", port)
			}

			mode := &serial.Mode{
				BaudRate: 9600,
			}

			port, err := serial.Open("COM26", mode)
			if err != nil {
				log.Fatal(err)
			}

			/*
				sendButton := tview.NewButton("Send").SetSelectedFunc(func() {
					_, err := port.Write([]byte("sdgsdfgsfgsfgsg\\0"))
					if err != nil {
						log.Fatal(err)
					}
					buff := make([]byte, 100)
					//for {
					n, err := port.Read(buff)
					if err != nil {
						log.Fatal(err)
						//break
					}
					if n == 0 {
						//break
					}

					//}
					textAreaOutput.SetText(fmt.Sprintf("%v", string(buff)+"\n"))
					//textAreaOutput.SetText(fmt.Sprintf("Sent %v bytes\n", n), true)
					textAreaInput.SetText("", true)
				})


		startButton := tview.NewButton("Start").SetSelectedFunc(func() {
			start := command{source: 0, command: 1, view: 0, prepareTime: 10, arrowCount: 30, reshootArrowCount: 1, warnTime: 30, groups: 1, flashingPrepareLight: true, passes: 20}
			const sz = int(unsafe.Sizeof(command{}))
			var asByteSlice []byte = (*(*[sz]byte)(unsafe.Pointer(&start)))[:]
			//_, err := port.Write([]byte("start\x00"))
			_, err := port.Write(asByteSlice)
			if err != nil {
				log.Fatal(err)
			}
		})

		stopButton := tview.NewButton("Stop").SetSelectedFunc(func() {
			_, err := port.Write([]byte("stop\x00"))
			if err != nil {
				log.Fatal(err)
			}
		})

		pauseButton := tview.NewButton("Pause").SetSelectedFunc(func() {
			_, err := port.Write([]byte("pause\x00"))
			if err != nil {
				log.Fatal(err)
			}
		})

		resetButton := tview.NewButton("Reset").SetSelectedFunc(func() {
			_, err := port.Write([]byte("reset\x00"))
			if err != nil {
				log.Fatal(err)
			}
		})

		appGrid := tview.NewGrid().
			SetColumns(40).
			SetRows(1, 1, 1, 1).
			AddItem(startButton, 0, 0, 1, 1, 1, 1, true).
			AddItem(stopButton, 1, 0, 1, 1, 1, 1, true).
			AddItem(pauseButton, 2, 0, 1, 1, 1, 1, true).
			AddItem(resetButton, 3, 0, 1, 1, 1, 1, true)

		app = tview.NewApplication()

		if err := app.SetRoot(appGrid, true).EnableMouse(true).Run(); err != nil {
			panic(err)
		}
	*/
}
