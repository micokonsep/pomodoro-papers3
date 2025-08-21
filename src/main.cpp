#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <M5Unified.h>
#include <M5GFX.h>
#include <esp_ota_ops.h>

// Forward declarations
void drawButton(int x, int y, int w, int h, String text, uint32_t color);
void drawIconButton(int x, int y, int w, int h, String icon, uint32_t color);
void drawRefreshButton(int x, int y, int w, int h, uint32_t color);
void drawSettingsButton(int x, int y, int w, int h, uint32_t color);
void drawBatteryIcon(int x, int y, int percentage, bool charging);
void updateBatteryInfo();
void displayLockScreen();
void redrawAllButtons();

// Global variables for animation
int timerCenterX, timerCenterY;
int outerRadius = 220;
int innerRadius = 170;
int currentSecond = 0;
int currentMinute = 0;
unsigned long lastSecondUpdate = 0;
bool animationRunning = false;
bool timerPaused = false;
int timerDuration = 25; // Default 25 minutes

// Deep sleep variables
unsigned long lastActivityTime = 0;
const unsigned long SLEEP_TIMEOUT = 5 * 60 * 1000; // 5 minutes in milliseconds

// Battery monitoring variables
unsigned long lastBatteryCheck = 0;
const unsigned long BATTERY_CHECK_INTERVAL = 60 * 1000; // Check every 60 seconds
int batteryLevel = 0;
bool isCharging = false;

// SD Card pin definitions for PaperS3
#define SD_SPI_CS_PIN 47
#define SD_SPI_SCK_PIN 39
#define SD_SPI_MOSI_PIN 38
#define SD_SPI_MISO_PIN 40

// SD Card variables
bool sdCardInitialized = false;

// Button positions
struct Button {
  int x, y, w, h;
  String type;
  String label;
};

Button buttons[7];

void drawCircularTimer(int centerX, int centerY, int minutes) {
  // Draw outer circle with 60 dots (seconds) - all start as black
  for (int i = 0; i < 60; i++) {
    float angle = (i * 6) * PI / 180; // 6 degrees per dot (360/60)
    int x = centerX + (outerRadius * cos(angle - PI/2));
    int y = centerY + (outerRadius * sin(angle - PI/2));
    M5.Display.fillCircle(x, y, 4, TFT_BLACK);
  }
  
  // Draw inner circle with correct number of dots based on timer duration
  for (int i = 0; i < minutes; i++) {
    float angle = (i * (360.0 / minutes)) * PI / 180; // Divide 360 by number of minutes
    int x = centerX + (innerRadius * cos(angle - PI/2));
    int y = centerY + (innerRadius * sin(angle - PI/2));
    M5.Display.fillCircle(x, y, 6, TFT_BLACK);
  }
  
  // Draw time text
  M5.Display.setTextSize(6);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.drawString(String(minutes), centerX, centerY - 15);
  
  M5.Display.setTextSize(3);
  M5.Display.drawString("min", centerX, centerY + 25);
}

void updateOuterDot(int dotIndex, uint32_t color) {
  // Update only one specific dot
  float angle = (dotIndex * 6) * PI / 180; // 6 degrees per dot (360/60)
  int x = timerCenterX + (outerRadius * cos(angle - PI/2));
  int y = timerCenterY + (outerRadius * sin(angle - PI/2));
  M5.Display.fillCircle(x, y, 4, color);
}

void updateInnerDot(int dotIndex, uint32_t color) {
  // Update only one specific inner dot
  float angle = (dotIndex * (360.0 / timerDuration)) * PI / 180; // Divide 360 by timer duration
  int x = timerCenterX + (innerRadius * cos(angle - PI/2));
  int y = timerCenterY + (innerRadius * sin(angle - PI/2));
  M5.Display.fillCircle(x, y, 6, color);
}

void startAnimation() {
  animationRunning = true;
  timerPaused = false;
  currentSecond = 0;
  currentMinute = 0;
  lastSecondUpdate = millis();
}

void pauseAnimation() {
  timerPaused = true;
}

void resumeAnimation() {
  timerPaused = false;
  lastSecondUpdate = millis();
}

