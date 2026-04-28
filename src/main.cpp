#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

//Define the pin for the factory reset button. 
//This should be a pin that can trigger an interrupt (like GPIO3).
#define RESET_BUTTON_PIN 3 // GPIO3

//We will also use the built-in LED to indicate when the device is in configuration mode.
#define LED_PIN 8 // GPIO8 typically connected to onboard LED or use an external LED if not available

#define BLINK_DELAY 500
#define FIRMWARE_VERSION "1.0" // Firmware version string
// GitHub owner/org change this to your GitHub username or organization name
#define GITHUB_OWNER "Ben-BJD"
// GitHub repo name where releases are published change this to your repository name
#define GITHUB_REPO "how-to-flash-ESP32-over-wifi-auto-update"

// We use a volatile variable to indicate that the factory reset button was pressed. 
//This is set in the ISR
volatile bool factoryResetRequested = false;

WiFiClientSecure client;
HTTPClient http;

String firmwareUrl = ""; // This will be set after checking for updates

// Intermediate certificate for GitHub (Needed for secure connection to GitHub API)
//Expires 2036
const char* ca_cert_intermediate_github = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDXzCCAuagAwIBAgIQNuBZ7YiN1Xrt1XC2cn+b2jAKBggqhkjOPQQDAzBfMQsw
CQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1T
ZWN0aWdvIFB1YmxpYyBTZXJ2ZXIgQXV0aGVudGljYXRpb24gUm9vdCBFNDYwHhcN
MjEwMzIyMDAwMDAwWhcNMzYwMzIxMjM1OTU5WjBgMQswCQYDVQQGEwJHQjEYMBYG
A1UEChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQDEy5TZWN0aWdvIFB1YmxpYyBT
ZXJ2ZXIgQXV0aGVudGljYXRpb24gQ0EgRFYgRTM2MFkwEwYHKoZIzj0CAQYIKoZI
zj0DAQcDQgAEaKGnbAUnBYljHDmn/yUhxe3TLxKYuyzc9VXoSaCEV5F73Fhfa/Si
/RMsmwTFW3R9s7J6JpYZFmu4do3vk/Vgl6OCAYEwggF9MB8GA1UdIwQYMBaAFNEi
2kxZ8UtfJjiqndbu6w3D+6lhMB0GA1UdDgQWBBQXmagEwW/kLXCoChA9A9PpGrgm
YzAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBADAdBgNVHSUEFjAU
BggrBgEFBQcDAQYIKwYBBQUHAwIwGwYDVR0gBBQwEjAGBgRVHSAAMAgGBmeBDAEC
ATBUBgNVHR8ETTBLMEmgR6BFhkNodHRwOi8vY3JsLnNlY3RpZ28uY29tL1NlY3Rp
Z29QdWJsaWNTZXJ2ZXJBdXRoZW50aWNhdGlvblJvb3RFNDYuY3JsMIGEBggrBgEF
BQcBAQR4MHYwTwYIKwYBBQUHMAKGQ2h0dHA6Ly9jcnQuc2VjdGlnby5jb20vU2Vj
dGlnb1B1YmxpY1NlcnZlckF1dGhlbnRpY2F0aW9uUm9vdEU0Ni5wN2MwIwYIKwYB
BQUHMAGGF2h0dHA6Ly9vY3NwLnNlY3RpZ28uY29tMAoGCCqGSM49BAMDA2cAMGQC
MFsKnBQDh64l+v+aUYWjDCJKQMxHUUGmcwAYDIjJ9pbRYItMCIx5xu0oUb6sIfTX
qQIwPddcsDE4KdeLu1hJdpHgdLvsHAK3vygyLGujMU9xBJCDackRT93VHEE0gppg
NqdV
-----END CERTIFICATE-----
)EOF";

// Google Trust Services - GTS Root
// This covers release-assets.githubusercontent.com
//expires 2035
const char* ca_cert_google_gts = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// This is our Interrupt Service Routine (ISR)
// It must be as fast as possible!
void IRAM_ATTR onFactoryReset() 
{
    factoryResetRequested = true;
}

// This callback is called when the ESP enters configuration mode (i.e., when it creates the Access Point).
void configModeCallback(WiFiManager *wiFiManager) 
{
    Serial.println("Entered config mode!");
    Serial.println(WiFi.softAPIP());
    
    // Turn on a physical LED so the user knows to look at their phone!
    digitalWrite(LED_PIN, HIGH); // Turn on the LED to indicate we're in config mode
}

