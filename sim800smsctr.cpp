#include <Arduino.h>
#include "sim800smsctr.h" 

//public definitions
SIM800smscrt::SIM800smscrt(uint8_t receivePin, uint8_t transmitPin, uint8_t dtrPin, Print * outstream, bool inverse_logic, unsigned int buffSize){
	simBoard = new SoftwareSerial(receivePin, transmitPin, inverse_logic, buffSize);

    _stream = outstream; //Инициализация указателя на поток в Случае если NULL вывод производится не будет
    isSIM800Configed = false;
    statusPNWL = false; //флаг указывающий на необходимость проверки номеров из списка;
    PNWL = ""; //список номеров "Допуск" (белый список)

    DTRpin = dtrPin;

    sendingSMS = false; //флаг указывающий на запрос о открытии терминал отправки СМС

    firstCmdQ_RX = NULL; //первый элемент очереди списка отправки команд
    lastCmdQ_RX = NULL; //последний элемент очереди списка отправки команд
    curCmdQ_RX = NULL; //последний элемент очереди списка отправки команд
    cmdCount = 0; //кол-во элементов в очереди списка отправки команд
    isWaitRespose = false; //фактор необходимости ожидания ответа списка отправки команд
    mlsGetCommand = 0; //время следующего отклика объекта в глобальном цикле
    mlsResponse = 0; //время неответа SIM800L

    firstRespQ = NULL; //первый элемент очереди
    lastRespQ = NULL; //последний элемент очереди
    iterRespQ = NULL; //последний элемент очереди
    prev_iterRespQ = NULL; //последний элемент очереди
    respCount = 0; //кол-во элементов в очереди
    haseResponse = false; //фактор наличиия ответа или приема данных
    needRepeatCmd = false; //фактор отрицательного ответа
    haseConnectionError = false;// Ошибка соединения
    mlsGetResponse = 0; //время следующего отклика объекта в глобальном цикле

    firstMsgQ = NULL; //первый элемент очереди
    lastMsgQ = NULL; //последний элемент очереди
    iterMsgQ = NULL; //последний элемент очереди
    msgCount = 0; //кол-во элементов в очереди
    haseMsg = false; //фактор наличиия ответа или приема данных
    mlsGetMsg = 0; //время следующего отклика объекта в глобальном цикле
    mlsSMSGetRequest = 0; //время следующего запроса наличия СМС  в глобальном цикле
    mlsLastATsent = 0;
}

bool SIM800smscrt::begin(uint32_t baudRate){ //преднастройка и установка связи с указанной скоростью вызывать 
	bool b; //Флаг указывающий на состояние шагов
    b = simBoard->begin(baudRate); //установка скорости связи
    delay(2000);
	if(b){ //Если подключеие удалось
        if(_stream != NULL) //В случае если выводной стрим определен 
            _stream->println(F("SIM800l is started, try to sinchronize: ")); //Вывод о готовности к синхронизации
        uint8_t _hopes = 0; //итератор шагов проверки 
        const uint8_t max_hopes = 10; //максимальное число проверок
        b = false; //Выставляем флаг для начала проверок синхронизации
        do{
            _hopes++; 
            if(_stream != NULL){ //В случае если выводной стрим определен 
                _stream->print(F("   Hope "));
                _stream->println(_hopes); //Отпечата номера пробы синхонизации
            }
            SendAT(atAT);//Отправка первичного АТ для синхронизации начала работы
            delay(5000);//Ожидаение ответа с передачей управления серверу
            if(simBoard->available()){ //наличие ответа от SIM800l
                String str_response = simBoard->readString();//Чтение из потока от SIM800l 
                str_response.trim();
                if(_stream != NULL)
                    _stream->println("response : "+str_response);
                b = str_response.indexOf(respOK) > -1; //Проверка ответа есть ли ОК 
            }
        }while(!b && _hopes < max_hopes);
		configureSIM800(); //Вызов конфигурации
    }
    if(_stream != NULL){ //В случае если выводной стрим определен 
        if(b)
            _stream->println(F("SIM800l is ready to use"));
        else
            _stream->println(F("SIM800l is not ready"));
    }
	return b;
}							   

bool SIM800smscrt::available(){ //если есть сообщение ожидающее получения
	return msgCount > 0; //Очередь сообщений не пуста
}

