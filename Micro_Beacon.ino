// Micro_Beacon -- a basic CW beacon for Arduino Nano/Uno/ATMega328 and si5351 breakout 
// Derived from na simple CW keyer, VK3HN, 7 Apr 2017
// https://vk3hn.wordpress.com/

#include <si5351.h>     // Etherkit si3531 library from NT7S,  V2.1.4   https://github.com/etherkit/Si5351Arduino 
#include <Wire.h>
  

#define PIN_KEY_LINE       2  // digital pin for the key line (mirrors PIN_TONE_OUT)
#define PIN_PTT_LINE       3  // digital pin for the PTT line
#define PIN_TONE_OUT       8  // digital pin with keyed audio tone on it
#define PIN_KEYER_MEM1     9  // digital pin to read pushbutton Keyer Memory 1 
#define PIN_KEYER_MEM2    10  // digital pin to read pushbutton Keyer Memory 2 
#define PIN_PADDLE_R      11  // digital pin for paddle left (dot)
#define PIN_PADDLE_L      12  // digital pin for paddle right (dash)

#define PIN_KEYER_SPEED    3  // analogue pin for speed potentiometer wiper 
#define CW_TONE_HZ       700  // CW tone frequency (Hz)   

#define CW_DASH_LEN        5  // length of dash (in dots)
#define BREAK_IN_DELAY   800  // break-in hang time (mS)
#define SERIAL_LINE_WIDTH 80  // number of morse chars on Serial after which we newline 

#define CW_IDENT
#define CW_IDENT_SECS     2  // seconds between CW ident 
#define CW_IDENT_WPM      38  // CW ident speed (dot length mS)

byte freq_indx = 0; 

unsigned long freq[] = {   
  3540000,
//  7040000,
// 14040000,
// 21040000,
};

/* 
  1840000,
  3540000,
  7040000,

 10140000,
 14040000,
 21040000,
 
 28240000,
 50140000,
144440000
 */

// #defines for LCD display for VSWR meter

enum trx_state_e {
  E_STATE_RX, 
  E_STATE_TX
};  // state of the txcvr 

trx_state_e curr_state;

enum key_state_e {
  E_KEY_UP, 
  E_KEY_DOWN
};  // state of the key line 
key_state_e key_state;

int char_sent_ms, curr_ms;
int dot_length_ms = 60;   // keyer speed ((dot length mS)), 60 equates to 10 w.p.m.
int last_ident_ms;        // CW ident timer
int ident_secs_count = 0; // seconds counter
                         
bool space_inserted;
bool paddle_squeezed;
unsigned int ch_counter;
// bool  firsttime = true;

// morse reference table
struct morse_char_t {
  char ch[7]; 
};

morse_char_t MorseCode[] = {
  {'A', '.', '-',  0,   0,   0,   0},
  {'B', '-', '.', '.', '.',  0,   0},
  {'C', '-', '.', '-', '.',  0,   0},
  {'D', '-', '.', '.',  0,   0,   0},
  {'E', '.',  0,   0,   0,   0,   0},
  {'F', '.', '.', '-', '.',  0,   0},
  {'G', '-', '-', '.',  0,   0,   0},
  {'H', '.', '.', '.', '.',  0,   0},
  {'I', '.', '.',  0,   0,   0,   0},
  {'J', '.', '-', '-', '-',  0,   0},
  {'K', '-', '.', '-',  0,   0,   0},
  {'L', '.', '-', '.', '.',  0,   0},
  {'M', '-', '-',  0,   0,   0,   0},
  {'N', '-', '.',  0,   0,   0,   0},
  {'O', '-', '-', '-',  0,   0,   0},
  {'P', '.', '-', '-', '.',  0,   0},
  {'Q', '-', '-', '.', '-',  0,   0},
  {'R', '.', '-', '.',  0,   0,   0},
  {'S', '.', '.', '.',  0,   0,   0},
  {'T', '-',  0,   0,   0,   0,   0},
  {'U', '.', '.', '-',  0,   0,   0},
  {'V', '.', '.', '.', '-',  0,   0},
  {'W', '.', '-', '-',  0,   0,   0},
  {'X', '-', '.', '.', '-',  0,   0},
  {'Y', '-', '.', '-', '-',  0,   0},
  {'Z', '-', '-', '.', '.',  0,   0},
  {'0', '-', '-', '-', '-', '-',  0},
  {'1', '.', '-', '-', '-', '-',  0},
  {'2', '.', '.', '-', '-', '-',  0},
  {'3', '.', '.', '.', '-', '-',  0},
  {'4', '.', '.', '.', '.', '-',  0},
  {'5', '.', '.', '.', '.', '.',  0},
  {'6', '-', '.', '.', '.', '.',  0},
  {'7', '-', '-', '.', '.', '.',  0},
  {'8', '-', '-', '-', '.', '.',  0},
  {'9', '-', '-', '-', '-', '.',  0},
  {'/', '-', '.', '.', '-', '.',  0},
  {'?', '.', '.', '-', '-', '.', '.'},
  {'.', '.', '-', '.', '-', '.', '-'},
  {',', '-', '-', '.', '.', '-', '-'},
  {'(', '-', '.', '-', '.', '-', 0},
  {')', '.', '-', '.', '-', '.', 0}
};