// Function to blink the LED
void blink()
{
  // Blink the LED to indicate that the ESP32 is awake
  digitalWrite(LED_PIN, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(BLINK_DELAY);                      // wait for half a second
  digitalWrite(LED_PIN, LOW);   // turn the LED off by making the voltage LOW
  delay(BLINK_DELAY);                      // wait for half a second
}

void performTasks() 
{
    // Placeholder for actual tasks (e.g., reading sensors, sending data)
    //just blink the led for now to show that we're alive and running our main code. In a real project, this is where you'd put your main logic.
    for (size_t i = 0; i < 10; i++)
    {
      blink();
    }
}

bool hasFirmwareUpdates()
{
  bool result = false;

  const char* api_url = "https://api.github.com/repos/" GITHUB_OWNER "/" GITHUB_REPO "/releases/latest";
  Serial.printf("Checking for updates at: %s\n", api_url);
  
  //Force clear any previous connection residues
  client.stop(); 
  
  //GitHub is heavy; give the C3 more time to do the math
  client.setHandshakeTimeout(30); 

  http.begin(client, api_url);
  http.addHeader("User-Agent", "ESP32-C3-Updater");
  
  //Disable connection "keep-alive" to free up RAM immediately after the request
  http.addHeader("Connection", "close");

  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) 
  {
      Serial.println("Successfully connected to GitHub API. Parsing response...");
      // Get the stream from the wifi client
      WiFiClient* stream = http.getStreamPtr();
      
      // Create a JSON document to hold the parsed data
      JsonDocument doc; 

      //Create a filter to only parse the fields we care about, which saves RAM and speeds up parsing
      JsonDocument filter;
      filter["tag_name"] = true;// We want the release tag to compare versions
      filter["assets"][0]["browser_download_url"] = true;// We want the download URL for the updated firmware binary

      //Apply filter during parsing
      DeserializationError error = deserializeJson(doc, *stream, DeserializationOption::Filter(filter));

      if (error) 
      {
          Serial.print("JSON Parse failed: ");
          Serial.println(error.c_str());
      }
      else
      {
        const char* latest_version = doc["tag_name"]; 
        Serial.print("Latest Version on GitHub: ");
        Serial.println(latest_version);

        // Compare with your local FIRMWARE_VERSION
        if (String(latest_version) != String(FIRMWARE_VERSION)) 
        {
            Serial.println("New update found!");
            
            // Extract the firmware download URL from the JSON response
            firmwareUrl = doc["assets"][0]["browser_download_url"].as<String>();
            Serial.print("Firmware download URL: ");
            Serial.println(firmwareUrl);
            
            result = true;
        }
        else 
        {
            Serial.println("No update needed.");
        }
      }
  } 
  else 
  {
      Serial.printf("Error occurred while checking updates: %d\n", httpResponseCode);
  }
  
  http.end(); // Always end the HTTP connection
  return result;
}

//This function will follow redirects manually to find the final download URL, 
//which is necessary for GitHub releases that often redirect to S3 buckets
//We do this manually to have more control and to swap in the correct certificate for the final host
void traceRedirects(String url) 
{
    String currentUrl = url;
    bool resolved = false;

    while (!resolved) 
    {
      // If we see a redirect to githubusercontent.com, we know we've hit the final download host
      if (currentUrl.indexOf("githubusercontent.com") > 0) 
      {
          Serial.println("Redirect to Google Storage detected. Swapping to GTS Root...");
          //swap to the Google/GTS cert for the final heavy download
          client.setCACert(ca_cert_google_gts);
          firmwareUrl = currentUrl; // Final URL is the one we want to download from
          Serial.println(firmwareUrl);
          return;
      }

      Serial.printf("Connecting to: %s\n", currentUrl.c_str());
      
      http.begin(client, currentUrl);
      http.addHeader("User-Agent", "ESP32-C3-Recon");
      
      // CRITICAL: Tell HTTPClient NOT to follow redirects automatically
      http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
      
      // We only care about the "Location" header to find the next URL in the redirect chain
      const char * headerKeys[] = {"Location"};
      http.collectHeaders(headerKeys, 1);

      int code = http.GET();
      Serial.printf("HTTP Code: %d\n", code);

      if (code == 301 || code == 302) 
      {
          String nextUrl = http.header("Location");
          Serial.printf("Redirected to: %s\n", nextUrl.c_str());
          currentUrl = nextUrl; // Prepare for the next hop
          http.end();
      } 
      else 
      {
          Serial.print(code);
          Serial.println(" Final destination reached or error occurred.");
          resolved = true;
          http.end();
      }
    }
}

