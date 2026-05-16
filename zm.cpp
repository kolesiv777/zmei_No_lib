#include <iostream>      
#include <cstdlib>       // для rand(), srand()
#include <ctime>         // для time()
#include <unistd.h>      // для usleep(), read(), STDIN_FILENO
#include <termios.h>     // для управления терминалом (tcgetattr, tcsetattr)
#include <fcntl.h>       // для fcntl, O_NONBLOCK

/**Константы игры*/
const int w = 20;          // ширина поля (включая стены)
const int h = 20;          // высота поля
const int maxLen = 400;    // максимальная длина змейки (запас на всё поле)

/**Глобальные переменные*/
int x[maxLen], y[maxLen];  // координаты всех сегментов: x[0],y[0] - голова
int len;                   // текущая длина змейки (сколько сегментов активно)
int dir;                   // направление: 0 - вверх, 1 - вниз, 2 - влево, 3 - вправо
int foodX, foodY;          // координаты еды
int gameOver;              // флаг: 0 - игра идёт, 1 - проигрыш, 2 - победа
/*Смещения для каждого направления (используются для движения головы)*/
int dx[4] = {0, 0, -1, 1};
int dy[4] = {-1, 1, 0, 0};

/**Настройки терминала для неблокирующего ввода*/
struct termios orig_termios;   // для сохранения исходных настроек

// Переключение терминала в raw режим и неблокирующий ввод
void enableRawMode() {
    // Получаем текущие настройки терминала для stdin
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    // Отключаем канонический режим и эхо 
    raw.c_lflag &= ~(ICANON | ECHO);
    // Применяем новые настройки
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    // Устанавливаем неблокирующий режим чтения: read() сразу возвращает 0, если нет данных
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

// Восстановление исходных настроек терминала
void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    fcntl(STDIN_FILENO, F_SETFL, 0);   // снимаем флаг неблокирования
}

/*Вспомогательные функции*/

// Проверка, занята ли клетка (cx, cy) змейкой
bool isSnake(int cx, int cy) {
    for (int i = 0; i < len; i++)
        if (x[i] == cx && y[i] == cy) return true;
    return false;
}

// Генерация новой еды в случайном свободном месте внутри поля (не на стенах)
void generateFood() {
    int attempts = 0;
    do {
        foodX = rand() % (w - 2) + 1;   
        foodY = rand() % (h - 2) + 1;
        attempts++;
        // Если после 1000 попыток не нашли свободное место — всё поле заполнено, победа
        if (attempts > 1000) {
            gameOver = 2;
            return;
        }
    } while (isSnake(foodX, foodY));   // повторяем, если клетка занята змейкой
}

/*Инициализация игры*/
void setup() {
    len = 3;                           // начальная длина
    // Располагаем змейку горизонтально в центре: голова справа, тело и хвост слева
    x[0] = w / 2;     y[0] = h / 2;    // голова
    x[1] = w / 2 - 1; y[1] = h / 2;    // тело
    x[2] = w / 2 - 2; y[2] = h / 2;    // хвост
    dir = 3;                            // начинаем движение вправо
    gameOver = 0;
    srand(time(0));                     // инициализируем генератор случайных чисел
    generateFood();                     // создаём первую еду
}

/*Отрисовка игрового поля*/
void draw() {
    system("clear");   // очистка экрана
    for (int i = 0; i < h; i++) {        // строки (y)
        for (int j = 0; j < w; j++) {    // столбцы (x)
            if (i == 0 || i == h - 1 || j == 0 || j == w - 1) {
                printf("#");              // стена по краям
            } else if (i == y[0] && j == x[0]) {
                printf("@");              // голова
            } else if (i == foodY && j == foodX) {
                printf("*");              // еда
            } else {
                bool printed = false;
                // ищем, есть ли в этой клетке какой-либо сегмент тела
                for (int k = 1; k < len; k++) {
                    if (i == y[k] && j == x[k]) {
                        printf("o");
                        printed = true;
                        break;
                    }
                }
                if (!printed) printf(" "); // пустое место
            }
        }
        printf("\n");
    }
    printf("Score: %d\n", len - 3);   // счёт = количество съеденных кусочков
}

/*Обработка нажатий клавиш*/
void input() {
    char c;
    if (read(STDIN_FILENO, &c, 1) > 0) {   // если есть ввод
        switch (c) {
            case 'w': if (dir != 1) dir = 0; break;   // нельзя повернуть вверх, если движемся вниз
            case 's': if (dir != 0) dir = 1; break;
            case 'a': if (dir != 3) dir = 2; break;
            case 'd': if (dir != 2) dir = 3; break;
        }
    }
}

/*Основная логика игры (движение, столкновения, еда)*/
void logic() {
    // 1. Вычисляем новую позицию головы в соответствии с направлением
    int newX = x[0] + dx[dir];
    int newY = y[0] + dy[dir];

    // 2. Проверка столкновения со стеной (координаты внутри поля: от 1 до w-2, от 1 до h-2)
    if (newX <= 0 || newX >= w - 1 || newY <= 0 || newY >= h - 1) {
        gameOver = 1;   // проигрыш
        return;
    }

    // 3. Проверка, съедена ли еда
    if (newX == foodX && newY == foodY) {
        len++;   // увеличиваем длину
        if (len > maxLen) len = maxLen;   // защита от переполнения

        // Сдвигаем все сегменты змейки на одну позицию назад (кроме нового места головы)
        // Проходим от хвоста (len-1) до второго сегмента (1)
        for (int i = len - 1; i > 0; i--) {
            x[i] = x[i - 1];
            y[i] = y[i - 1];
        }
        // Устанавливаем голову на новое место (там, где была еда)
        x[0] = newX;
        y[0] = newY;

        // Генерируем новую еду
        generateFood();
    } else {
        // 4. Обычное движение (без еды)
        // Сдвигаем все сегменты (хвост отбрасывается)
        for (int i = len - 1; i > 0; i--) {
            x[i] = x[i - 1];
            y[i] = y[i - 1];
        }
        x[0] = newX;
        y[0] = newY;

        // 5. Проверка столкновения головы с собственным телом
        for (int i = 1; i < len; i++) {
            if (x[0] == x[i] && y[0] == y[i]) {
                gameOver = 1;
                return;
            }
        }
    }
}

/*Главная функция*/
int main() {
    enableRawMode();   // настраиваем терминал для мгновенного ввода без эха
    setup();           // инициализация игры

    // Главный игровой цикл
    while (!gameOver) {
        draw();                 // отрисовка поля
        input();                // обработка нажатий
        logic();                // обновление состояния
        usleep(200000);         // задержка 200 мс 
    }

    disableRawMode();   // восстанавливаем нормальный режим терминала
    system("clear");    // очищаем экран перед финальным сообщением

    // Вывод результата
    if (gameOver == 1)
        printf("Game Over! Your score: %d\n", len - 3);
    else
        printf("You win! Final score: %d\n", len - 3);

    return 0;
}