String morse_msg[] = {"CQ CQ DE VK3HN K", "CQ CQ SOTA DE VK3HN/P K" };

Si5351 si5351;                // I2C address defaults to x60 in the NT7S lib


//-------------------------------------------------------------------------------------

int morse_lookup(char c)
// returns the index of parameter 'c' in MorseCode array, or -1 if not found
{
  for(int i=0; i<sizeof(MorseCode); i++)
  {
    if(c == MorseCode[i].ch[0])
      return i;
  }
  return -1; 
}


bool get_button(byte btn)
{
// Read the digital pin 'btn', with debouncing  
// pins are pulled up (ie. default is HIGH)
// returns TRUE if pin is high, FALSE otherwise 
  if (!digitalRead(btn)) 
  {
    delay(5);  // was 20mS, needs to be long enough to ensure debouncing
    if (!digitalRead(btn))
    {
      // while (!digitalRead(btn)); // loop here until button released
      return 1;                     // now, return 'true' 
    }
  }
  return 0;
}


int read_analogue_pin(byte p)
{
// Take an averaged reading of analogue pin 'p'  
  int i, val=0, nbr_reads=2; 
  for (i=0; i<nbr_reads; i++)
  {
    val += analogRead(p);
    delay(5); 
  }
  return val/nbr_reads; 
}

int read_keyer_speed()
{ 
  int n = read_analogue_pin((byte)PIN_KEYER_SPEED);
  //Serial.print("Speed returned=");
  //Serial.println(n);
  dot_length_ms = 60 + (n-183)/5;   // scale to wpm (10 wpm == 60mS dot length)
                                     // '511' should be mid point of returned range
                                     // change '5' to widen/narrow speed range...
                                     // smaller number -> greater range  
  return n;
}


void set_key_state(key_state_e k)
{
// push the morse key down, or let it spring back up
// changes global 'key_state' {E_KEY_DOWN, E_KEY_UP}
  switch (k){
      case E_KEY_DOWN:
      {
        // do whatever you need to key the transmitter
        digitalWrite(PIN_KEY_LINE, 1);
        digitalWrite(13, 1);  // for now, turn the Nano's LED on
        key_state = E_KEY_DOWN;
        // Serial.println("key down");
      }
      break;

      case E_KEY_UP:
      {
        // do whatever you need to un-key the transmitter 
        digitalWrite(PIN_KEY_LINE, LOW);
        digitalWrite(13, 0);  // for now, turn the Nano's LED off
        key_state = E_KEY_UP;
        char_sent_ms = millis();
        space_inserted = false;
        // Serial.println("key down");
      }
      break;
  }    
}


void activate_state(trx_state_e s)
{
// if necessary, activate the receiver or the transmitter 
// changes global 'curr_state' {E_STATE_RX, E_STATE_TX}
  switch (s)
  {
      case E_STATE_RX:
      {
        if(curr_state == E_STATE_RX)
        {
          // already in receive state, nothing to do!
        }
        else
        { 
          // turn transmitter off (drop PTT line)
          digitalWrite(PIN_PTT_LINE, 0); 
          // un-mute receiver
          curr_state = E_STATE_RX;
          // Serial.println();
          Serial.println("\n>Rx");
        }
      }
      break;

      case E_STATE_TX:
      {
        if(curr_state == E_STATE_TX)
        {
          // already in transmit state, nothing to do!
        }
        else
        {
          // turn transmitter on (raise PTT line) 
          digitalWrite(PIN_PTT_LINE, 1); 
          curr_state = E_STATE_TX;
          Serial.println("\n>Tx");
        }
      }
      break;
  }    
}