void SIM800smscrt::SendSMS(const String & smsTxt, const String & phoneNumber, bool haseNextNumber){//Отправление СМС по указанному номеру
    SetQueueCmd(String(atSentSMSprefix)+"\""+phoneNumber+"\""); //Отправка запроса на отправку сообщения 
    SetQueueCmd(smsTxt + "\r\n" + (String)((char)26)+ "\r\n" , true);//сразу после получения отклика ">"  
                                                                    //После текста отправляем перенос строки и Ctrl+Z - это терминатор 
                                                                    //указывающий на конец текста 
    if(!haseNextNumber)//Если не указанно что сообщение не последнее в сиписке 
        SetQueueCmd(atDelSentSMS);//отправляемых отправляем команду на удаления из памяти SIM800l
}

void SIM800smscrt::SendSMSbyList(const String & smsTxt, const String & phoneNumbersList){ //Отправление СМС по указанному списку номеров 
                                                                                        //сепаратором номеров в строке является ","
   String _List = phoneNumbersList;
    _List.trim();
    while(_List.length()>8){
        _List.trim();
        String _phone = "";
        int i = _List.indexOf(",");
        if(i > -1){
            _phone = _List.substring(0, i);
            _List.remove(0, i + 1);
        } else {
            _phone = _List;
            _List = "";
        }
        _phone.trim();
        SendSMS(smsTxt, _phone, true);//Вызываем отправку СМС без удаления из памяти SIM800l
        delay(idleTimeMls);//Передача управления внутреннему серверу
    }
    SetQueueCmd(atDelSentSMS);//отправляемых отправляем команду на удаления из памяти SIM800l
}

void SIM800smscrt::SendSMSbyPNWL(const String & smsTxt){ //Отправление СМС по указанному списку номеров
    if(PNWL.length()>0)
        SendSMSbyList(smsTxt, PNWL);
}

void SIM800smscrt::SendAT(const String & msgAT){ //Отправка АТ команды на SIM800L через сериал порт
    if(_stream != NULL){
        _stream->print(F("cmd: "));
        _stream->println(msgAT);
    }
    simBoard->println(msgAT);//ussd
}

void SIM800smscrt::SendUSSD(const String & msgUSSD){//Отправление USSD запроса
    SetQueueCmd(String(atUSSD)+"\""+msgUSSD+"\""); //Отправка команды на USSD запрос 
}


String SIM800smscrt::GetSMS(String & phoneNumber){ //получение очередного СМС или комманды от SIM800L
    SMS_t * _sms = GetMsg();//Получение очередного СМС из очереди
    phoneNumber = _sms->phone_number;//Присвоение для передачи номера телефона
    String message = _sms->message;//Определение для возвратаа результата
    if(_sms != NULL)
        delete _sms; //Освобождение памяти
    return message; //возврат текста сообщения
}

String SIM800smscrt::GetSMS(){ //получение очередного СМС или комманды от SIM800L
    SMS_t * _sms = GetMsg();//Получение очередного СМС из очереди
    String message = _sms->message;//Определение для возвратаа результата
    if(_sms != NULL)
        delete _sms; //Освобождение памяти
    return message; //возврат текста сообщения
}

SMS_t * SIM800smscrt::GetSMSt(){ //получение очередного СМС или комманды от SIM800L
    return GetMsg();//Получение очередного СМС из очереди
}

