#ifndef SIM800SMSCTR_H
#define SIM800SMSCTR_H

#include <SoftwareSerial.h>

const String charCRLF = "\r\n";
const String ctrlZ = (String)((char)26)+ charCRLF;


//Констаннты
    const uint16_t maxCommandCount = 200; //максимальная длина очереди
    const uint32_t respose_period = 2000; //период свободного хода без отклика
    const uint32_t resposeMaxperiod = 60000; //период свободного хода без отклика
    const uint32_t LastATsentPeriod = 20000; //период свободного хода без отклика
    const uint32_t SMSGetRequestPeriod = 1 * 60000;  //период запроса наличия СМС  в случае долгого ожидания
    const uint8_t idleTimeMls = 5;//Время в миллисекундах для передачи управления серверу для избежания
                                    // зависаний WIFI и HTTP сервера
    

//АТ команды 
    const char* const atAT PROGMEM = "AT"; // Пустая АТ команда
    const char* const atEcho_Off PROGMEM = "ATE0&W"; // Отключение echo режима
    const char* const atEcho_On PROGMEM = "ATE1&W"; // Отключение echo режима
    const char* const atDelSentSMS PROGMEM = "AT+CMGDA=\"DEL SENT\""; // Удаление всех отправленных СМС
    const char* const atSentSMSprefix PROGMEM = "AT+CMGS="; // Удаление всех отправленных СМС
    const char* const atGetAllSMS PROGMEM = "AT+CMGL=\"ALL\",1"; //Вызов списка полученных СМС без изменения статус на прочитанный
    const char* const atDeleteSMS PROGMEM = "AT+CMGD="; //Команда удаления сообщения. формат CMGD=<index>[,<delflag>] 
                                                        //<index> - индекс полученного сообщения если deflag не пуст и больше нуля
                                                        //то индекс не учитывается и выполняется груповое удаление
    const char* const atSignalQuality PROGMEM = "AT+CSQ"; //Команда проверка уровня сигнала
    const char* const atPowerStatus PROGMEM = "AT+CBC"; //Команда проверка напряжения
    const char* const atUSSD PROGMEM = "AT+CUSD=1,"; //Команда проверка напряжения

//Ответы от SIM800l
    const char* const respOK PROGMEM = "OK"; // Положительный ответ на выполнение некой AT команды
    const char* const respERROR PROGMEM = "ERROR"; // При выполнение некой AT команды Ошибка
    const char* const respSendSMS PROGMEM = ">"; // Положительный ответ о готовности отправки СМС
    const char* const respCMTI PROGMEM = "+CMTI: \"SM\""; //Ответ о SIM800 о получении СМС
    const char* const respCMGL PROGMEM = "+CMGL:"; //Ответ о SIM800 со списком полученых СМС
    const char* const respCUSD PROGMEM = "+CUSD:"; //Ответ о SIM800 с USSD ответов

    const char* const ret_ussdRESP PROGMEM = "ussdresp#"; //Возврат ответат от о SIM800 с USSD ответов
 
 

//Типы статусов
    const char* const SMSstatREC_UNREAD PROGMEM = "REC UNREAD"; //Received unread messages Полученные непреочитанные сообщения
    const char* const SMSstatREC_READ PROGMEM = "REC READ"; //Received read messages Полученные преочитанные сообщения
    const char* const SMSstatSTO_UNSENT PROGMEM = "STO UNSENT"; //Stored unsent messages Записанные неотправленные сообщения
    const char* const SMSstatSTO_SENT PROGMEM = "STO SENT"; //Stored sent messages Записанные отправленные сообщения
    const char* const SMSstatALL PROGMEM = "ALL"; //All messages Все сообщения
    const char* const SMSstatNAN PROGMEM = "NAN"; //All messages Все сообщения

    const uint8_t SMSstatusType_count = 5;
    const char* const SMSstatusN[]={//Вектор наименований статусов СМС
                                    SMSstatREC_UNREAD, //Received unread messages Полученные непреочитанные сообщения
                                    SMSstatREC_READ, //Received read messages Полученные преочитанные сообщения
                                    SMSstatSTO_UNSENT, //Stored unsent messages Записанные неотправленные сообщения
                                    SMSstatSTO_SENT, //Stored sent messages Записанные отправленные сообщения
                                    SMSstatALL //All messages Все сообщения
    };

//Дополнительные операционные типы

    struct CmdQueue{//Тип для организации очереди АТ команд 
        String _cmd; //Текст АТ команды 
        bool waitResponse;//флаг указывающий на необходимость ожидания ответа после выполнения АТ команды
        CmdQueue * ptrNext;//указатель на следующий элемент очереди
        CmdQueue(const String & cmd){ //Коснтруктор с объязательным уазанием необходимости ожидания ответа
        ptrNext = NULL;
        _cmd = cmd;
        waitResponse = true;
        };
        CmdQueue(const String & cmd, bool needResponse){//Коснтруктор с альтернативным уазанием необходимости ожидания ответа
        ptrNext = NULL;
        _cmd = cmd;
        waitResponse = needResponse;
        }

    };

    struct MsgQueue{//Тип организации очереди Сообщений
        String _msg; //Текстовое сообщение 
        MsgQueue * ptrNext;//указатель на следующий элемент очереди
        MsgQueue(const String & msg){ //конструктор
            ptrNext = NULL;
            _msg = msg;
        };
    };

    struct SMS_t{//Тип СМС с возможностью организации очереди
        uint16_t index;//Индекс СМС в памяти
        String stat;//Статус СМС
        String phone_number;//Номер телефона
        String date_str;//Дата и время  СМС 
        String message;//текст сообщения
        SMS_t * ptrNext;//указатель на следующий элемент очереди
        SMS_t(){
            index = 0;//Индекс СМС в памяти
            stat = "";//Статус СМС
            phone_number = "";//Номер телефона
            date_str = "";//Дата и время  СМС 
            message = "";//текст сообщения
            ptrNext = NULL;
        }
        void StringToSMS(String & str_sms); //Функия преобразования текста в тип СМС, получаемый //параметр str_sms преобразуется в текст сообщения
                                            
    };

