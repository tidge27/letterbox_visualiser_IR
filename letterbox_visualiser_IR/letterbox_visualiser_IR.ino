
#include <Adafruit_NeoPixel.h>
#include <avr/power.h>

#define PIN            3
#define NUMPIXELS      150

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);


int msgeq_strobe_pin = 4; // strobe pins on digital 4
int msgeq_reset_pin = 5; // reset pins on digital 5
int seven_section_spectrum[7];
const int history_length = 10;
int spectrum_history[history_length][7];
int history_average_new[7];
int history_average_old[7];

boolean strobe_lights_enable = true;
int strobe_state_previous = 0;
int strobe_lights_pin_1 = 20;
int strobe_lights_brightness_1 = 0;

int cap_touch_pins[] = {
  18,19,17};
int cap_touch_normal_value[3];
boolean cap_touch_state[3] = {
  false, false, false};

int knob_pins[] = {
  1,2};

int spin_led_pos = 0;
boolean spin_clockwise;
unsigned long last_beat = millis();

int color_index = 0;
const int no_of_color_palettes = 8;
uint32_t spectrum_vis_color_pallette[no_of_color_palettes][3] = {
  {
    65280,20,1310720      }
  ,{
    255,1188864,16711680      }
  ,{
    65280,1310720,20      }
  ,{
    16776960,1310740,3421236      }
  ,{
    16777215,0,0      }
  ,{
    16711830,1310720,1310720      }
};
int spin_vis_settings[no_of_color_palettes][5] = {{2,75,1,0,1},{2,75,0,0,1},{2,75,1,0,1},{2,75,0,0,1},{2,75,0,0,1},{4,38,1,1,0},{2,75,1,0,1}};
uint32_t spin_vis_color_pallette[no_of_color_palettes][6] = {
  {
    65280,20,1310720      }
  ,{
    255,1188864,16711680      }
  ,{
    65280,1310720,20      }
  ,{
    16776960,16711935,16776960,16711935      }
  ,{
    16711935,1310740,16711935,1310740      }
  ,{
    16777215,16711680,16777215,16711680      }
  ,{
    16711830,13107200,13158600      }
};

int pulse_vis_settings[no_of_color_palettes][2] = {{0,0},{0,0},{0,0},{1,0},{1,1},{1,1},{1,1}};
uint32_t pulse_vis_color_pallette[no_of_color_palettes][4] = {
  {
    65280,20,1310720,255      }
  ,{
    255,1188864,16711680,16711680      }
  ,{
    65280,1310720,20,16711680      }
  ,{
    16776960,1310740,3421236,16711680      }
  ,{
    16777215,6579300,3618615,16711680      }
  ,{
    16711830,13107200,13158600,16711680      }
};

void setup() {

  for(int i=0; i<3; i++) {
    cap_touch_normal_value[i] = touchRead(cap_touch_pins[i]);
  }

  pinMode(strobe_lights_pin_1, OUTPUT);
  // Stops current inrush at start
  digitalWrite(strobe_lights_pin_1,HIGH);
  delay(1000);
  digitalWrite(strobe_lights_pin_1, LOW);


  pixels.begin();
  pixels.clear();
  pixels.show();
  Serial.begin(9600);

  for(int row = 0; row < history_length; row++) {
    for(int col = 0; col < 7; col++) {
      spectrum_history[row][col] = 0;
    }
  }

  pinMode(msgeq_reset_pin, OUTPUT); // reset
  pinMode(msgeq_strobe_pin, OUTPUT); // strobe
  digitalWrite(msgeq_reset_pin,LOW); // reset low
  digitalWrite(msgeq_strobe_pin,HIGH); //pin 5 is RESET on the shield


}


boolean checkCapacitiveButtonPress(int button_index) {
  if(touchRead(cap_touch_pins[button_index]) > cap_touch_normal_value[button_index] + 1000) {
    if(cap_touch_state[button_index] == false) {
      cap_touch_state[button_index] = true;
      return true;
    } 
    else {
      return false;
    }
  } 
  else {
    cap_touch_state[button_index] = false;
    return false;
  }
}


int convertnum(int num) {
  num = num - (num/150)*150;
  return num;
}


void colladascopeifyfull() {
  for (int i = 0; i < 29;i++){
    uint32_t color = pixels.getPixelColor(i);
    pixels.setPixelColor(56-i, color);
  } 
  for (int i = 0; i < 57;i++){
    uint32_t color = pixels.getPixelColor(i);
    pixels.setPixelColor(131-i, color);
  } 

  for (int i = 0; i < 28;i++){
    uint32_t color = pixels.getPixelColor(i);
    pixels.setPixelColor(150-map(i,0,28,0,10), color);
    pixels.setPixelColor(131+map(i,0,28,0,10), color);
    pixels.setPixelColor(56+map(i,0,28,0,10), color);
    pixels.setPixelColor(75-map(i,0,28,0,10), color);
  }

}