void stopAnimation() {
  animationRunning = false;
  timerPaused = false;
  currentSecond = 0;
  currentMinute = 0;
  
  // Reset all dots to black
  for (int i = 0; i < 60; i++) {
    updateOuterDot(i, TFT_BLACK);
  }
  for (int i = 0; i < timerDuration; i++) {
    updateInnerDot(i, TFT_BLACK);
  }
}

void updateAnimation() {
  if (!animationRunning || timerPaused) return;
  
  unsigned long currentTime = millis();
  if (currentTime - lastSecondUpdate >= 1000) { // Every 1 second
    // Change current outer dot from black to white (reverse/counterclockwise)
    int dotIndex = 59 - currentSecond; // Start from dot 59 (top) and go backwards
    updateOuterDot(dotIndex, TFT_WHITE);
    currentSecond++;
    
    // Check if 60 seconds completed (1 minute)
    if (currentSecond >= 60) {
      currentSecond = 0; // Reset seconds
      currentMinute++;
      
      // Reset all outer dots to black for next minute
      for (int i = 0; i < 60; i++) {
        updateOuterDot(i, TFT_BLACK);
      }
      
      // Update inner dot for completed minute
      if (currentMinute <= timerDuration) {
        int innerDotIndex = (timerDuration - 1) - (currentMinute - 1); // Start from last dot (top) and go backwards
        updateInnerDot(innerDotIndex, TFT_WHITE);
      }
      
      // Full refresh every 5 minutes to prevent ghosting
      if (currentMinute % 5 == 0) {
        M5.Display.fillScreen(TFT_WHITE);
        drawCircularTimer(timerCenterX, timerCenterY, timerDuration);
        
        // Redraw title text
        int titleY = timerCenterY + 300;
        M5.Display.setTextSize(4);
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setTextDatum(MC_DATUM);
        M5.Display.drawString("POMODORO", timerCenterX, titleY);
        M5.Display.setTextSize(2);
        M5.Display.drawString("epaper", timerCenterX, titleY + 35);
        
        // Redraw all buttons
        redrawAllButtons();
        
        // Redraw battery info
        drawBatteryIcon(10, 10, batteryLevel, isCharging);
        
        // Redraw all completed dots
        for (int i = 0; i < currentSecond; i++) {
          int dotIndex = 59 - i;
          updateOuterDot(dotIndex, TFT_WHITE);
        }
        for (int i = 0; i < currentMinute; i++) {
          int innerDotIndex = (timerDuration - 1) - i;
          updateInnerDot(innerDotIndex, TFT_WHITE);
        }
      }
      
      // Check if timer duration completed
      if (currentMinute >= timerDuration) {
        // Animation complete, reset everything
        animationRunning = false;
        currentSecond = 0;
        currentMinute = 0;
        
        // Reset activity time so user gets full 10 minutes before sleep
        lastActivityTime = millis();
        
        // Pomodoro finished sound - longer beep
        M5.Speaker.tone(600, 500);
        delay(1000);
        M5.Speaker.tone(600, 500);
        delay(1000);
        M5.Speaker.tone(600, 500);
        delay(1000);
        M5.Speaker.tone(600, 500);
        
        // Reset all dots to black
        for (int i = 0; i < 60; i++) {
          updateOuterDot(i, TFT_BLACK);
        }
        for (int i = 0; i < timerDuration; i++) {
          updateInnerDot(i, TFT_BLACK);
        }
      }
    }
    
    lastSecondUpdate = currentTime;
  }
}

void drawButton(int x, int y, int w, int h, String text, uint32_t color) {
  // Draw button outline only (no fill)
  M5.Display.drawRoundRect(x, y, w, h, 8, TFT_BLACK);
  
  // Draw button text
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.drawString(text, x + w/2, y + h/2);
}

void drawIconButton(int x, int y, int w, int h, String icon, uint32_t color) {
  // Draw button outline only (no fill)
  M5.Display.drawRoundRect(x, y, w, h, 8, TFT_BLACK);
  
  int centerX = x + w/2;
  int centerY = y + h/2;
  
  if (icon == "play") {
    // Draw play triangle
    M5.Display.fillTriangle(centerX - 8, centerY - 10, centerX - 8, centerY + 10, centerX + 8, centerY, TFT_BLACK);
  } else if (icon == "pause") {
    // Draw pause bars
    M5.Display.fillRect(centerX - 8, centerY - 10, 5, 20, TFT_BLACK);
    M5.Display.fillRect(centerX + 3, centerY - 10, 5, 20, TFT_BLACK);
  } else if (icon == "stop") {
    // Draw stop square
    M5.Display.fillRect(centerX - 8, centerY - 8, 16, 16, TFT_BLACK);
  }
}