void SIM800smscrt::loop(){ //функия вызова в бесконечном цикле для этапного выполнения; НУЖНО ДОПИЛИТь
    uint32_t CurrentMillis = millis();
 
    if(simBoard->available()){ //Есть поток от SIM800l
        SetQueueResp(simBoard->readString()); //Отправка потока в очередь на обработку откликов
        delay(idleTimeMls);//Передача управления внутреннему серверу
    }

    if(isWaitRespose && mlsResponse < CurrentMillis){ //Истечение таймера отмены ожидания отклика
        isWaitRespose = false;//Возврать состояния флага в состояние снятия блокировки
        iterRespQ = NULL;//Установка итератора в NULL
        prev_iterRespQ = NULL;//Установка итератора в NULL
        delay(idleTimeMls);//Передача управления внутреннему серверу
    }

    if(respCount){//Если есть сообщения от SIM800l
        if(isWaitRespose){//Если ожидается отклик от SIM800l
            iterRespQ = firstRespQ; //Определение итератора на первый элемент списка

            while(iterRespQ != NULL){//Цикл прохода по списку сообщений от SIM800l для поиска требуемого отклика 
                iterRespQ->_msg.trim();
                bool intexOfOK = iterRespQ->_msg.indexOf(respOK) > -1; //проверка ответа содержащего OK - положительный ответ о выполнении
                bool containERROR = iterRespQ->_msg.indexOf(respERROR) > -1;//проверка ответа содержащего ERROR завершение с ошибкой
                bool containSMSstart = sendingSMS && iterRespQ->_msg.indexOf(respSendSMS) > -1;//проверка ответа содержащего ">", что говорит о 
                                                                                        //готовностик отправке СМС
                if(intexOfOK or containERROR or containSMSstart){//При наличии отклика "OK" или "ERROR" флаг ожидание деактивируется
                    if(_stream != NULL)
                        _stream->println("resp : "+iterRespQ->_msg);
                    isWaitRespose = false;//Возврать состояния флага в состояние снятия блокировки
                   
                    if((intexOfOK && iterRespQ->_msg.length()==2) || containERROR){ //В случае когда ответ ОК пиходить вместе с информацией 
                                                                                    //необходимой для обработки ее не надо удалять
                        PopResp(iterRespQ);//Удаление из очереди указанного объекта
                    }

                    iterRespQ = NULL;//Установка итератора в NULL
                    prev_iterRespQ = NULL;//Установка итератора в NULL
                } else {
                    prev_iterRespQ = iterRespQ;//Запоминаем указатель на предыдущий объект
                    iterRespQ = iterRespQ->ptrNext;//Отбираем указатель на следующий объект списка
                }
                delay(idleTimeMls);//Передача управления внутреннему серверу
            }
        } else {//Очередная обработка ответов от SIM800l
            MsgQueue * ptrMsg = GetResp(); //Временный указател очередного объекта списка ответов SIM800l. 
            ptrMsg->_msg.trim();
            if(_stream != NULL)
                _stream->println("get : "+ptrMsg->_msg);
            if(ptrMsg->_msg.indexOf(respCMTI) > -1){//Проверка на получение ответа о наличии СМС
                SetQueueCmd(atGetAllSMS); //Отправка АТ команды на предоставление всего списка СМС
            } else if(ptrMsg->_msg.indexOf(respCMGL) > -1){//Обработка СМС
                int16_t nextindexOf; //начальный индекса следующего сообщения  
                uint8_t break_iterator = 0;
                const uint8_t max_break_iterator = 20;
                do{
                    break_iterator++;
                    nextindexOf = ptrMsg->_msg.indexOf(respCMGL, 7);//определение начального индекса следующего сообщения 
                    String msgSMS;                                                        //поиск производится с 7-го элемента дабы пропустить уже определенный
                    if(nextindexOf > -1){
                        msgSMS = ptrMsg->_msg.substring(0, nextindexOf); //Полный текст одного SMS
                        ptrMsg->_msg.remove(0, nextindexOf); //Удаление текста выбранного SMS из общего списка
                        ptrMsg->_msg.trim();
                    } else {
                        msgSMS = ptrMsg->_msg;
                        ptrMsg->_msg.trim();
                        ptrMsg->_msg = "";
                    } 
                    delay(idleTimeMls);//Передача управления внутреннему серверу
                    SMS_t * _sms = new SMS_t();//создаем новый объект SMS 
                    if(parseSMS(msgSMS, _sms))
                        SetMsgtoQueue(_sms);//Добавление сообщения в очередь для передачи ресипиенту
                    else{
                        if(_sms != NULL)
                            delete _sms; //Освобождение памяти 
                    }
                }while((nextindexOf > -1) && (break_iterator < max_break_iterator));//цыкл продолжается пока есть следующее сообщение
                mlsSMSGetRequest = millis() + SMSGetRequestPeriod;
            } else if(ptrMsg->_msg.indexOf(respCUSD) > -1){//Обработка USSD ответов
                ptrMsg->_msg.replace(respCUSD, ret_ussdRESP);
                SMS_t * _sms = new SMS_t();//создаем новый объект SMS
                _sms->message = ptrMsg->_msg; //передаем в объект USSD ответ с префиксом для отправки
                SetMsgtoQueue(_sms);//Добавление сообщения в очередь для передачи ресипиенту
                delay(idleTimeMls);//Передача управления внутреннему серверу
            }
            if(ptrMsg != NULL)
                delete ptrMsg;//После обработки ответа очищаем память
        }
    }

    if((cmdCount > 0) && !isWaitRespose && (mlsGetCommand < CurrentMillis)){//Очередная обработка списка комад для передачи на SIM800l.  
                                                                    //Выполняется если в очереди есть элементы, если нет запрета 
                                                                    //(т.е. не ожидается ответ от SIM800l) и если прошло время пустого хода
        curCmdQ_RX = GetQueueCmd(); //получение очередного отклика из очереди
        SendAT(curCmdQ_RX->_cmd); //отправка АТ команды на SIM800l
        isWaitRespose = curCmdQ_RX->waitResponse;//Установка флага необходимости ожидания ответа. 
                                                //Если флаг true отправка следующих команд БЛОКИРУЕТСЯ
        if(isWaitRespose)
            mlsResponse = millis() + resposeMaxperiod; //Запуск таймера отмены блокировки ожидания отклика 
        mlsGetCommand = millis() + respose_period; //Запуск таймера пустого хода 
        delay(idleTimeMls);//Передача управления внутреннему серверу
    }

    if(!isWaitRespose && mlsSMSGetRequest < CurrentMillis){ //Нступило время таймера запроса о налии СМС
        SetQueueCmd(atGetAllSMS);//Отправка запроса о наличии СМС 
        mlsSMSGetRequest = millis() + SMSGetRequestPeriod; 
        delay(idleTimeMls);//Передача управления внутреннему серверу
    }

    if(!isWaitRespose && !cmdCount && mlsLastATsent < CurrentMillis){ //Нступило время таймера запроса о налии СМС
        SetQueueCmd(atAT);//Отправка запроса о наличии СМС 
        mlsLastATsent = millis() + LastATsentPeriod; 
        delay(idleTimeMls);//Передача управления внутреннему серверу
    }

    delay(idleTimeMls);//Передача управления внутреннему серверу
}

