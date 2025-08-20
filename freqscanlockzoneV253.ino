// 引脚定义
const int analogInputPin = A0;                    // 主选择输入
const int pulseDurationInputPin = A1;             // 脉冲时长调节
const int regionSwitchPin = A6;                   // 区域切换引脚(模拟输入)
const int regionIndicatorAPin = A4;               // 区域A指示
const int regionIndicatorBPin = A5;               // 区域B指示
const int outputPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11}; // 输出引脚
const int numOutputPins = 10;
const int pulseOutputPin = 12;                    // 脉冲输出
const int randomButtonPin = 13;                   // 随机按钮
const int resetButtonPin = A2;                    // 复位按钮
const int lockIndicatorPin = A3;                  // 锁定指示
const int lockTriggerPin = A7;                    // 锁定触发(模拟输入)

// 状态变量
enum RegionMode { FULL, REGION_A, REGION_B };
RegionMode currentRegion = FULL;                  // 初始设为全区域
bool randomPinsState[numOutputPins] = {false};
bool lastOutputPinsState[numOutputPins] = {false};
bool isPulseActive = false;
unsigned long pulseStartTime = 0;
unsigned long pulseDuration = 100;                // 默认脉冲时间

// 锁定状态定义
#define UNLOCKED 0
#define LOCKED_HIGH 1
#define LOCKED_LOW 2
uint8_t pinLockState[numOutputPins] = {UNLOCKED}; // 每个引脚的锁定状态

// 按钮状态
bool lastRandomButtonState = HIGH;
bool lastResetButtonState = HIGH;
bool lastLockTriggerState = false;               // 上次锁定触发状态
float lastRegionSwitchVoltage = 0.0;
const float triggerThreshold = 3.7;              // 触发阈值电压(3.7V)
const unsigned long longPressDuration = 3000;    // 长按判定时间(3秒)
unsigned long lockTriggerTime = 0;               // 锁定触发时间戳

// 函数声明
void updateRegionIndicators();
void handleRandomButton();
void resetAllPins();
void resetRandomPins();
int getCurrentInterval();
void handleLockFunction(int interval);
void handleRegionSwitch();
void updatePulseDuration();
void checkOutputPinsState(int interval);
void updateOutputPins(int interval);
void updateLockIndicator(int interval);
void managePulse();
void printDebugInfo(int interval);
void triggerPulse();
void randomizePins();
unsigned long generateRandomSeed();

// 随机种子生成
unsigned long generateRandomSeed() {
    ADMUX = _BV(REFS1) | _BV(REFS0) | _BV(MUX3);
    delay(10);
    ADCSRA |= _BV(ADEN) | _BV(ADSC);
    while (ADCSRA & _BV(ADSC));
    int noise = ADC;
    
    uint32_t clockJitter = 0;
    for (uint8_t i = 0; i < 16; i++) {
        clockJitter = (clockJitter << 1) | (micros() & 0x1);
        delayMicroseconds(100);
    }
    
    return millis() + noise + clockJitter;
}

void setup() {
    randomSeed(generateRandomSeed());
    Serial.begin(9600);
    
    // 初始化输出引脚
    for (int i = 0; i < numOutputPins; i++) {
        pinMode(outputPins[i], OUTPUT);
        digitalWrite(outputPins[i], LOW);
    }
    
    // 初始化功能引脚
    pinMode(pulseOutputPin, OUTPUT);
    pinMode(randomButtonPin, INPUT_PULLUP);
    pinMode(resetButtonPin, INPUT_PULLUP);
    pinMode(lockTriggerPin, INPUT);
    pinMode(lockIndicatorPin, OUTPUT);
    pinMode(regionSwitchPin, INPUT);
    pinMode(regionIndicatorAPin, OUTPUT);
    pinMode(regionIndicatorBPin, OUTPUT);
    
    updateRegionIndicators();
    Serial.println("系统初始化完成");
}

