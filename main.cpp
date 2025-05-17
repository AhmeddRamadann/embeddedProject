/**
 * @Project : Embedded final project (CSE211s)
 * @Board   : NUCLEO-F401RE + Arduino Multifunction Shield
 * @Authors : Mostafa Samy, 
              Ahmed Ramadan,
              Sherif Ahmed
 * @Date    : 17-May-2025 
 */

/******** /LIB ********/
#include "mbed.h"
#include <cstdint>

/******** /CONSTANTS MACROS ********/
#define DIGIT_REFRESH_DELAY_MS  4
#define DP_POS                  1

/******** /CONSTANTS ********/
const float VREF = 3.3;         // Reference voltage
const uint8_t digitBits[10] = {
    0b11000000, // 0
    0b11111001, // 1
    0b10100100, // 2
    0b10110000, // 3
    0b10011001, // 4
    0b10010010, // 5
    0b10000010, // 6
    0b11111000, // 7
    0b10000000, // 8
    0b10010000  // 9
};  // 7-segment display bits for digits 0-9
const uint8_t digitPos[4] = {0x01, 0x02, 0x04, 0x08};   // Digit positions for 7-segment display


/******** /PIN-MODE DEFINITIONS ********/
DigitalOut latchPin(D4);
DigitalOut clockPin(D7);
DigitalOut dataPin(D8);

InterruptIn resetBtn(A1);   // S1
InterruptIn modeBtn(A3);    // S3

AnalogIn pot(A0);           // Potentiometer for voltage measurement

/******** /GLOBAL VARIABLES ********/
Ticker timerTicker;
Ticker dispTicker;

volatile int seconds = 0, minutes = 0;
volatile bool showVolt = false;
volatile int currentDigit = 0;

float minVoltage = 3.3, maxVoltage = 0.0;

/******** /Functions declarations ********/
void shiftOutMSBFirst(uint8_t value);                   // Shift out 8 bits MSB first
void writeToShiftRegister(uint8_t bits, uint8_t digit); // Write to shift register
void updateTime();                                      // Update time every second
void updateDisplay();                                   // Update display every 4ms
void resetTimeISR();                                    // Reset time ISR interrupt
void toggleVoltModeISR();                               // Toggle voltage mode ISR interrupt
void releaseVoltModeISR();                              // Release voltage mode ISR interrupt

/** 
 * @defgroup Functions definitions
 * @brief Functions definitions for the timer and display
 * @details The functions are used to control the 7-segment display and update the time and handles the interrupts from the switches on the shield board.
 * @{   
 */

/**
 * @brief Shift out 8 bits MSB first
 * @param value @type: uint8_t 
 * The value to be shifted out
 * 
 * @note The function shifts out the bits of the value one by one, starting from the most significant bit (MSB).
 *       The data is sent to the shift register using the dataPin, and the clockPin is used to synchronize the data transfer.
 */
void shiftOutMSBFirst(uint8_t value)
{
    for (int i = 7; i >= 0; i--) {
        dataPin = (value >> i) & 1;
        clockPin = 1;
        clockPin = 0;
    }
}

/**
 * @brief Write to shift register
 * @param bits @type: uint8_t
 * The bits to be written to the shift register
 * @param digit @type: uint8_t
 * The digit position to which the bits will be written
 * 
 * @note The function first sets the latchPin to 0, then shifts out the bits and the digit position to the shift register.
 *      Finally, it sets the latchPin to 1 to update the output of the shift register.
 */
void writeToShiftRegister(uint8_t bits, uint8_t digit)
{
    latchPin = 0;
    shiftOutMSBFirst(bits);
    shiftOutMSBFirst(digit);
    latchPin = 1;
}

/**
 * @brief Update time every second
 * 
 * @note The function increments the seconds variable and checks if it has reached 60.
 *       If so, it resets seconds to 0 and increments minutes. The minutes variable is limited to 99.
 */
void updateTime()
{
    seconds++;
    if (seconds >= 60) {
        seconds = 0;
        minutes = (minutes + 1) % 100;
    }
}

/**
 * @brief Update display every 4ms
 * 
 * @note The function updates the display with the current time or voltage value.
 *       It uses the digitBits array to get the corresponding bits for each digit and the digitPos array to set the digit position.
 *       The function also handles the decimal point (DP) position and updates the min and max voltage values.
 */
void updateDisplay()
{
    int dispVal = 0;
    
    int digits[4] = {
        (dispVal / 1000) % 10,
        (dispVal / 100) % 10,
        (dispVal / 10) % 10,
        (dispVal) % 10
    };

    for (int i = 0; i < 4; ++i) {
        uint8_t bits = digitBits[digits[i]];
        if (showVolt && i == DP_POS) {
            bits &= ~0x80;                      // Enable DP
            float voltage = pot.read() * VREF;
            minVoltage = fmin(minVoltage, voltage);
            maxVoltage = fmax(maxVoltage, voltage);
            dispVal = (voltage * 100);          // X.XX → XX.X format
        }
        writeToShiftRegister(bits, digitPos[i]);
        ThisThread::sleep_for(DIGIT_REFRESH_DELAY_MS);
    }
}

/**
 * @defgroup Interrupts handlers
 * @brief Interrupts handlers for the switches on the shield board
 * @details The functions are used to handle the interrupts from the switches on the shield board.
 * @{
 */

/**
 * @brief Reset time ISR interrupt handler
 * 
 * @note The function resets the seconds and minutes variables to 0.
 */
void resetTimeISR()
{
    seconds = 0;
    minutes = 0;
}

/**
 * @brief Toggle voltage mode ISR interrupt handler
 * 
 * @note The function sets the showVolt variable to true, indicating that the voltage mode is active.
 */
void toggleVoltModeISR()
{
    showVolt = true;
}

/**
 * @brief Release voltage mode ISR interrupt handler
 * 
 * @note The function sets the showVolt variable to false, indicating that the voltage mode is inactive.
 */
void releaseVoltModeISR()
{
    showVolt = false;
}
/** @} End of Interrupts handlers group */
/** @} End of Functions definitions group */


/**
 * @defgroup Main function
 * @note The function initializes the ISRs and starts the periodic tasks using Tickers.
 *       It then enters an infinite loop, where all tasks are handled by ISRs and Tickers.
 */
int main()
{
    // Attach ISRs to buttons
    resetBtn.rise(&resetTimeISR);       // S1 Pressed
    modeBtn.fall(&toggleVoltModeISR);   // S3 Pressed
    modeBtn.rise(&releaseVoltModeISR);  // S3 Released

    // Initialize Tickers
    timerTicker.attach(&updateTime, 1s);    // Update time every second
    dispTicker.attach(&updateDisplay, 4ms); // Update display every 4ms

    while (true) {
        // Everything handled by ISRs and Tickers
        ThisThread::sleep_for(50ms);
    }
}
/** @} End of Main function group */
// End of file