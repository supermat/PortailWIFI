#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
// WIFI Manager , afin de gérer la connexion au WIFI de façon plus intuitive
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <FS.h>   //Include SPIFFS

#define led 2
#define PINPortail 5

ESP8266WebServer server(80);         // serveur WEB sur port 80
File fsUploadFile;

/*void handleRoot(){
  server.sendHeader("Location", "/index.html",true);   //Redirige vers page index.html sur SPIFFS
  server.send(302, "text/plane","");
}*/

// fonction de traitement SETLED
void handleLED(){
  String SetLED = server.arg("LED");
  Serial.println(SetLED);
  if (SetLED=="on")  digitalWrite(led, LOW);
  else  digitalWrite(led, HIGH);
  
  server.send(200, "text/plane",SetLED);
}

void openRelay(int pinRelay) {
  pinMode(pinRelay, OUTPUT);
  digitalWrite(pinRelay, 1);
  Serial.print("Opening relay pin ");
  Serial.println(pinRelay);
  delay(1000);
  digitalWrite(pinRelay, 0);
}

void handleWebRequests(){
  if(loadFromSpiffs(server.uri())) return;
  String message = "File Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.println(message);
}

 
void setup() {
  Serial.begin(115200);    // on démarre le moniteur série

  //******** WiFiManager ************
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  
  //Si besoin de fixer une adresse IP
  //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //Forcer à effacer les données WIFI dans l'eprom , permet de changer d'AP à chaque démarrage ou effacer les infos d'une AP dans la mémoire ( à valider , lors du premier lancement  )
  //wifiManager.resetSettings();
  
  //Récupère les identifiants   ssid et Mot de passe dans l'eprom  et essaye de se connecter
  //Si pas de connexion possible , il démarre un nouveau point d'accés avec comme nom , celui définit dans la commande autoconnect ( ici : AutoconnectAP )
  wifiManager.autoConnect("AutoConnectPortail");
  //Si rien indiqué le nom par défaut est  ESP + ChipID
  //wifiManager.autoConnect();
    
  // ****** Fin config WIFI Manager ************

  

  Serial.println("Attente de connection.");
  // on attend d'être connecté  au WiFi avant de continuer
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  //Si connexion affichage info dans console
  Serial.println("Connecté");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); 


  //demarrage file system
  if (SPIFFS.begin()) {
    Serial.println("Demarrage file System");
  }
  else Serial.println("Erreur file System");
  

  //Initialize Webserver
  //server.on("/",handleRoot); // Inutile car redirigé dans loadFromSpiffs
  server.on("/setLED",handleLED); //pour gerer led
  server.on("/list", HTTP_GET, handleFileList); // Liste des fichiers
  server.on("/edit", HTTP_PUT, handleFileCreate); //create file
  server.on("/edit", HTTP_DELETE, handleFileDelete);  //delete file
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);
  server.onNotFound(handleWebRequests); //Set setver all paths
  server.begin(); 
  
  pinMode(led, OUTPUT);     // Initialise la broche "led" comme une sortie 
}
 

void loop() {
  // à  chaque interation, la fonction hand traite les requêtes 
  server.handleClient();
             
}

bool loadFromSpiffs(String path){
  String dataType = "text/plain";
  /******* Gestion de l'ouverture du portail ******/
  if(path.endsWith("portail"))
  {
    openRelay(PINPortail);
    Serial.println(path);
    path = path.substring(0, path.length()-7); //On redirige vers l'url sans portail
    
    Serial.println(path);
  }
  /******* Fin Gestion de l'ouverture du portail ******/
  if(path.endsWith("edit")) path += ".htm"; //Pour edit on redirige vers edit.htm
  
  if(path.endsWith("/")) path += "index.htm"; //Redirection du path par defaut vers index.htm

  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".html")) dataType = "text/html";
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";
  else if(path.endsWith(".gz")) dataType =  "application/x-gzip";
  
String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
  }
      
  Serial.println(path);
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (server.hasArg("download")) dataType = "application/octet-stream";
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
  }

  dataFile.close();
  return true;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //Serial.print("handleFileUpload Data: "); Serial.println(upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileDelete: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileCreate: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}
  
  String path = server.arg("dir");
  Serial.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  
  output += "]";
  server.send(200, "text/json", output);
}