void SIM800smscrt::SetPhoneNumbersWL(const String & _phoneNumbers){PNWL = _phoneNumbers;}//Установка списка номеров

void SIM800smscrt::SetPNWLstatus(bool status){statusPNWL = status;} //Установка флага фильтрации по номерам

bool SIM800smscrt::GetPNWLstatus(){return statusPNWL;} //Получение статуса флага фильтрации по номерам

//private definitions
void SIM800smscrt::configureSIM800(){
	//  sendATCommand("AT+CLVL?", true);          // Запрашиваем громкость динамика
	//  sendATCommand("AT+CMGF=1", true);         // Включить TextMode для SMS
	//  sendATCommand("AT+DDET=1,0,0", true);     // Включить DTMF
	//  sendATCommand("ATE0V0+CMEE=1;&W", true);     // Режим работы готового продукта ECHO выключен тип ответов и ошибок цифровой 
	//  sendATCommand("ATE1V1+CMEE=2;&W", true);     // Автонастройка скорости для готового кода
	SetQueueCmd(atEcho_Off); //Отключение echo режима 
	SetQueueCmd("ATS0=0");     // Автоответ звонка выключен
	SetQueueCmd("AT+CLIP=1");     // Автоопределине номера включен
	SetQueueCmd("AT+CMGF=1;&W"); //Установка текстового режима СМС
	//  sendATCommand("AT+CMGF=1", true);     // Выбор формата SMS (textMode)
    // SetQueueCmd("AT+CMGD=0,4", true);     //Удаление всех SMS
	//  sendATCommand("AT+CMGL=\"REC UNREAD\",1", true);     // 
	//AT+CBC запрос напряжения
	isSIM800Configed = true; 
}

void SIM800smscrt::SetQueueCmd(const String & cmd, bool needResponse){//запись команды в очередь для передачи SIM800l
    if(firstCmdQ_RX == NULL){//при пустой очереди 
        firstCmdQ_RX = new CmdQueue(cmd, needResponse); //создание нового члена и присвоение его указателю первого члена 
        lastCmdQ_RX = firstCmdQ_RX; //отождествление указателей первого и последнего членов
    } else { //если очередь не пуста (количество членов > 0)
        lastCmdQ_RX->ptrNext = new CmdQueue(cmd, needResponse);//создание очередного члена списка
        lastCmdQ_RX = lastCmdQ_RX->ptrNext;//переназначение указателя последнего члена
    }
    cmdCount++; 
    mlsLastATsent = millis() + LastATsentPeriod;
}