class SIM800smscrt {
public:
    SIM800smscrt(uint8_t receivePin, uint8_t transmitPin, uint8_t dtrPin, Print * outstream = NULL, bool inverse_logic = false, unsigned int buffSize = 64);

    void SendSMS(const String & smsTxt, const String & phoneNumber, bool haseNextNumber = false); //Отправление СМС по указанному номеру
    void SendSMSbyList(const String & smsTxt, const String & phoneNumbersList); //Отправление СМС по указанному списку номеров
    void SendSMSbyPNWL(const String & smsTxt); //Отправление СМС по указанному списку номеров
    void SendAT(const String & msgAT); //Отправление СМС по указанному списку номеров
    void SendUSSD(const String & msgUSSD); //Отправление USSD запроса

    String GetSMS(String & phoneNumber); //получение очередного СМС или комманды от SIM800L
    String GetSMS(); //получение очередного СМС или комманды от SIM800L
    SMS_t * GetSMSt(); //получение очередного СМС или комманды от SIM800L в виде указателя на СМС 
                       //объязательно освободить (delet) после использованиея 
    bool available(); //если есть сообщение ожидающее получения

    bool begin(uint32_t baudRate); //преднастройка и установка связи с указанной скоростью вызывать 
                                   // только в setup() использованна функция delay()
    
    void loop(); //функия вызова в бесконечном цикле для этапного выполнения;

    void SetPhoneNumbersWL(const String & _phoneNumbers); //Установка списка номеров
    void SetPNWLstatus(bool status = true); //Установка флага фильтрации по номерам
    bool GetPNWLstatus(); //Получение статуса флага фильтрации по номерам

protected:
    SoftwareSerial * simBoard; //UART подключения к SIM800l
    Print * _stream; //Поток для вывода 
private: 
    void configureSIM800();
    void SetQueueCmd(const String & cmd, bool needResponse = true);//запись команды в очередь для передачи SIM800l
    CmdQueue * GetQueueCmd(); //Получение очередного объекта (команды) из очереди для передачи SIM800l
    void SetQueueResp(const String & _resp);//запись отклика в очередь для обработки получен от SIM800l
    MsgQueue * GetResp(); //Получение очередного объекта (отклика) из очереди полученных от SIM800l
    void PopResp(MsgQueue * ptrResp); //Удалени и-того объекта (отклика) из очереди полученных от SIM800l
    void SetMsgtoQueue(SMS_t * _msg);//запись команды в очередь для получения ресипиентом
    SMS_t * GetMsg();//Получение очередного объекта (команды) в очередь для получения ресипиентом

    bool checkSMS(const String & msg); //Проверяем является ли сообщение СМС-ом
    bool parseSMS(String & msg, SMS_t * _sms);//Парсинг СМС
    

    bool isSIM800Configed; //флаг указывающий на выполненность конфигурации
    bool statusPNWL; //флаг указывающий на необходимость проверки номеров из списка;
    String PNWL; //список номеров "Допуск" (белый список)

    uint8_t DTRpin;

    bool sendingSMS; //флаг указывающий на запрос о открытии терминал отправки СМС

    CmdQueue * firstCmdQ_RX; //первый элемент очереди
    CmdQueue * lastCmdQ_RX; //последний элемент очереди
    CmdQueue * curCmdQ_RX; //последний элемент очереди
    uint16_t cmdCount; //кол-во элементов в очереди
    bool isWaitRespose; //фактор необходимости ожидания ответа
    uint32_t mlsGetCommand; //время следующего отклика объекта в глобальном цикле
    uint32_t mlsResponse; //время неответа SIM800L

    MsgQueue * firstRespQ; //первый элемент очереди откликов
    MsgQueue * lastRespQ; //последний элемент очереди откликов
    MsgQueue * iterRespQ; //итератор элемент очереди откликов используется для алтернативного прохода по очереди с целью поиска
    MsgQueue * prev_iterRespQ; //предитератор или итератор предыдущего объекта очереди откликов используется для алтернативного прохода по очереди с целью поиска
    uint16_t respCount; //кол-во элементов в очереди откликов
    bool haseResponse; //фактор наличиия ответа или приема данных 
    bool needRepeatCmd; //фактор отрицательного ответа
    bool haseConnectionError;// Ошибка соединения
    uint32_t mlsGetResponse; //время следующего отклика объекта в глобальном цикле

    SMS_t * firstMsgQ; //первый элемент очереди
    SMS_t * lastMsgQ; //последний элемент очереди
    SMS_t * iterMsgQ; //последний элемент очереди
    uint16_t msgCount; //кол-во элементов в очереди
    bool haseMsg; //фактор наличиия ответа или приема данных
    uint32_t mlsGetMsg; //время следующего отклика объекта в глобальном цикле

    uint32_t mlsSMSGetRequest; //время следующего запроса наличия СМС  в глобальном цикле
    uint32_t mlsLastATsent; //время последней отправки АТ команды

};


#endif