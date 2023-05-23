/* 
Derechos de autor (c) 2018 Ondrej Telka. (https://ondrej.xyz/)
 * 
 * Este programa es software libre: puedes redistribuirlo y/o modificarlo  
 * bajo los términos de la Licencia Pública General GNU publicada por  
 * La Free Software Foundation, versión 3.
 *
 * Este programa se distribuye con la esperanza de que sea útil, pero 
 * SIN NINGUNA GARANTÍA; sin siquiera la garantía implícita de 
 * COMERCIABILIDAD o IDONEIDAD PARA UN PROPÓSITO PARTICULAR. Ver el GNU 
 * Licencia Pública General para más detalles.
 *
 * Debería haber recibido una copia de la Licencia Pública General de GNU 
 * junto con este programa. Si no es así, consulte <http://www.gnu.org/licenses/>.
 */

#include "LedControl.h" // La biblioteca LedControl se utiliza para controlar una matriz LED. Encuéntrela usando "Library Manager" o descargue zip aquí: https://github.com/wayoda/LedControl


// --------------------------------------------------------------- //
// ------------------ Configuración de usuario ------------------- //
// --------------------------------------------------------------- //

// Aqui definidos todos los pines
struct Pin {
	static const short joystickX = A2;   // Pin del eje X del joystick
	static const short joystickY = A3;   // Pin del eje Y del joystick
	static const short joystickVCC = 15;
  static const short joystickGND = 14;
  //static const short joystick5v =  // Energia para el joystick 
	//static const short joystickGND = GND; // GND virtual para el joystick 
  //static const short joystick SW = AT; // Solo aterrizado
	
  static const short potentiometer = A5; // Potenciómetro para control de velocidad de serpiente

	static const short CLK = 10;   // reloj para matriz LED
	static const short CS  = 11;  // chip-select para matriz LED
	static const short DIN = 12; // entrada de datos para matriz LED
};

// Brillo de la matriz LED: entre 0 (más oscuro) y 15 (más brillante)
const short intensity = 8;

// Lento = Rápido para el control de desplazamiento de los mensajes
const short messageSpeed = 5;

// longitud inicial de la serpiente (1...63, recomendado 3)
const short initialSnakeLength = 3;


void setup() {
	Serial.begin(115200);  // establecer la misma velocidad en baudios en el monitor serie
	initialize();         // inicializar pines y matriz LED
	calibrateJoystick(); // Calibre el joystick Home (no lo toque)
	showSnakeMessage(); // desplaza el mensaje 'snake' alrededor de la matriz
}


void loop() {
	generateFood();    // Si no hay comida, genera una
	scanJoystick();    // Observa los movimientos del joystick y parpadea con la comida
	calculateSnake();  // Calcula los parámetros de la serpiente
	handleGameStates();

	// Enmarca esto si quieres que el tablero de juego actual se imprima en el serial (ralentiza un poco el juego)
	// dumpGameBoard();
}





// --------------------------------------------------------------- //
// --------------------- soporte variables ----------------------- //
// --------------------------------------------------------------- //

LedControl matrix(Pin::DIN, Pin::CLK, Pin::CS, 1);

struct Point {
	int row = 0, col = 0;
	Point(int row = 0, int col = 0): row(row), col(col) {}
};

struct Coordinate {
	int x = 0, y = 0;
	Coordinate(int x = 0, int y = 0): x(x), y(y) {}
};

bool win = false;
bool gameOver = false;

// Coordenadas primarias de cabeza de serpiente (cabeza de serpiente), se generarán aleatoriamente
Point snake;

// La comida aún no está en ninguna parte
Point food(-1, -1);

// Construir con valores predeterminados en caso de que el usuario desactive la calibración
Coordinate joystickHome(500, 500);

// Parámetros de serpiente
int snakeLength = initialSnakeLength; // Elegido por el usuario en la sección Configuración
int snakeSpeed = 1; // se establecerá de acuerdo con el valor del potenciómetro, no puede ser 0
int snakeDirection = 0; // Si es 0, la serpiente no se mueve