void updateFirmware() 
{
  Serial.println("Starting OTA update...");

  Serial.println("Resolving redirect...");

  traceRedirects(firmwareUrl);

  Serial.println("Starting OTA update with final URL...");

  httpUpdate.setLedPin(LED_PIN, LOW); // Optional: Blink LED
  
  //The ESP32 needs to know it's allowed to take its time
  client.setHandshakeTimeout(60);
  
  t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

  switch (ret) 
  {
      case HTTP_UPDATE_FAILED:
          Serial.printf("Update failed (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
          break;
      case HTTP_UPDATE_NO_UPDATES:
          Serial.println("No update needed.");
          break;
      case HTTP_UPDATE_OK:
          Serial.println("Update successful!"); // Device will reboot automatically
          break;
  }
}

// Set time via NTP, as required for x.509 validation
void setClock() 
{
  const char* ntpServer = "pool.ntp.org";
  const long  gmtOffset_sec = 0;
  const int   daylightOffset_sec = 3600;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  Serial.print(F("Waiting for NTP time sync: "));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    yield();
    delay(500);
    Serial.print(F("."));
    now = time(nullptr);
  }

  Serial.println(F(""));
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print(F("Current time: "));
  char buf[26];
  Serial.print(asctime_r(&timeinfo, buf));
}

void checkFactoryReset()
{
  delay(1000);
  // Check if the factory reset button was pressed
  if (factoryResetRequested) 
  {
      // do the heavy lifting outside of interrupt context
      WiFiManager wm;
      wm.resetSettings();        // clears WifiManager NVS
      delay(100);                // give NVS a moment to commit
      ESP.restart();             // now reboot
  }
}

void setup() 
{
    // Set up the reset button with an interrupt
    pinMode(RESET_BUTTON_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(RESET_BUTTON_PIN), onFactoryReset, RISING);

    pinMode(LED_PIN, OUTPUT);// initialize digital pin LED_BUILTIN as an output.
    digitalWrite(LED_PIN, LOW); // Start with the LED off. We will turn it on in config mode.

    // Start serial communication
    Serial.begin(115200);
    Serial.println("--- Starting Custom WiFiManager ---");

    // Initialize WiFiManager
    // Local intialization. There is no need to keep it in memory after setup() has finished.
    WiFiManager wm;
    wm.setAPCallback(configModeCallback);// Set the callback that will run when we enter configuration mode

    // Set a timeout so the ESP doesn't hang forever if nobody configures the WiFi
    wm.setConfigPortalTimeout(300); // Auto close portal after 5 minutes

    // Print debug info to the Serial monitor (helpful for troubleshooting)
    wm.setDebugOutput(true); 
    // -------------------------

    // Wifi Manager CUSTOMISATIONS: 

    // ==========================================================
    // CUSTOMIZE THE MENU
    // ==========================================================
    // Available options: "wifi", "wifinoscan", "info", "param", "close", "sep", "erase", "restart", "exit"
    // Let's remove all but the configuration and update options to simplify the user experience.
    std::vector<const char *> menu = {"wifi", "sep", "update"};
    wm.setMenu(menu);

    //Custom Title
    wm.setTitle("My Custom Device Setup");

    Serial.println("Starting WiFiManager...");
    Serial.println("If no valid WiFi is saved, it will create an Access Point.");

    // autoConnect() performs the magic.
    // It tries to connect to the last saved WiFi. 
    //    - WiFiManager stores the SSID/password as key/value pairs via NVS and reads them back on boot.
    // If it fails or no WiFi is saved, it sets up an Access Point.
    // AP Name: "ESP32_Config_AP", AP Password: "pizza4tree"
    bool res;
    res = wm.autoConnect("ESP32_Config_AP", "pizza4tree");

    // Check the result
    if(!res) 
    {
        Serial.println("Failed to connect or hit timeout.");
        Serial.println("Rebooting to try again...");
        delay(3000);
        ESP.restart();
    } 
    else 
    {
      digitalWrite(LED_PIN, LOW); // Turn off the LED since we're no longer in config mode

      // If you get here, you have successfully connected to the local router!
      Serial.println("");
      Serial.println("SUCCESS! Connected to WiFi.");

      Serial.println("--- Current Configuration ---");
      Serial.print("Assigned IP: ");
      Serial.println(WiFi.localIP());
      Serial.println("-----------------------------");
    
      // Set the clock before we do anything else, so that certificate validation works correctly
      setClock();

      // Set the intermediate cert for GitHub API calls
      client.setCACert(ca_cert_intermediate_github);

      // Check for firmware updates and apply if available
      if( hasFirmwareUpdates() )
      {
          updateFirmware();
      }

      performTasks();
      checkFactoryReset();

      //Go to sleep for 1 hour
      Serial.println("Going to sleep now");
      //Waits for the transmission of outgoing serial data to complete.
      Serial.flush();
      //esp_sleep_enable_timer_wakeup(3600ULL * 1000000ULL);
      //esp_deep_sleep_start();
    }
}

void loop() 
{
  // This will never be reached
}