void loop() {
    // 读取按钮状态
    bool currentRandomButtonState = digitalRead(randomButtonPin);
    bool currentResetButtonState = digitalRead(resetButtonPin);
    float currentRegionVoltage = analogRead(regionSwitchPin) * (5.0 / 1023.0);

    // 处理随机按钮（仅在reset为低电平时响应）
    if (currentResetButtonState == LOW && currentRandomButtonState == HIGH && lastRandomButtonState == LOW) {
        handleRandomButton();
    }

    // 修改后的复位逻辑：上升沿触发复位随机引脚
    if (currentResetButtonState == HIGH && lastResetButtonState == LOW) {
        resetRandomPins();
    }

    // 处理锁定触发和区域切换
    int interval = getCurrentInterval();
    handleLockFunction(interval);
    
    if (currentRegionVoltage >= triggerThreshold && lastRegionSwitchVoltage < triggerThreshold) {
        handleRegionSwitch();
    }

    // 更新状态
    lastRandomButtonState = currentRandomButtonState;
    lastResetButtonState = currentResetButtonState;
    lastRegionSwitchVoltage = currentRegionVoltage;

    // 更新输出
    updatePulseDuration();
    checkOutputPinsState(interval);
    updateOutputPins(interval);
    updateLockIndicator(interval);
    managePulse();
    printDebugInfo(interval);
}

// ========== 功能函数实现 ========== //

void updateRegionIndicators() {
    digitalWrite(regionIndicatorAPin, currentRegion != REGION_A);
    digitalWrite(regionIndicatorBPin, currentRegion != REGION_B);
}

void handleRandomButton() {
    randomizePins();
}

void randomizePins() {
    memset(randomPinsState, 0, sizeof(randomPinsState));
    int availablePins[numOutputPins];
    int numAvailable = 0;
    
    for (int i = 0; i < numOutputPins; i++) {
        if (pinLockState[i] != UNLOCKED) continue;
        
        switch (currentRegion) {
            case FULL:    availablePins[numAvailable++] = i; break;
            case REGION_A: if (i <= 4) availablePins[numAvailable++] = i; break;
            case REGION_B: if (i >= 5) availablePins[numAvailable++] = i; break;
        }
    }

    if (numAvailable > 0) {
        int numToSelect = random(1, min(5, numAvailable + 1));
        for (int i = 0; i < numToSelect; i++) {
            int randomIndex = random(0, numAvailable);
            randomPinsState[availablePins[randomIndex]] = true;
            availablePins[randomIndex] = availablePins[--numAvailable];
        }
    }
    Serial.println("随机选择已更新");
}

void resetAllPins() {
    for (int i = 0; i < numOutputPins; i++) {
        randomPinsState[i] = false;
        pinLockState[i] = UNLOCKED;
        digitalWrite(outputPins[i], LOW);
    }
    Serial.println("所有引脚已完全复位");
}

void resetRandomPins() {
    for (int i = 0; i < numOutputPins; i++) {
        if (pinLockState[i] == UNLOCKED) {
            randomPinsState[i] = false;
        }
    }
    Serial.println("随机选择已复位（锁定引脚保持状态）");
}

int getCurrentInterval() {
    int analogValue = analogRead(analogInputPin);
    return constrain(map(analogValue, 0, 1000, 0, numOutputPins - 1), 0, numOutputPins - 1);
}

void handleLockFunction(int currentInterval) {
    float currentVoltage = analogRead(lockTriggerPin) * (5.0 / 1023.0);
    bool currentState = (currentVoltage >= triggerThreshold);
    
    // 检测上升沿
    if(currentState && !lastLockTriggerState) {
        if(pinLockState[currentInterval] == UNLOCKED) {
            // 锁定高电平
            pinLockState[currentInterval] = LOCKED_HIGH;
            lockTriggerTime = millis();
            Serial.print("引脚 ");
            Serial.print(outputPins[currentInterval]);
            Serial.println(" 高电平锁定");
        } 
        else {
            // 解锁
            pinLockState[currentInterval] = UNLOCKED;
            Serial.print("引脚 ");
            Serial.print(outputPins[currentInterval]);
            Serial.println(" 已解锁");
        }
    }
    
    // 检查长按转换条件
    if(currentState && (pinLockState[currentInterval] == LOCKED_HIGH)) {
        if(millis() - lockTriggerTime >= longPressDuration) {
            pinLockState[currentInterval] = LOCKED_LOW;
            Serial.print("引脚 ");
            Serial.print(outputPins[currentInterval]);
            Serial.println(" 转换为低电平锁定");
        }
    }
    
    lastLockTriggerState = currentState;
}

