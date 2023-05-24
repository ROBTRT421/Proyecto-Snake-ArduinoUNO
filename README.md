# Video: https://youtube.com/shorts/U3pQNbivbO8?feature=share


# Tutorial
1. Obtenga las piezas, instale [Arduino IDE] (https://www.arduino.cc/en/Main/Software), instale los controladores para Arduino (si tiene un clon de Arduino Y está utilizando Windows)
2. Copie y pegue [el código](https://github.com/ROBTRT421/Proyecto-Snake/blob/main/SNAKE%20A5.ino.ino) en la IDE de Arduino
3. Instalar la librería 'LedControl' usando Arduino IDE [Library Manager](https://www.arduino.cc/en/Guide/Libraries#toc2)
4. Conecte todo _(vea el diagrama de cableado)_
5. Conecte su Arduino y selecciónelo en 'Tools > Board' y 'Tools > Port'
6. Carguielo a la placa
7. _(opcional)_ Modifica las variables, explora el código :guiño:


# Partes
Part Name            |      Amazon link       | Note
:------------------- | ---------------------- | :------------------------------------------------
Arduino UNO  (clone) | https://www.amazon.com.mx/Arduino-Org-A000066-R3-microcontrolador
LED Matrix           | https://www.amazon.com/s?k=arduino+matrix+MAX7219 | MAX7219 controlled 8x8 LED Matrix
Joystick             | https://www.amazon.com/s?k=arduino+joystick+breakout | 
Potentiometer        | https://www.amazon.com/s?k=10k+potentiometer | any 1k ohm to 100k ohm should be fine
Some wires           | https://www.amazon.com/s?k=arduino+wires | 12 wires needed
Breadboard           | https://www.amazon.com/s?k=arduino+breadboard | 



# Wiring diagram
Pin           | Arduino NANO or UNO
:------------ | :------------------
Matrix CLK    | 10
Matrix CS     | 11
Matrix DIN    | 12
Joystick X    | A2
Joystick Y    | A3
Potentiometer | A5

![wiring diagram]( https://github.com/ROBTRT421/Proyecto-Snake/blob/main/Plano%20Snake.png "wiring diagram")

El orden exacto de los pines para la matriz o el joystick puede ser diferente del que se muestra en la imagen, así que sea inteligente y use la tabla anterior.