void send_dot()
{
  delay(dot_length_ms);  // wait for one dot period (space)

  // send a dot and the following space
  tone(PIN_TONE_OUT, CW_TONE_HZ);

  si5351.set_freq(freq[freq_indx] * SI5351_FREQ_MULT, SI5351_CLK0);  
  si5351.output_enable(SI5351_CLK0, 1);
//  si5351.set_freq(freq[freq_indx+1] * SI5351_FREQ_MULT, SI5351_CLK1);  
//  si5351.output_enable(SI5351_CLK1, 1);
//  si5351.set_freq(freq[freq_indx+2] * SI5351_FREQ_MULT, SI5351_CLK2);  
//  si5351.output_enable(SI5351_CLK2, 1);

  set_key_state(E_KEY_DOWN);
//  if(ch_counter % SERIAL_LINE_WIDTH == 0) Serial.println(); 
  Serial.print(".");
  delay(dot_length_ms);  // key down for one dot period
  noTone(PIN_TONE_OUT);
  
  si5351.output_enable(SI5351_CLK0, 0);
//  si5351.output_enable(SI5351_CLK1, 0);
//  si5351.output_enable(SI5351_CLK2, 0);

  set_key_state(E_KEY_UP); 
  ch_counter++;
}


void send_dash()
{
  delay(dot_length_ms);  // wait for one dot period (space)
  // send a dash and the following space
  tone(PIN_TONE_OUT, CW_TONE_HZ);
  
  si5351.set_freq(freq[freq_indx] * SI5351_FREQ_MULT, SI5351_CLK0);  
  si5351.output_enable(SI5351_CLK0, 1);
//  si5351.set_freq(freq[freq_indx+1] * SI5351_FREQ_MULT, SI5351_CLK1);  
//  si5351.output_enable(SI5351_CLK1, 1);
//  si5351.set_freq(freq[freq_indx+2] * SI5351_FREQ_MULT, SI5351_CLK2);  
//  si5351.output_enable(SI5351_CLK2, 1);

  set_key_state(E_KEY_DOWN); 
//  if(ch_counter % SERIAL_LINE_WIDTH == 0) Serial.println(); 
  Serial.print("-");
  delay(dot_length_ms * CW_DASH_LEN);  // key down for CW_DASH_LEN dot periods
  noTone(PIN_TONE_OUT);
  
  si5351.output_enable(SI5351_CLK0, 0);
//  si5351.output_enable(SI5351_CLK1, 0);
//  si5351.output_enable(SI5351_CLK2, 0);

  set_key_state(E_KEY_UP); 
  ch_counter++;
}


void send_letter_space()
{
  delay(dot_length_ms * 4);  // wait for 3 dot periods
  Serial.print(" ");
}


void send_word_space()
{
  delay(dot_length_ms * 7);  // wait for 6 dot periods
  Serial.print("  ");
}


void send_morse_char(char c)
{
  // 'c' is a '.' or '-' char, so send it 
  if(c == '.') send_dot();
  else if (c == '-') send_dash();
  // ignore anything else, including 0s
}


void play_message(String m, int s)
{
// sends the message in string 'm' as CW, with inter letter and word spacing
// s is the speed to play at; if s == 0, use the current speed  
  int i, j, n, old_s; 
  char buff[100];

  Serial.println(m);
//  Serial.println(m.length());

  // use ch = m.charAt(index);
  m.toCharArray(buff, m.length()+1);

  if(s > 0)  // caller has passed in a speed to send message at 
  {
    old_s = dot_length_ms; // preserve the current keyer speed
    dot_length_ms = s;
  }

  digitalWrite(PIN_PTT_LINE, 1); // turn transmitter on 

  for (i=0; i<m.length(); i++)
  {
    if(buff[i] == ' ') 
    {
       send_word_space(); 
    }
    else
    {
      if( (n = morse_lookup(buff[i])) == -1 )
      {
        // char not found, ignore it (but report it on Serial)
        Serial.print("Char in message not found in MorseTable <");
        Serial.print(buff[i]);
        Serial.println(">");
      }
      else
      {
        // char found, so send it as dots and dashes
        // Serial.println(n);
        for(j=1; j<7; j++)
          send_morse_char(MorseCode[n].ch[j]);
        send_letter_space();  // send an inter-letter space
        if(s==0) 
          read_keyer_speed();  // see if speed has changed mid message 
      } // else
    } // else 
  } // for  
  Serial.println();
  if(s > 0)  // reset speed back to what it was  
    dot_length_ms = old_s;

  digitalWrite(PIN_PTT_LINE, 0); // turn transmitter off
} // play_message