// constantes de dirección
const short up     = 1;
const short right  = 2;
const short down   = 3; // 'abajo - 2' debe ser 'arriba'
const short left   = 4; // 'izquierda - 2' debe ser 'derecha'

// umbral donde se aceptará el movimiento del joystick
const int joystickThreshold = 160;

// logaritmo artificial (inclinación) del potenciómetro (-1 = lineal, 1 = natural, más grande = más pronunciado (recomendado 0...1))
const float logarithmity = 0.4;

// Almacenamiento de segmentos corporales de serpiente
int gameboard[8][8] = {};




// --------------------------------------------------------------- //
// -------------------------- funciones -------------------------- //
// --------------------------------------------------------------- //


// Si no hay comida, genere una, también verifique la victoria
void generateFood() {
	if (food.row == -1 || food.col == -1) {
		// evidente
		if (snakeLength >= 64) {
			win = true;
			return; // Evitar que el generador de alimentos funcione, en este caso funcionaría para siempre, porque no podrá encontrar un píxel sin una serpiente
		}

		// Generar alimentos hasta que estén en la posición correcta
		do {
			food.col = random(8);
			food.row = random(8);
		} while (gameboard[food.row][food.col] > 0);
	}
}


// Observa los movimientos del joystick y parpadea con la comida
void scanJoystick() {
	int previousDirection = snakeDirection; // Guardar la última dirección
	long timestamp = millis();

	while (millis() < timestamp + snakeSpeed) {
		// Calcule la velocidad de la serpiente exponencialmente (10...1000ms)
		float raw = mapf(analogRead(Pin::potentiometer), 0, 1023, 0, 1);
		snakeSpeed = mapf(pow(raw, 3.5), 0, 1, 10, 1000); // Cambiar la velocidad exponencialmente
		if (snakeSpeed == 0) snakeSpeed = 1; // Seguridad: la velocidad no puede ser 0

		// determinar la dirección de la serpiente
		analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold ? snakeDirection = up    : 0;
		analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold ? snakeDirection = down  : 0;
		analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold ? snakeDirection = left  : 0;
		analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold ? snakeDirection = right : 0;

		// Ignorar el cambio de dirección en 180 grados (sin efecto para que la serpiente no se mueva)
		snakeDirection + 2 == previousDirection && previousDirection != 0 ? snakeDirection = previousDirection : 0;
		snakeDirection - 2 == previousDirection && previousDirection != 0 ? snakeDirection = previousDirection : 0;

		// Parpadea inteligentemente con la comida
		matrix.setLed(0, food.row, food.col, millis() % 100 < 50 ? 1 : 0);
	}
}


// Calcular los datos de movimiento de la serpiente
void calculateSnake() {
	switch (snakeDirection) {
		case up:
			snake.row--;
			fixEdge();
			matrix.setLed(0, snake.row, snake.col, 1);
			break;

		case right:
			snake.col++;
			fixEdge();
			matrix.setLed(0, snake.row, snake.col, 1);
			break;

		case down:
			snake.row++;
			fixEdge();
			matrix.setLed(0, snake.row, snake.col, 1);
			break;

		case left:
			snake.col--;
			fixEdge();
			matrix.setLed(0, snake.row, snake.col, 1);
			break;

		default: // Si la serpiente no se mueve, salga
			return;
	}

	// Si se encuentra con un segmento del cuerpo de la serpiente, esto causará el final del juego (la serpiente debe estar en movimiento)
	if (gameboard[snake.row][snake.col] > 1 && snakeDirection != 0) {
		gameOver = true;
		return;
	}

	// Verifique si la comida fue consumida
	if (snake.row == food.row && snake.col == food.col) {
		food.row = -1; // reset food
		food.col = -1;

		// Incrementar la longitud de la serpiente
		snakeLength++;

		// Incrementar todos los segmentos del cuerpo de la serpiente
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				if (gameboard[row][col] > 0 ) {
					gameboard[row][col]++;
				}
			}
		}
	}

	// Agrega nuevo segmento en la ubicación de la cabeza de la serpiente
	gameboard[snake.row][snake.col] = snakeLength + 1; // será decrementado en un momento

	// Decremento Todos los segmentos del cuerpo de la serpiente, si el segmento es 0, apague el LED correspondiente
	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {
			// Si hay un segmento del cuerpo, disminuya su valor
			if (gameboard[row][col] > 0 ) {
				gameboard[row][col]--;
			}

			// 
			matrix.setLed(0, row, col, gameboard[row][col] == 0 ? 0 : 1);
		}
	}
}