void drawRefreshButton(int x, int y, int w, int h, uint32_t color) {
  // Draw small circular button outline
  M5.Display.drawCircle(x + w/2, y + h/2, w/2 - 2, TFT_BLACK);
  
  int centerX = x + w/2;
  int centerY = y + h/2;
  int radius = w/2 - 6;
  
  // Draw refresh arrow (circular arrow)
  // Top arc
  for (int i = 0; i < 270; i += 10) {
    float angle1 = i * PI / 180;
    float angle2 = (i + 10) * PI / 180;
    int x1 = centerX + radius * cos(angle1);
    int y1 = centerY + radius * sin(angle1);
    int x2 = centerX + radius * cos(angle2);
    int y2 = centerY + radius * sin(angle2);
    M5.Display.drawLine(x1, y1, x2, y2, TFT_BLACK);
  }
  
  // Arrow head
  M5.Display.fillTriangle(centerX + radius - 2, centerY - 6, 
                          centerX + radius - 2, centerY + 2, 
                          centerX + radius + 4, centerY - 2, TFT_BLACK);
}

void drawBatteryIcon(int x, int y, int percentage, bool charging) {
  // Battery outline (48x24 pixels - 2x size)
  M5.Display.drawRect(x, y, 48, 24, TFT_BLACK);
  M5.Display.fillRect(x + 48, y + 6, 4, 12, TFT_BLACK); // Battery tip
  
  // Clear battery interior
  M5.Display.fillRect(x + 2, y + 2, 44, 20, TFT_WHITE);
  
  // Battery fill based on percentage
  int fillWidth = (percentage * 44) / 100;
  if (fillWidth > 0) {
    M5.Display.fillRect(x + 2, y + 2, fillWidth, 20, TFT_BLACK);
  }
  
  // Charging indicator (lightning bolt) - 2x size
  if (charging) {
    M5.Display.drawLine(x + 20, y - 16, x + 28, y - 16, TFT_BLACK);
    M5.Display.drawLine(x + 24, y - 20, x + 24, y - 12, TFT_BLACK);
    M5.Display.drawLine(x + 22, y - 18, x + 26, y - 14, TFT_BLACK);
  }
  
  // Battery percentage text - 2x size
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.drawString(String(percentage) + "%", x + 60, y + 4);
}

void updateBatteryInfo() {
  unsigned long currentTime = millis();
  if (currentTime - lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
    batteryLevel = M5.Power.getBatteryLevel();
    isCharging = M5.Power.isCharging();
    lastBatteryCheck = currentTime;
    
    // Only update display if not during timer animation to avoid flicker
    if (!animationRunning || timerPaused) {
      // Clear previous battery display area (larger for 2x size)
      M5.Display.fillRect(10, 10, 140, 40, TFT_WHITE);
      // Redraw battery info
      drawBatteryIcon(10, 10, batteryLevel, isCharging);
    }
  }
}

void redrawAllButtons() {
  // Redraw all buttons
  drawIconButton(buttons[0].x, buttons[0].y, buttons[0].w, buttons[0].h, "play", TFT_WHITE);
  drawIconButton(buttons[1].x, buttons[1].y, buttons[1].w, buttons[1].h, "pause", TFT_WHITE);
  drawIconButton(buttons[2].x, buttons[2].y, buttons[2].w, buttons[2].h, "stop", TFT_WHITE);
  drawButton(buttons[3].x, buttons[3].y, buttons[3].w, buttons[3].h, "25Min", TFT_WHITE);
  drawButton(buttons[4].x, buttons[4].y, buttons[4].w, buttons[4].h, "5Min", TFT_WHITE);
  drawButton(buttons[5].x, buttons[5].y, buttons[5].w, buttons[5].h, "30Min", TFT_WHITE);
  drawRefreshButton(buttons[6].x, buttons[6].y, buttons[6].w, buttons[6].h, TFT_WHITE);
}