void fadepixels(int fade, int resolution) {
  for(int i = 0; i<=150; i++) {
    uint32_t color = pixels.getPixelColor(i);
    uint8_t red = color >> 16;
    uint8_t green = color >> 8;
    uint8_t blue = color >> 0;

    if( fade > resolution) {
      fade = resolution;
    }

    int red_new = (float(red * (resolution - fade))/resolution);
    int green_new = (float(green * (resolution - fade))/resolution);
    int blue_new = (float(blue * (resolution - fade))/resolution);

    uint32_t new_color = pixels.Color(red_new,green_new,blue_new);

    pixels.setPixelColor(i, new_color);
  }
}


void spectrumVisualiser(int spectrum_input[7], uint32_t colors[3], boolean colidascopeon = false){
  // sets pixel colors around the outer edge
  for(int i = 57; i < 150; i = i + 4){
    pixels.setPixelColor(i,colors[2]);
  }  

  int thirteen_spectrum[13];
  const int constr_val = 65;

  thirteen_spectrum[0] = map(constrain(spectrum_input[0],constr_val,1023),constr_val,1023,1,255);
  thirteen_spectrum[2] = map(constrain(spectrum_input[1],constr_val,1023),constr_val,1023,1,255);
  thirteen_spectrum[4] = map(constrain(spectrum_input[2],constr_val,1023),constr_val,1023,1,255);
  thirteen_spectrum[6] = map(constrain(spectrum_input[3],constr_val,1023),constr_val,1023,1,255);
  thirteen_spectrum[8] = map(constrain(spectrum_input[4],constr_val,1023),constr_val,1023,1,255);
  thirteen_spectrum[10] = map(constrain(spectrum_input[5],constr_val,1023),constr_val,1023,1,255);
  thirteen_spectrum[12] = map(constrain(spectrum_input[6],constr_val,1023),constr_val,1023,1,255);

  thirteen_spectrum[1] = (thirteen_spectrum[0] + thirteen_spectrum[2])/2;
  thirteen_spectrum[3] = (thirteen_spectrum[2] + thirteen_spectrum[4])/2;
  thirteen_spectrum[5] = (thirteen_spectrum[4] + thirteen_spectrum[6])/2;
  thirteen_spectrum[7] = (thirteen_spectrum[6] + thirteen_spectrum[8])/2;
  thirteen_spectrum[9] = (thirteen_spectrum[8] + thirteen_spectrum[10])/2;
  thirteen_spectrum[11] = (thirteen_spectrum[10] + thirteen_spectrum[12])/2;



  uint8_t red = colors[0] >> 16;
  uint8_t green = colors[0] >> 8;
  uint8_t blue = colors[0] >> 0;

  for (int var = 0; var < 13; var++)
  {
    int red_band = (float(red * thirteen_spectrum[var])/255.0);
    int green_band = (float(green * thirteen_spectrum[var])/255.0);
    int blue_band = (float(blue * thirteen_spectrum[var])/255.0);

    uint32_t band_color = pixels.Color(red_band,green_band,blue_band);

    pixels.setPixelColor((4*var + 2),colors[1]);
    pixels.setPixelColor((4*var + 3),band_color);
    pixels.setPixelColor((4*var + 4),band_color);
    pixels.setPixelColor((4*var + 5),band_color);
    pixels.setPixelColor((4*var + 6),colors[1]);
  }
  //  if(colidascopeon){
  //    colladascopeifyfull();
  //  }
  pixels.show();
}