CmdQueue * SIM800smscrt::GetQueueCmd(){ //Получение очередного объекта (команды) из очереди для передачи SIM800l
    CmdQueue * tmpPtr = firstCmdQ_RX; //выбор очередного 
    firstCmdQ_RX = firstCmdQ_RX->ptrNext; //переопределение первого члена списка. В случае если следующий член списка отсутвует первый превращается в NULL
    cmdCount--;
    if(firstCmdQ_RX == NULL){
        lastCmdQ_RX = NULL; //Обнуление указателя последнего члена очереди в случае если взятый член последний.
        cmdCount = 0;//контрольное обнуление
    }
    return tmpPtr; //возврат выбранного объекта
}

void SIM800smscrt::SetQueueResp(const String & _resp){//запись отклика в очередь для обработки получен от SIM800l
    if(firstRespQ == NULL){//при пустой очереди 
        firstRespQ = new MsgQueue(_resp); //создание нового члена и присвоение его указателю первого члена 
        lastRespQ = firstRespQ; //отождествление указателей первого и последнего членов
    } else { //если очередь не пуста (количество членов > 0)
        lastRespQ->ptrNext = new MsgQueue(_resp);//создание очередного члена списка
        lastRespQ = lastRespQ->ptrNext;//переназначение указателя последнего члена
    } 
    respCount++;
}

MsgQueue * SIM800smscrt::GetResp(){//Получение очередного объекта (отклика) из очереди полученных от SIM800l
    MsgQueue * tmpPtr = firstRespQ; //выбор очередного 
    firstRespQ = firstRespQ->ptrNext; //переопределение первого члена списка. В случае если следующий член списка отсутвует первый превращается в NULL
    respCount--;
    if(firstRespQ == NULL){
        lastRespQ = NULL; //Обнуление указателя последнего члена очереди в случае если взятый член последний.
        respCount = 0;//контрольное обнуление
    }
    return tmpPtr; //возврат выбранного объекта
}

void SIM800smscrt::PopResp(MsgQueue * ptrResp){//Удалени и-того объекта (отклика) из очереди полученных от SIM800l
    if(ptrResp == NULL)
        return;

    MsgQueue * tmpPtr = firstRespQ; //выбор первого элемента 
    MsgQueue * tmpPtrPrevious = NULL; //обнуление предитератора

    while(tmpPtr != NULL && tmpPtr != ptrResp){ //Повторение цикла пока член не найден
        tmpPtrPrevious = tmpPtr;// определение предитератора
        tmpPtr = tmpPtr->ptrNext; //выбор следующего объекта
        delay(idleTimeMls);//Передача управления внутреннему серверу
    }

    if(tmpPtr == ptrResp){ //При нахождении требуемого объекта
        if(tmpPtr == lastRespQ) //Если найденный объект последний в очереди
            lastRespQ = tmpPtrPrevious; //присваеваем предитератор
        if(tmpPtr == firstRespQ) //Если найденный объект первый в очереди
            firstRespQ = firstRespQ->ptrNext; //передвигаем очередь на следующий член
        else if(tmpPtrPrevious != NULL) //если предитератор не ноль, это означает что найденный член не первый
            tmpPtrPrevious->ptrNext = tmpPtr->ptrNext;

        respCount--; //Уменьшаем количество 
        if(firstRespQ == NULL){
            lastRespQ = NULL; //Обнуление указателя последнего члена очереди в случае если взятый член последний.
            respCount = 0;//контрольное обнуление
        }
        delete tmpPtr; //Освобождаем память
    }
}

void SIM800smscrt::SetMsgtoQueue(SMS_t * _msg){//запись команды в очередь для получения ресипиентом
    if(firstMsgQ == NULL){//при пустой очереди 
        firstMsgQ = _msg; //создание нового члена и присвоение его указателю первого члена 
        lastMsgQ = firstMsgQ; //отождествление указателей первого и последнего членов
    } else { //если очередь не пуста (количество членов > 0)
        lastMsgQ->ptrNext = _msg;//создание очередного члена списка
        lastMsgQ = lastMsgQ->ptrNext;//переназначение указателя последнего члена
    } 
    msgCount++;
}

SMS_t * SIM800smscrt::GetMsg(){//Получение очередного объекта (команды) в очередь для получения ресипиентом
    SMS_t * tmpPtr = firstMsgQ; //выбор очередного 
    firstMsgQ = firstMsgQ->ptrNext; //переопределение первого члена списка. В случае если следующий член списка отсутвует первый превращается в NULL
    msgCount--;
    if(firstMsgQ == NULL){
        lastMsgQ = NULL; //Обнуление указателя последнего члена очереди в случае если взятый член последний.
        msgCount = 0;//контрольное обнуление
    }
    return tmpPtr; //возврат выбранного объекта
}