void handleButtonPress(int buttonIndex) {
  // Update last activity time
  lastActivityTime = millis();
  
  // Visual feedback - briefly fill button when pressed
  M5.Display.fillRoundRect(buttons[buttonIndex].x, buttons[buttonIndex].y, 
                           buttons[buttonIndex].w, buttons[buttonIndex].h, 8, TFT_LIGHTGRAY);
  delay(200);
  
  switch (buttonIndex) {
    case 0: // Play button
      {
        M5.Speaker.tone(800, 100); // Buzz sound for play
        if (!animationRunning) {
          startAnimation();
          // Force full refresh by clearing and redrawing everything
          M5.Display.fillScreen(TFT_WHITE);
          drawCircularTimer(timerCenterX, timerCenterY, timerDuration);
          // Redraw title text
          int titleY = timerCenterY + 300;
          M5.Display.setTextSize(4);
          M5.Display.setTextColor(TFT_BLACK);
          M5.Display.setTextDatum(MC_DATUM);
          M5.Display.drawString("POMODORO", timerCenterX, titleY);
          M5.Display.setTextSize(2);
          M5.Display.drawString("epaper", timerCenterX, titleY + 35);
          // Redraw all buttons
          redrawAllButtons();
          // Redraw battery info
          drawBatteryIcon(10, 10, batteryLevel, isCharging);
        } else if (timerPaused) {
          resumeAnimation();
        }
      }
      break;
    case 1: // Pause button
      M5.Speaker.tone(600, 100); // Buzz sound for pause
      if (animationRunning && !timerPaused) {
        pauseAnimation();
      }
      break;
    case 2: // Stop button
      {
        M5.Speaker.tone(400, 100); // Buzz sound for stop
        stopAnimation();
        // Force full refresh by clearing and redrawing everything
        M5.Display.fillScreen(TFT_WHITE);
        drawCircularTimer(timerCenterX, timerCenterY, timerDuration);
        // Redraw title text
        int titleY = timerCenterY + 300;
        M5.Display.setTextSize(4);
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setTextDatum(MC_DATUM);
        M5.Display.drawString("POMODORO", timerCenterX, titleY);
        M5.Display.setTextSize(2);
        M5.Display.drawString("epaper", timerCenterX, titleY + 35);
        // Redraw all buttons
        redrawAllButtons();
        // Redraw battery info
        drawBatteryIcon(10, 10, batteryLevel, isCharging);
      }
      break;
    case 3: // 25Min button
      timerDuration = 25;
      stopAnimation();
      // Redraw entire timer display with new duration
      M5.Display.fillScreen(TFT_WHITE);
      drawCircularTimer(timerCenterX, timerCenterY, timerDuration);
      M5.Display.display(); // Full refresh when changing preset
      break;
    case 4: // 5Min button
      timerDuration = 5;
      stopAnimation();
      // Redraw entire timer display with new duration
      M5.Display.fillScreen(TFT_WHITE);
      drawCircularTimer(timerCenterX, timerCenterY, timerDuration);
      M5.Display.display(); // Full refresh when changing preset
      break;
    case 5: // 30Min button
      timerDuration = 30;
      stopAnimation();
      // Redraw entire timer display with new duration
      M5.Display.fillScreen(TFT_WHITE);
      drawCircularTimer(timerCenterX, timerCenterY, timerDuration);
      M5.Display.display(); // Full refresh when changing preset
      break;
    case 6: // Refresh button
      {
        M5.Speaker.tone(500, 100); // Buzz sound for refresh
        
        // Triple refresh anti-ghosting sequence
        // Step 1: Full black screen
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.display();
        delay(100);
        
        // Step 2: Full white screen
        M5.Display.fillScreen(TFT_WHITE);
        M5.Display.display();
        delay(100);
        
        // Step 3: Draw actual content
        drawCircularTimer(timerCenterX, timerCenterY, timerDuration);
        
        // Redraw title text
        int titleY = timerCenterY + 300;
        M5.Display.setTextSize(4);
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setTextDatum(MC_DATUM);
        M5.Display.drawString("POMODORO", timerCenterX, titleY);
        M5.Display.setTextSize(2);
        M5.Display.drawString("epaper", timerCenterX, titleY + 35);
        
        // Redraw all buttons
        redrawAllButtons();
        
        // Redraw battery info
        drawBatteryIcon(10, 10, batteryLevel, isCharging);
        
        // Redraw all completed dots if timer is running
        if (animationRunning) {
          for (int i = 0; i < currentSecond; i++) {
            int dotIndex = 59 - i;
            updateOuterDot(dotIndex, TFT_WHITE);
          }
          for (int i = 0; i < currentMinute; i++) {
            int innerDotIndex = (timerDuration - 1) - i;
            updateInnerDot(innerDotIndex, TFT_WHITE);
          }
        }
        
        M5.Display.display(); // Final display refresh
      }
      break;
  }
  
  // Redraw all buttons after feedback (needed for preset buttons that clear screen)
  if (buttonIndex >= 3 && buttonIndex <= 5) { // If preset button was pressed, redraw all buttons
    // Redraw title text
    int titleY = timerCenterY + 300;
    M5.Display.setTextSize(4);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString("POMODORO", timerCenterX, titleY);
    M5.Display.setTextSize(2);
    M5.Display.drawString("epaper", timerCenterX, titleY + 35);
    
    // Redraw all buttons
    redrawAllButtons();
    
    // Redraw battery info
    drawBatteryIcon(10, 10, batteryLevel, isCharging);
  } else if (buttonIndex != 6) { // Skip redraw for refresh button since it does full refresh
    // Just redraw the pressed button
    if (buttons[buttonIndex].type == "icon") {
      // Clear the button area first
      M5.Display.fillRoundRect(buttons[buttonIndex].x, buttons[buttonIndex].y, buttons[buttonIndex].w, buttons[buttonIndex].h, 8, TFT_WHITE);
      drawIconButton(buttons[buttonIndex].x, buttons[buttonIndex].y, buttons[buttonIndex].w, buttons[buttonIndex].h, buttons[buttonIndex].label, TFT_WHITE);
    } else {
      drawButton(buttons[buttonIndex].x, buttons[buttonIndex].y, buttons[buttonIndex].w, buttons[buttonIndex].h, buttons[buttonIndex].label, TFT_WHITE);
    }
  }
}