void spinVisualiser(uint32_t colors[], int barnum, int barwidth, boolean collidascope = false, boolean stop_start_noise_opposite = false, boolean colorchange = true, boolean dirchange = true, boolean poschange = true, boolean fade_to_black = true)
{
  pixels.clear();
  int spacebet = 150 / barnum;

  // display values of left channel on serial monitor
  int   power = history_average_new[0]*5 + history_average_new[1]*5;

  if(map(power,0,11253,0,1000) >= 400) {
    if(stop_start_noise_opposite) {
     // spin_clockwise = spin_clockwise;
    } else {
      spin_clockwise = !spin_clockwise;
    }
    last_beat = millis();
  } else {
   if(stop_start_noise_opposite) {
    spin_clockwise = !spin_clockwise;
   }
  } 

  if (spin_clockwise || !dirchange){
    if(poschange) {
      spin_led_pos +=1;
    }
  } 
  else {
    if(poschange) {
      spin_led_pos -=1;
    }
  }

  //check overrun
  if (spin_led_pos == 151){
    spin_led_pos = 0;
  } 
  else if(spin_led_pos == -1) {
    spin_led_pos = 150;
  }

  for(int i = 0; i <= barnum; i++) {
    for( int v = 0 ; v< barwidth; v++) {
      if (spin_clockwise && colorchange) {
        pixels.setPixelColor(convertnum(v + spin_led_pos + i*spacebet), colors[i+1]);
      } 
      else {
        pixels.setPixelColor(convertnum(v + spin_led_pos + i*spacebet), colors[i-1]);
      }
    }
  }
  if(collidascope) {
    colladascopeifyfull();
  }
  fadepixels((millis() - last_beat), 2000);
  pixels.show();
  pixels.clear();
}



void pulseVisualiser(uint32_t colors[4], boolean middle_edge = true, boolean all_pixels_high_color = false) {


  int powers1[] = {
    2,2,2,2,2,2,2    };
  int power1 = history_average_new[0]*powers1[0] + history_average_new[1]*powers1[1] + history_average_new[2]*powers1[2] + history_average_new[3]*powers1[3] + history_average_new[4]*powers1[4] + history_average_new[5]*powers1[5] + history_average_new[6]*powers1[6];
  power1 = constrain(power1, 1000, 10000);
  int power_conv1 = map(power1, 1000, 10000, 0, 35);

  if(middle_edge) {
    for ( int i = 0; i< power_conv1; i++) {
      if(i<9){
        pixels.setPixelColor(28-i, colors[0]);
      } 
      else if(i<14) {
        pixels.setPixelColor(28-i, colors[1]);
      } 
      else if(i<20) {
        pixels.setPixelColor(28-i, colors[2]);
      } 
      else {
        pixels.setPixelColor(28-i, colors[3]);
        
        if(all_pixels_high_color) {
          for(int z = 0; z<=150; z++) {
            if(pixels.getPixelColor(z) > 0) {
              pixels.setPixelColor(z, colors[3]);
            }
          }
        }
      }
    }
  }

  if(!middle_edge){
    for ( int i = 0; i< power_conv1; i++) {
      if(i<10){
        pixels.setPixelColor(i, colors[0]);
      } 
      else if(i<17) {
        pixels.setPixelColor(i, colors[1]);
      } 
      else if(i<23) {
        pixels.setPixelColor(i, colors[2]);
      } 
      else {
        pixels.setPixelColor(i, colors[3]);
      }
    }
  }


  colladascopeifyfull();
  pixels.show();
  pixels.clear();
}



boolean spectrumBandRising(int band_zero_index, int difference = 40) {
  if(history_average_old[band_zero_index] + difference < history_average_new[band_zero_index]) {
    return true;
  } 
  else { 
    return false;
  }
}

boolean spectrumBandHeight(int band_zero_index, int height = 300) {
  if(height < history_average_new[band_zero_index]) {
    return true;
  } 
  else { 
    return false;
  }
}

boolean fullSpectrumAboveHeight(int height = 300) {
  if(spectrumBandHeight(0,height) && spectrumBandHeight(1,height) && spectrumBandHeight(2,height) && spectrumBandHeight(3,height) && spectrumBandHeight(4,height) && spectrumBandHeight(5,height) && spectrumBandHeight(6,height)) {
    return true;
  } 
  else {
    return false;
  }
} 

boolean triggersList(int mode) {
  switch(mode) {

  case 0:
    //fade_speed = 0;
    if(spectrumBandRising(5,45) || spectrumBandRising(6,50) || spectrumBandRising(1,55)) {
      return true;
    } 
    else {
      return false;
    }
    break;

  case 1:
    //fade_speed = 9;
    if(spectrumBandRising(0,50)) {
      return true;
    } 
    else {
      return false;
    }
    break;

  case 2:
    //fade_speed = 9;
    if(spectrumBandRising(0,45) && spectrumBandRising(1,30) && fullSpectrumAboveHeight(70)) {
      return true;
    } 
    else {
      return false;
    }
    break;

  case 3:
    //fade_speed = 9;
    if(spectrumBandRising(0,45) && spectrumBandHeight(0,300) && spectrumBandHeight(1,400) && fullSpectrumAboveHeight(70)) {
      return true;
    } 
    else {
      return false;
    }
    break;

  case 4:
    //fade_speed = 9;
    if(spectrumBandHeight(0,600) && spectrumBandHeight(1,500) && fullSpectrumAboveHeight(150)) {
      return true;
    } 
    else {
      return false;
    }
    break;
  }
}