// hace que la serpiente aparezca en el otro lado de la pantalla si sale del borde
void fixEdge() {
	snake.col < 0 ? snake.col += 8 : 0;
	snake.col > 7 ? snake.col -= 8 : 0;
	snake.row < 0 ? snake.row += 8 : 0;
	snake.row > 7 ? snake.row -= 8 : 0;
}


void handleGameStates() {
	if (gameOver || win) {
		unrollSnake();

		showScoreMessage(snakeLength - initialSnakeLength);

		if (gameOver) showGameOverMessage();
		else if (win) showWinMessage();

		// Vuelve a iniciar el juego
		win = false;
		gameOver = false;
		snake.row = random(8);
		snake.col = random(8);
		food.row = -1;
		food.col = -1;
		snakeLength = initialSnakeLength;
		snakeDirection = 0;
		memset(gameboard, 0, sizeof(gameboard[0][0]) * 8 * 8);
		matrix.clearDisplay(0);
	}
}


void unrollSnake() {
	// switch off the food LED
	matrix.setLed(0, food.row, food.col, 0);

	delay(800);

	// Parpadea la pantalla 5 veces
	for (int i = 0; i < 5; i++) {
		// Invertir la pantalla
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				matrix.setLed(0, row, col, gameboard[row][col] == 0 ? 1 : 0);
			}
		}

		delay(20);

		// Invertirlo de nuevo
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				matrix.setLed(0, row, col, gameboard[row][col] == 0 ? 0 : 1);
			}
		}

		delay(50);

	}


	delay(600);

	for (int i = 1; i <= snakeLength; i++) {
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				if (gameboard[row][col] == i) {
					matrix.setLed(0, row, col, 0);
					delay(100);
				}
			}
		}
	}
}


// Calibre el joystick Home 10 veces
void calibrateJoystick() {
	Coordinate values;

	for (int i = 0; i < 10; i++) {
		values.x += analogRead(Pin::joystickX);
		values.y += analogRead(Pin::joystickY);
	}

	joystickHome.x = values.x / 10;
	joystickHome.y = values.y / 10;
}


void initialize() {
	pinMode(Pin::joystickVCC, OUTPUT);
	digitalWrite(Pin::joystickVCC, HIGH);

	pinMode(Pin::joystickGND, OUTPUT);
	digitalWrite(Pin::joystickGND, LOW);

	matrix.shutdown(0, false);
	matrix.setIntensity(0, intensity);
	matrix.clearDisplay(0);

	randomSeed(analogRead(A5));
	snake.row = random(8);
	snake.col = random(8);
}


void dumpGameBoard() {
	String buff = "\n\n\n";
	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {
			if (gameboard[row][col] < 10) buff += " ";
			if (gameboard[row][col] != 0) buff += gameboard[row][col];
			else if (col == food.col && row == food.row) buff += "@";
			else buff += "-";
			buff += " ";
		}
		buff += "\n";
	}
	Serial.println(buff);
}





// --------------------------------------------------------------- //
// -------------------------- mensajes --------------------------- //
// --------------------------------------------------------------- //

const PROGMEM bool snakeMessage[8][57] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};


const PROGMEM bool gameOverMessage[8][90] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

const PROGMEM bool scoreMessage[8][58] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

const PROGMEM bool digits[][8][8] = {
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 1, 1, 1, 0},
		{0, 1, 1, 1, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 1, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 1, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 0, 0, 0, 1, 1, 0, 0},
		{0, 0, 1, 1, 0, 0, 0, 0},
		{0, 1, 1, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 1, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 0, 0, 1, 1, 1, 0, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 1, 1, 0, 0},
		{0, 0, 0, 1, 1, 1, 0, 0},
		{0, 0, 1, 0, 1, 1, 0, 0},
		{0, 1, 0, 0, 1, 1, 0, 0},
		{0, 1, 1, 1, 1, 1, 1, 0},
		{0, 0, 0, 0, 1, 1, 0, 0},
		{0, 0, 0, 0, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 1, 0},
		{0, 1, 1, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 0, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 0, 0, 1, 1, 0, 0},
		{0, 0, 0, 0, 1, 1, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 1, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	}
};