void handleRegionSwitch() {
    currentRegion = static_cast<RegionMode>((currentRegion + 1) % 3);
    updateRegionIndicators();
    Serial.print("区域切换至: ");
    switch (currentRegion) {
        case FULL:    Serial.println("全区域"); break;
        case REGION_A: Serial.println("区域A (D2-D6)"); break;
        case REGION_B: Serial.println("区域B (D7-D11)"); break;
    }
}

void updatePulseDuration() {
    int analogValue = analogRead(pulseDurationInputPin);
    analogValue = constrain(analogValue, 50, 1000);
    pulseDuration = map(analogValue, 50, 1000, 20, 1000);
}

void checkOutputPinsState(int interval) {
    if (!isPulseActive && pinLockState[interval] != LOCKED_LOW) {
        for (int i = 0; i < numOutputPins; i++) {
            bool currentState = (i == interval || randomPinsState[i] || pinLockState[i] == LOCKED_HIGH);
            if (currentState && !lastOutputPinsState[i]) {
                triggerPulse();
                break;
            }
        }
    }
}

void updateOutputPins(int interval) {
    for (int i = 0; i < numOutputPins; i++) {
        bool shouldActive = false;
        
        switch(pinLockState[i]) {
            case LOCKED_HIGH:
                shouldActive = true;
                break;
            case LOCKED_LOW:
                shouldActive = false;
                break;
            case UNLOCKED:
                shouldActive = (i == interval || randomPinsState[i]);
                break;
        }
        
        digitalWrite(outputPins[i], shouldActive ? HIGH : LOW);
        lastOutputPinsState[i] = shouldActive;
    }
}

void updateLockIndicator(int currentInterval) {
    digitalWrite(lockIndicatorPin, pinLockState[currentInterval] != UNLOCKED);
}

void managePulse() {
    if (isPulseActive && (millis() - pulseStartTime >= pulseDuration)) {
        digitalWrite(pulseOutputPin, LOW);
        isPulseActive = false;
    }
}

void triggerPulse() {
    if (!isPulseActive) {
        digitalWrite(pulseOutputPin, HIGH);
        pulseStartTime = millis();
        isPulseActive = true;
    }
}

void printDebugInfo(int interval) {
    static unsigned long lastPrintTime = 0;
    if (millis() - lastPrintTime > 500) {
        bool a2State = digitalRead(resetButtonPin);
        bool d13State = digitalRead(randomButtonPin);

        int a0Value = analogRead(analogInputPin);
        float a0Voltage = a0Value * (5.0 / 1023.0);
        int a1Value = analogRead(pulseDurationInputPin);
        float a1Voltage = a1Value * (5.0 / 1023.0);
        int a6Value = analogRead(regionSwitchPin);
        float a6Voltage = a6Value * (5.0 / 1023.0);
        int a7Value = analogRead(lockTriggerPin);
        float a7Voltage = a7Value * (5.0 / 1023.0);

        Serial.print("A0:");
        Serial.print(a0Value);
        Serial.print("(");
        Serial.print(a0Voltage, 1);
        Serial.print("V) | A1:");
        Serial.print(a1Value);
        Serial.print("(");
        Serial.print(a1Voltage, 1);
        Serial.print("V) | A2:");
        Serial.print(a2State ? "HIGH" : "LOW");
        Serial.print(" | A6:");
        Serial.print(a6Value);
        Serial.print("(");
        Serial.print(a6Voltage, 1);
        Serial.print("V) | A7:");
        Serial.print(a7Value);
        Serial.print("(");
        Serial.print(a7Voltage, 1);
        Serial.print("V) | D13:");
        Serial.print(d13State ? "HIGH" : "LOW");
        Serial.print(" | 区间:");
        Serial.print(interval);
        Serial.print(" | 区域:");
        switch (currentRegion) {
            case FULL:    Serial.print("全"); break;
            case REGION_A: Serial.print("A"); break;
            case REGION_B: Serial.print("B"); break;
        }
        Serial.print(" | 脉冲:");
        Serial.print(pulseDuration);
        Serial.print("ms | 锁定:");
        for (int i = 0; i < numOutputPins; i++) {
            if (pinLockState[i] != UNLOCKED) {
                Serial.print(outputPins[i]);
                Serial.print(pinLockState[i] == LOCKED_HIGH ? "(H)" : "(L)");
                Serial.print(" ");
            }
        }
        Serial.println();
        lastPrintTime = millis();
    }
}