void strobeLights(int mode=0) {
  // 12 modes, with a trigger and fade setting for each
  int strobe_modes[12][2] = {
    {
      4,0    }
    ,{
      3,0    }
    ,{
      1,2    }
    ,{
      4,2    }
    ,{
      3,2    }
    ,{
      2,2    }
    ,{
      4,3    }
    ,{
      3,4    }
    ,{
      2,5    }
    ,{
      1,7    }
    ,{
      1,9    }
    ,{
      0,9    }
  };

  if(strobe_lights_enable) {
    int fade_speed = 0;
    fade_speed = strobe_modes[read_knob(0)][1];
    // fade speed between 0 and 10  0 means instant fade, 10 is slow
    boolean lights_on;

    lights_on = triggersList(strobe_modes[read_knob(0)][0]);

    if(fade_speed) {
      if(lights_on) {
        strobe_lights_brightness_1 = 255;
      } 
      else {
        if(strobe_lights_brightness_1 > 6) {
          strobe_lights_brightness_1 = strobe_lights_brightness_1*(fade_speed+50)/61;
        } 
        else {
          strobe_lights_brightness_1 = strobe_lights_brightness_1*(fade_speed+150)/161;
        }
      }
      analogWrite(strobe_lights_pin_1, strobe_lights_brightness_1);
    } 
    else {
      if(lights_on) {
        analogWrite(strobe_lights_pin_1, 255);
      } 
      else {
        analogWrite(strobe_lights_pin_1, 0);
      }
    }

  } 
  else {
    analogWrite(strobe_lights_pin_1, 0);
  }

}

int read_knob(int knob_index) {
  int output_var = 0;
  int analog_input = analogRead(knob_pins[knob_index]);
  for( int i = 0; i<=12; i++) {
    if (i*93-45 < analog_input) {
      output_var = i;
    }
  }
  return output_var;
}



void loop() {

  if(checkCapacitiveButtonPress(0)) {
    color_index++;
    if(color_index == no_of_color_palettes) {
      color_index = 0;
    }
  }
  if(checkCapacitiveButtonPress(1)) {
    analogWrite(strobe_lights_pin_1, 0);
    strobe_lights_enable = !strobe_lights_enable;
  }



  read_msgeq_7();  //Reads the frequency satuff intot he spectrum[7] array

  switch(read_knob(1)) {
  case 0:
    spectrumVisualiser(history_average_new,spectrum_vis_color_pallette[color_index]);
    break;
  case 1:
    spinVisualiser(spin_vis_color_pallette[color_index],spin_vis_settings[color_index][0],spin_vis_settings[color_index][1],spin_vis_settings[color_index][2],spin_vis_settings[color_index][3],spin_vis_settings[color_index][4]);
    break;
  case 2:
    pulseVisualiser(pulse_vis_color_pallette[color_index], pulse_vis_settings[color_index][0], pulse_vis_settings[color_index][1]);
    break;
  }

  if((touchRead(cap_touch_pins[2]) > cap_touch_normal_value[2] + 1000)) {
    strobe_state_previous++;
    int strobe_delay_time = 5;
    if (strobe_state_previous == strobe_delay_time*2) { 
      strobe_state_previous = 0;
    }
    if(strobe_state_previous > strobe_delay_time) {
      analogWrite(strobe_lights_pin_1, 255);
    } 
    else {
      analogWrite(strobe_lights_pin_1, 0);
    }
  } 
  else {
    strobeLights(4);
  }

}


void read_msgeq_7()  {
  digitalWrite(msgeq_reset_pin, HIGH);
  digitalWrite(msgeq_reset_pin, LOW);
  for(int band=0; band <7; band++)  {
    digitalWrite(msgeq_strobe_pin,LOW); // strobe pin on the shield - kicks the IC up to the next band 
    delayMicroseconds(30); // 
    seven_section_spectrum[band] = analogRead(0); // store spectrum_input band reading
    digitalWrite(msgeq_strobe_pin,HIGH); 
  }
  shiftHistories();
}
void shiftHistories() {
  for(int i=history_length-1;i>0;i--) {
    for(int x=0;x<7;x++) {
      spectrum_history[i][x] = spectrum_history[i-1][x];
    }
  }
  for(int x=0;x<7;x++) {
    spectrum_history[0][x] = seven_section_spectrum[x];
  }
  for(int i=0;i<7;i++) {
    int count=0;
    for(int x=0;x<history_length;x++) {
      count = count + spectrum_history[x][i];
    }
    history_average_old[i] = history_average_new[i];
    history_average_new[i] = count/history_length;
  }
}














