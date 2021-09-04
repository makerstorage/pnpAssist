
// prints the 3 line text and call from main loop
void oledPrint() {
  u8g.firstPage();
  do {
    draw();
  } while ( u8g.nextPage() );
  delay(10);
}

void draw(void)
{
  u8g.setColorIndex(1);
  u8g.setFont(u8g_font_unifont);
  u8g.drawStr( 0, 20, bufferX);
  u8g.drawStr( 0, 40, bufferY);

  u8g.drawBox(0, 45, 128, 60);
  u8g.setColorIndex(0);
  u8g.drawStr( 0, 60, bufferZ);
 

}