// desplaza el mensaje 'snake' alrededor de la matriz
void showSnakeMessage() {
	[&] {
		for (int d = 0; d < sizeof(snakeMessage[0]) - 7; d++) {
			for (int col = 0; col < 8; col++) {
				delay(messageSpeed);
				for (int row = 0; row < 8; row++) {
					// lee el byte del PROGMEM y lo muestra en la pantalla
					matrix.setLed(0, row, col, pgm_read_byte(&(snakeMessage[row][col + d])));
				}
			}

			// Si se mueve el joystick, salga del mensaje
			if (analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold
			        || analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold
			        || analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold
			        || analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold) {
				return; // Devolver la función de Lambda
			}
		}
	}();

	matrix.clearDisplay(0);

	// Espere a que vuelva el joystick
	while (analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold
	        || analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold
	        || analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold
	        || analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold) {}

}


// desplaza el mensaje 'Game Over' alrededor de la matriz
void showGameOverMessage() {
	[&] {
		for (int d = 0; d < sizeof(gameOverMessage[0]) - 7; d++) {
			for (int col = 0; col < 8; col++) {
				delay(messageSpeed);
				for (int row = 0; row < 8; row++) {
					// lee el byte del PROGMEM y lo muestra en la pantalla
					matrix.setLed(0, row, col, pgm_read_byte(&(gameOverMessage[row][col + d])));
				}
			}

			// Si se mueve el joystick, salga del mensaje
			if (analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold
			        || analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold
			        || analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold
			        || analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold) {
				return; // Devolver la función de Lambda
			}
		}
	}();

	matrix.clearDisplay(0);

	// Espere a que vuelva el joystick co
	while (analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold
	        || analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold
	        || analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold
	        || analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold) {}

}


// desplaza el mensaje 'win' alrededor de la matriz
void showWinMessage() {
	// aún no implementado // TAREAS: implementarlo
}


// desplaza el mensaje 'puntuación' con números alrededor de la matriz
void showScoreMessage(int score) {
	if (score < 0 || score > 99) return;

	// Especificar dígitos de puntuación
	int second = score % 10;
	int first = (score / 10) % 10;

	[&] {
		for (int d = 0; d < sizeof(scoreMessage[0]) + 2 * sizeof(digits[0][0]); d++) {
			for (int col = 0; col < 8; col++) {
				delay(messageSpeed);
				for (int row = 0; row < 8; row++) {
					if (d <= sizeof(scoreMessage[0]) - 8) {
						matrix.setLed(0, row, col, pgm_read_byte(&(scoreMessage[row][col + d])));
					}

					int c = col + d - sizeof(scoreMessage[0]) + 6; // move 6 px in front of the previous message

					// Si la puntuación es < 10, desplace el primer dígito (cero)
					if (score < 10) c += 8;

					if (c >= 0 && c < 8) {
						if (first > 0) matrix.setLed(0, row, col, pgm_read_byte(&(digits[first][row][c]))); // show only if score is >= 10 (see above)
					} else {
						c -= 8;
						if (c >= 0 && c < 8) {
							matrix.setLed(0, row, col, pgm_read_byte(&(digits[second][row][c]))); // Mostrar siempre
						}
					}
				}
			}

			// Si se mueve el joystick, salga del mensaje
			if (analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold
			        || analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold
			        || analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold
			        || analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold) {
				return; // Devolver la función de Lambda
			}
		}
	}();

	matrix.clearDisplay(0);

	//	// Espere a que vuelva el joystick
	//	while (analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold
	//	        || analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold
	//	        || analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold
	//	        || analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold) {}

}


// Función de mapa estándar, pero con flotadores
float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}