bool SIM800smscrt::checkSMS(const String & msg){ //Проверяем является ли сообщение СМС-ом
    return (msg.indexOf("+CMGL: ") > -1) && //проверка контекста сообщения на формат СМС 
            (msg.indexOf(",\"REC UNREAD\",\"") > -1) &&  //прочитан ли СМС
            (msg.indexOf("\",\"\",\"") > -1) && 
            msg.indexOf("\"\r") > -1; //Окончательный терминатор
}

bool SIM800smscrt::parseSMS(String & msg, SMS_t * _sms){//Парсинг СМС текста в объект 
    msg.trim();//

    if(!checkSMS(msg)) //проверка на соответствие формату СМС иногда приходят крякозябры при помехах
        return false; //текс не соответсвует формату

    _sms->StringToSMS(msg); //Преобразование текста СМС в объект 

    SetQueueCmd(String(atDeleteSMS) + String(_sms->index)); //Отправляет сообщене SIM800L для удаления сообщения из списка

    if(statusPNWL)  
        return (PNWL.indexOf(_sms->phone_number.substring(4)) > -1); //есть ли полученный номер в списке разрешенных 
                                                            //при проверке отрезается код стараны типа +374
    return true;
}


//Definitons of other types
void SMS_t::StringToSMS(String & str_sms){//Функия преобразования текста в тип СМС, получаемый //параметр str_sms преобразуется в текст сообщения
    String str_tmp = ""; //Временная строчная переменная
    uint8_t index_tmp = 0; //Временны индекс

    str_sms.remove(0, 7); //отрезаем лишние префиксы "+CMGL: " - 7 символов

    //Поиск индекса СМС 
        index_tmp = str_sms.indexOf(",");//Поиск индекса в строке конца представлени индекса СМС
        this->index = str_sms.substring(0, index_tmp).toInt(); //Отбираем индекс СМС 
        str_sms.remove(0, index_tmp+1);//Удаление текса об индексе вместе с запятой

    delay(idleTimeMls);//Передача управления внутреннему серверу    

    //Поиск статуса сообщения
        index_tmp = str_sms.indexOf(",");//Поиск индекса в строке конца представлени индекса СМС
        this->stat = str_sms.substring(0, index_tmp); //Отбираем статус сообщения СМС 
        str_sms.remove(0, index_tmp+1);//Удаление текса об индексе вместе с запятой
        this->stat.replace("\"","");//Удаления кавычек с обоих сторон статуса

    delay(idleTimeMls);//Передача управления внутреннему серверу

    //Поиск номера телефона
        index_tmp = str_sms.indexOf(",");//Поиск индекса в строке конца представлени индекса СМС
        this->phone_number = str_sms.substring(0, index_tmp); //Отбираем номер телефона отправителя СМС 
        str_sms.remove(0, index_tmp+1);//Удаление текса об индексе вместе с запятой
        this->phone_number.replace("\"","");//Удаления кавычек с обоих сторон номера телефона

    delay(idleTimeMls);//Передача управления внутреннему серверу

    //Удаление не обрабатываемого (не нужного) параметра 
        index_tmp = str_sms.indexOf(",");//Поиск индекса в строке конца представлени индекса СМС
        str_sms.remove(0, index_tmp+1);//Удаление текса об индексе вместе с запятой

    delay(idleTimeMls);//Передача управления внутреннему серверу
 
    //Поиск дата и время полученя СМС
        index_tmp = str_sms.indexOf("\r");//Поиск индекса в строке конца представлени индекса СМС
        this->date_str = str_sms.substring(0, index_tmp); //Отбираем дату получения СМС 
        str_sms.remove(0, index_tmp+1);//Удаление текса об индексе вместе с запятой
        this->date_str.replace("\"","");//Удаления кавычек с обоих сторон даты и времени телефона

    //Поиск дата и время полученя СМС
        index_tmp = str_sms.indexOf("\r\n\r\nOK");//Поиск индекса в строке конца представлени индекса СМС
        this->message = str_sms.substring(0, index_tmp); //Отбираем дату получения СМС 
        this->message.trim();
    
    delay(idleTimeMls);//Передача управления внутреннему серверу
    
    // str_sms.trim(); //удаление разделителей
    // str_sms.trim(); //удаление разделителей
    // this->message = str_sms; //Отбираем текст СМС
}