void setup()
{
  Serial.begin(9600);  
  Serial.println("\nBasic beacon VK3HN v1.0 26-Nov-2021. Please reuse, hack and pass on.");

  pinMode(PIN_PADDLE_L,  INPUT_PULLUP);
  pinMode(PIN_PADDLE_R,  INPUT_PULLUP);
  pinMode(PIN_KEYER_MEM1,INPUT_PULLUP);
  pinMode(PIN_KEYER_MEM2,INPUT_PULLUP);
  pinMode(PIN_PTT_LINE,  INPUT_PULLUP);

  digitalWrite(PIN_PTT_LINE, 0);  // send the PTT line low

  curr_state = E_STATE_RX;
  key_state = E_KEY_UP;
  char_sent_ms = millis();
  space_inserted = false;
  paddle_squeezed = false;
  ch_counter = 0;

  last_ident_ms = millis(); 

  // dump out the MorseCode table for diagnostic
/*  for(int i=0; i<40; i++)
  {
    Serial.print(MorseCode[i].ch[0]);
    Serial.print(' ');
    for(int j=1; j<7; j++)
      Serial.print(MorseCode[i].ch[j]);
    Serial.println();
  }
*/
// play the two messages as a diagnostic
//  play_message(morse_msg[0], 0);
//  delay(2000);
//  play_message(morse_msg[1], 0);
//  delay(2000);

// initialise and start the si5351 clocks

  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0); 
  si5351.set_correction(135000);    // Library update 26/4/2020: requires destination register address  ... si5351.set_correction(19100, SI5351_PLL_INPUT_XO);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);

  si5351.set_freq(freq[freq_indx] * SI5351_FREQ_MULT, SI5351_CLK0);  
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA); 
  si5351.output_enable(SI5351_CLK0, 0);   

//  si5351.set_freq(freq[freq_indx+1] * SI5351_FREQ_MULT, SI5351_CLK1);  
//  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA); 
//  si5351.output_enable(SI5351_CLK1, 0);   
  
//  si5351.set_freq(freq[freq_indx+2] * SI5351_FREQ_MULT, SI5351_CLK2);  
//  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA); 
//  si5351.output_enable(SI5351_CLK2, 0);   

}

void loop()
{  
  int f, r; 
  //-- keyer code begins --------------------------------------------------
  // read the speed control 
  read_keyer_speed();

  // see if a memory button has been pushed
  if(get_button(PIN_KEYER_MEM1))
    play_message(morse_msg[0], 0);  
  if(get_button(PIN_KEYER_MEM2))
    play_message(morse_msg[1], 0); 

  // see if the paddle has been pressed
  bool l_paddle = get_button(PIN_PADDLE_L);
  bool r_paddle = get_button(PIN_PADDLE_R);

  if(l_paddle or r_paddle) activate_state(E_STATE_TX);
  if(l_paddle) send_dot();
  if(r_paddle) send_dash();
  if(l_paddle and r_paddle) // paddle_squeezed = true;
  {
     send_dot();
     send_dash();
  }  

  curr_ms = millis();
  // if paddle has been idle for BREAK_IN_DELAY drop out of transmit 
  if((curr_state == E_STATE_TX) and (curr_ms - char_sent_ms) > BREAK_IN_DELAY)
  {
    // drop back to receive to implement break-in
    activate_state(E_STATE_RX); 
  }
  //-- keyer code ends --------------------------------------------------
  //-- CW ident code begins ---------------------------------------------
#ifdef CW_IDENT
  if(abs(curr_ms - last_ident_ms) > 1000)
  {
    ident_secs_count++;
    last_ident_ms = curr_ms;
    if(ident_secs_count%10 == 0)
    {
      Serial.print(ident_secs_count);
      Serial.print(" ");
    }
  }

  if(ident_secs_count == CW_IDENT_SECS)
  {
    if(curr_state == E_STATE_RX) 
    {
      Serial.println(); 
      Serial.print((unsigned int)(freq[freq_indx]/1000) );
      Serial.print("kHz ");
      play_message("( V V V DE VK3HN QF22NH )", CW_IDENT_WPM);  // only Id if not transmitting
      freq_indx ++;
      if(freq_indx == (sizeof(freq)/4) ) freq_indx = 0; 
//        Serial.print("sizeof(freq)=");  Serial.println((byte)(sizeof(freq)));
//      if(freq_indx == 4) freq_indx = 0; 
      delay(1000); 
    }
    ident_secs_count = 0;
  }
#endif 

  //-- CW ident code end ------------------------------------------------
}
