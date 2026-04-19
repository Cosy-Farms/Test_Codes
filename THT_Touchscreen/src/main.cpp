#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Instructions on screen
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Touch Test");

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.println("Tap anywhere!");

  Serial.println("Touch test ready!");
}

void loop()
{
  uint16_t tx, ty;

  // getTouch returns true when screen is touched
  if (tft.getTouch(&tx, &ty))
  {
    Serial.print("X: "); Serial.print(tx);
    Serial.print("  Y: "); Serial.println(ty);

    // Draw crosshair
    tft.fillCircle(tx, ty, 5, TFT_RED);
    tft.drawLine(tx - 12, ty, tx + 12, ty, TFT_YELLOW);
    tft.drawLine(tx, ty - 12, tx, ty + 12, TFT_YELLOW);

    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(tx + 8, ty - 4);
    tft.print(tx); tft.print(","); tft.print(ty);

    delay(100);
  }
}