void checkButtonTouch() {
  if (M5.Touch.getCount() > 0) {
    auto touch = M5.Touch.getDetail(0);
    if (touch.wasPressed()) {
      lastActivityTime = millis(); // Update activity time on any touch
      Serial.printf("==> Touch detected at (%d, %d)\n", touch.x, touch.y);
      
      for (int i = 0; i < 7; i++) {
        if (touch.x >= buttons[i].x && touch.x <= buttons[i].x + buttons[i].w &&
            touch.y >= buttons[i].y && touch.y <= buttons[i].y + buttons[i].h) {
          
          Serial.printf("==> Button %d touched: %s\n", i, buttons[i].label.c_str());
          
          // Handle all buttons through handleButtonPress
          handleButtonPress(i);
          break;
        }
      }
    }
  }
}

void displayLockScreen() {
  if (sdCardInitialized) {
    // Clear screen first
    M5.Display.fillScreen(TFT_WHITE);
    
    // Try to display pomodoro.png from SD card
    if (SD.exists("/pomodoro/pomodoro.png")) {
      // Center the 540x540 image on the screen
      int screenWidth = M5.Display.width();
      int screenHeight = M5.Display.height();
      int imageSize = 540;
      int x = (screenWidth - imageSize) / 2;
      int y = (screenHeight - imageSize) / 2;
      
      // Draw the PNG image at centered position
      M5.Display.drawPngFile(SD, "/pomodoro/pomodoro.png", x, y);
    } else {
      // Fallback: display simple text if image not found
      M5.Display.setTextSize(4);
      M5.Display.setTextColor(TFT_BLACK);
      M5.Display.setTextDatum(MC_DATUM);
      M5.Display.drawString("POMODORO", M5.Display.width() / 2, M5.Display.height() / 2 - 20);
      M5.Display.setTextSize(2);
      M5.Display.drawString("Sleep Mode", M5.Display.width() / 2, M5.Display.height() / 2 + 20);
    }
    
    // Give 2 seconds for image to display before deep sleep
    delay(2000);
  }
}

