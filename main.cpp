#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP_Mail_Client.h>

/*
  ToDo:
    - Proper buttons
    - Potentially take photo and cache it during first entrance, send if failed
*/

const String wifi_ssid = "";
const String wifi_pass = "";

const String smtp_host = "";
const int smtp_port = ;
const String smtp_user = ""; // email to send from
const String smtp_pass = "";                   // password

const String smtp_send = ""; // email to send to

SMTPSession smtp;
ESP_Mail_Session session;

const int code = 143312;
const int openTime = 30000;
const int attempts = 3;
const int debounce = 200;

const int doorCheck = 13;
const int redLight = 2;
const int greenLight = 0;

const int oneButton = 12;
const int twoButton = 14;
const int threeButton = 4;
const int fourButton = 5;

int counter = 1;
int requireCode = 0;

int lastInput = 0;     // the last digit entered
int lastInputTime = 0; // time until digit can be entered again
int inputDigits = 0;   // the entered code
int inputAmount = 0;   // how many digits have been entered
int currentAttempts = 0;

bool failedEntry = false;
bool opened = true; // if the door is open or not

bool armed = false;

void connectWiFi()
{
  WiFi.begin(wifi_ssid, wifi_pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.print("\nConnected: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);

  pinMode(redLight, OUTPUT);
  pinMode(greenLight, OUTPUT);
  digitalWrite(redLight, HIGH);
  digitalWrite(greenLight, HIGH);

  pinMode(doorCheck, INPUT_PULLUP);
  pinMode(oneButton, INPUT_PULLUP);
  pinMode(twoButton, INPUT_PULLUP);
  pinMode(threeButton, INPUT_PULLUP);
  pinMode(fourButton, INPUT_PULLUP);

  connectWiFi();

  session.server.host_name = smtp_host;
  session.server.port = smtp_port;
  session.login.email = smtp_user;
  session.login.password = smtp_pass;
  session.login.user_domain = "";

  session.time.ntp_server = "pool.ntp.org,time.nist.gov";
  session.time.gmt_offset = -5;
}

void resetEntry()
{
  lastInput = 0;     // the last digit entered
  lastInputTime = 0; // time until digit can be entered again
  inputDigits = 0;   // the entered code
  inputAmount = 0;   // how many digits have been entered - could be replaced by getting length of inputDigits later
}

int getLength(int a)
{
  int length = 0;
  while (a > 0)
  {
    a /= 10;
    length++;
  }
  return length;
}

bool equalLength(int a, int b)
{
  if (getLength(a) == getLength(b))
    return true;
  return false;
}

void loop()
{
  /*if (counter % 1000 == 0)
    Serial.println(counter);
  if (counter % 1000 == 0)
    Serial.println(requireCode);*/

  if (counter <= requireCode && armed)
  { // enter a code now
    if (counter % 750 == 0)
      digitalWrite(redLight, LOW);
    if (counter % 750 != 0 && counter % 250 == 0)
      digitalWrite(redLight, HIGH);

    if (lastInput == 0 && lastInputTime <= counter)
    {
      digitalWrite(greenLight, HIGH);

      if (!digitalRead(oneButton))
        lastInput = 1;
      if (!digitalRead(twoButton))
        lastInput = 2;
      if (!digitalRead(threeButton))
        lastInput = 3;
      if (!digitalRead(fourButton))
        lastInput = 4;

      if (lastInput != 0)
      {
        digitalWrite(greenLight, LOW);

        inputDigits = inputDigits * 10 + lastInput; /* lastInput * pow(10, getLength(code) - inputAmount - 1);*/
        inputAmount++;
        lastInputTime = counter + debounce;
        Serial.println(inputDigits);
      }
    }
    else if (digitalRead(oneButton) && digitalRead(twoButton) && digitalRead(threeButton) && digitalRead(fourButton)) // reset input
    {
      lastInput = 0;
    }

    if (equalLength(inputDigits, code))
    {
      if (inputDigits == code) // success!
      {
        failedEntry = false;
        requireCode = 0;
        Serial.println("success");
        currentAttempts = 0;
        armed = false;
      }
      else // failed input
      {
        requireCode = counter;
        Serial.println("Fail");
      }
    }
  }
  else if (requireCode != 0 && counter > requireCode)
  { // failed to enter code in time
    currentAttempts++;

    // you failed! animation type dealy doodle
    digitalWrite(redLight, LOW);
    delay(500);
    digitalWrite(redLight, HIGH);
    delay(250);
    digitalWrite(redLight, LOW);
    delay(500);
    digitalWrite(redLight, HIGH);
    delay(250);
    digitalWrite(redLight, LOW);
    delay(500);
    digitalWrite(redLight, HIGH);

    if (currentAttempts >= attempts)
    {
      if (WiFi.status() != WL_CONNECTED)
      {
        WiFi.disconnect();
        connectWiFi();
      }

      SMTP_Message message;
      message.sender.name = "ESP Security";
      message.sender.email = smtp_user;
      message.subject = "Code Entry Failed!";
      message.addRecipient("User", smtp_send);
      message.text.content = "There was an access to (room) with a failed code input.";

      // Connect to server with the session config
      smtp.connect(&session);

      // Start sending Email and close the session
      if (!MailClient.sendMail(&smtp, &message))
        Serial.println("Error sending Email, " + smtp.errorReason());

      failedEntry = true;
      requireCode = 0; // otherwise it may spam email - flag is true now anyways
      currentAttempts = 0;
    }
    else
    {
      requireCode = counter + openTime;
    }
    resetEntry();
  }
  else if (!armed && requireCode != 0 && counter > requireCode)
  {
    armed = true;
    // if there's a beeper, put a tone here I guess!
  }
  else if (digitalRead(doorCheck) == HIGH && !opened && armed)
  { // trigger into entry mode
    resetEntry();
    requireCode = counter + openTime;
    digitalWrite(redLight, HIGH);
    opened = true;
  }
  else if (digitalRead(doorCheck) == LOW && opened)
  { // door has been closed
    opened = false;
  }
  else
  { // idle
    if (!armed)
    {
      if (lastInput == 0 && lastInputTime <= counter)
      {
        digitalWrite(greenLight, HIGH);

        if (!digitalRead(oneButton))
          lastInput = 1;
        if (!digitalRead(twoButton))
          lastInput = 2;
        if (!digitalRead(threeButton))
          lastInput = 3;
        if (!digitalRead(fourButton))
          lastInput = 4;

        if (lastInput != 0)
        {
          digitalWrite(greenLight, LOW);

          inputDigits = inputDigits * 10 + lastInput; /* lastInput * pow(10, getLength(code) - inputAmount - 1);*/
          inputAmount++;
          lastInputTime = counter + debounce;
          Serial.println(inputDigits);
        }
      }
      else if (digitalRead(oneButton) && digitalRead(twoButton) && digitalRead(threeButton) && digitalRead(fourButton)) // reset input
      {
        lastInput = 0;
      }

      if (equalLength(inputDigits, code))
      {
        if (inputDigits == code) // success!
        {
          requireCode = counter + openTime;
          resetEntry();
        }
        else // failed input
        {
          digitalWrite(redLight, LOW);
          delay(500);
          digitalWrite(redLight, HIGH);
          delay(250);
          digitalWrite(redLight, LOW);
          delay(500);
          digitalWrite(redLight, HIGH);
          delay(250);
          digitalWrite(redLight, LOW);
          delay(500);
          digitalWrite(redLight, HIGH);

          resetEntry();
        }
      }
    }

    if (armed)
      digitalWrite(greenLight, LOW);

    if (counter % 4500 == 0)
      digitalWrite(redLight, LOW);
    if (counter % 4500 != 0 && counter % 500 == 0 && !failedEntry)
      digitalWrite(redLight, HIGH);

    if (counter > 1000000000 && requireCode == 0)
      counter = 1; // reset counter when available & getting pretty high
  }

  delay(1);
  counter++;
}