void checkDeepSleep() {
  // Only go to deep sleep if timer is not running and not paused
  if (!animationRunning && !timerPaused) {
    unsigned long currentTime = millis();
    if (currentTime - lastActivityTime > SLEEP_TIMEOUT) {
      // Display lock screen image before deep sleep
      displayLockScreen();
      
      // Go to deep sleep
      M5.Power.deepSleep();
    }
  }
}

void setup() {
  // Enable serial communication
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait for serial connection up to 3 seconds
  
  Serial.println("=== POMODORO TIMER STARTING ===");
  Serial.println("Serial communication established");
  
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.begin();
  M5.Display.setRotation(2); // Portrait mode
  M5.Display.fillScreen(TFT_WHITE);
  
  // Set speaker volume (0-255)
  M5.Speaker.setVolume(200);
  
  // Initialize SD card
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  if (SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
    sdCardInitialized = true;
  } else {
    sdCardInitialized = false;
  }
  
  // Initialize last activity time
  lastActivityTime = millis();
  
  // Initialize battery info
  batteryLevel = M5.Power.getBatteryLevel();
  isCharging = M5.Power.isCharging();
  lastBatteryCheck = millis();
  
  // Get display dimensions
  int screenWidth = M5.Display.width();
  int screenHeight = M5.Display.height();
  
  // Set global timer center position
  timerCenterX = screenWidth / 2;
  timerCenterY = screenHeight / 3;
  drawCircularTimer(timerCenterX, timerCenterY, 25);
  
  // Add title text between circle and buttons
  int titleY = timerCenterY + 300; // Position even lower, away from circles
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.drawString("POMODORO", timerCenterX, titleY);
  
  M5.Display.setTextSize(2);
  M5.Display.drawString("epaper", timerCenterX, titleY + 35);
  
  // Wait for button press to start animation
  
  // Button dimensions
  int buttonWidth = 120;
  int buttonHeight = 60;
  int buttonSpacing = 30;
  int verticalSpacing = 30;
  int startX = (screenWidth - (3 * buttonWidth + 2 * buttonSpacing)) / 2;
  
  // Store button positions for touch detection
  // First row buttons: Start, Pause, Stop (with icons)
  int row1Y = screenHeight - 180;
  buttons[0] = {startX, row1Y, buttonWidth, buttonHeight, "icon", "play"};
  buttons[1] = {startX + buttonWidth + buttonSpacing, row1Y, buttonWidth, buttonHeight, "icon", "pause"};
  buttons[2] = {startX + 2 * (buttonWidth + buttonSpacing), row1Y, buttonWidth, buttonHeight, "icon", "stop"};
  
  // Second row buttons: 25Min, 5Min, 30Min
  int row2Y = row1Y + buttonHeight + verticalSpacing;
  buttons[3] = {startX, row2Y, buttonWidth, buttonHeight, "text", "25Min"};
  buttons[4] = {startX + buttonWidth + buttonSpacing, row2Y, buttonWidth, buttonHeight, "text", "5Min"};
  buttons[5] = {startX + 2 * (buttonWidth + buttonSpacing), row2Y, buttonWidth, buttonHeight, "text", "30Min"};
  
  // Refresh button in top right corner
  int refreshSize = 40;
  buttons[6] = {screenWidth - refreshSize - 10, 10, refreshSize, refreshSize, "refresh", "refresh"};
  
  // Draw all buttons at once
  redrawAllButtons();
  
  // Draw initial battery info in top left corner
  drawBatteryIcon(10, 10, batteryLevel, isCharging);
  
  // Print button coordinates
  Serial.println("=== BUTTON COORDINATES DEBUG ===");
  for (int i = 0; i < 7; i++) {
    Serial.printf("Button %d (%s): x=%d, y=%d, w=%d, h=%d\n", 
                  i, buttons[i].label.c_str(), buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h);
  }
  Serial.printf("Refresh button area: (%d,%d) to (%d,%d)\n", 
                buttons[6].x, buttons[6].y, buttons[6].x + buttons[6].w, buttons[6].y + buttons[6].h);
  Serial.println("================================");
}

void loop() {
  M5.update();
  checkButtonTouch(); // Check for button touches
  updateAnimation(); // Update animation every loop
  updateBatteryInfo(); // Update battery info periodically
  checkDeepSleep(); // Check if should go to deep sleep
  delay(100);
}