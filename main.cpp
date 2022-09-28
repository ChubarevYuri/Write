/*
 ОС: Linux

ЯП: С/С++ по выбору.

По результатам напишите сколько чистого времени на ушло на задание.

Сделать два консольных  приложения.

Первое пишет в TCP/UDP/... сокет сообщение-пакет, содержащее, как минимум,
номер пакета, время отправки с точностью до миллисекунд, собственно данные и
контрольную сумму данных MD5.  Данные - это массив int16 слов. Размер массива
варьируется от 600 до 1600 слов.  Приложение отправляет 1000 пакетов с
интервалом 10 мс. Потом спит 10 секунд и отправляет следующие 1000 пакетов с
интервалом 10 мс, после чего завершает свою работу. По факту отправки каждого
пакета печатает в консоль “Sent: #номер пакета и #время отправки”.

Второе приложение – имеет 2 потока.  Первый поток - слушает сокет и пишет
принятые пакеты в кольцевой буфер размером 16 элементов. По факту приёма
печатает в консоль “Received: #номер пакета #время приёма и результат проверки
контрольной суммы PASS/FAIL”.  Второй поток делает выборку из кольцевого буфера
при наличии там необработанных данных и имеет задержку обработки пакета 15 мс.
По факту обработки пакета печатает в консоль “Processed: #номер пакета #время
окончания обработки и результат проверки контрольной суммы PASS/FAIL”.

Приложения должны работать как на одном компьютере, так и на разных. Компьютеры
могут иметь разные архитектуры, например, первый x86, второй PowerPC.

Можно использовать любые Linux библиотеки, но желательно ограничиться
использованием стандартных, BOOST и POSIX/Phtreads.  Для MD5 можно
использовать openssl.

Сборка с помощью CMAKE.

Настройки портов и прочие проще всего хардкодить константами с комментариями,
либо, на Ваш выбор, можно передавать в командной строке. Данные для пакетов
можно генерить на лету, хардкодить или читать из текстового файла. Главное,
чтобы было видно, что корректно передаются массивы произвольных данных
указанного выше типа и размера.

Предложите или напишите тесты, для чего определите максимально допустимый
процент потерь пакетов, исходя из вышеописанных задержек и таймингов. Ошибки
данных и контрольной суммы не допускаются.
 */

#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <string>
#include <random>
#include <openssl/evp.h>
#include <iomanip>

typedef unsigned char byte;
using namespace boost::asio;

//генерация md5 хэша
std::string md5(const std::string& content) {
    EVP_MD_CTX* context = EVP_MD_CTX_create();
    const EVP_MD* md = EVP_md5();
    unsigned char md_val[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    std::string result;

    EVP_DigestInit_ex2(context, md, NULL);
    EVP_DigestUpdate(context, content.c_str(), content.length());
    EVP_DigestFinal_ex(context, md_val, &md_len);
    EVP_MD_CTX_destroy(context);

    result.resize(md_len*2);
    for (unsigned int i = 0; i<md_len; i++) {
        std::sprintf(&result[i*2], "%02x", md_val[i]);
    }

    return result;
}


int int16_tToBytes(byte mass[], int pos, int16_t val) {
    for(int i = 0; i < 2; ++i) {
        mass[i+pos] = ((char*)&val)[i];
    }
    return (pos+2);
}

int time_pointToBytes(byte mass[], int pos, std::chrono::system_clock::time_point val) {
    for(int i = 0; i < 8; ++i) {
        mass[i+pos] = ((char*)&val)[i];
    }
    return (pos+8);
}

io_service service;

int main()
{
    service.run();
    std::srand(std::time(nullptr));

    //сетевое соединение
    ip::udp::endpoint sender_ep = ip::udp::endpoint(ip::address::from_string("127.0.0.1"), 8888);
    ip::udp::socket sock(service);

    //номер пакета
    int16_t i = 0;
    std::chrono::system_clock::time_point t = std::chrono::system_clock::now();
    while (i < 2000) {

        //Время создания пакета
        t = std::chrono::system_clock::now();

        //Количество передаваемых элементов (случайное)
        int16_t len = std::rand() % 1000 +600;

        //Массив байтов для передачи по сети
        byte mass[2+8+2+2*len];
        int pos = 0;

        //Запись пакета в массив байтов
        pos = int16_tToBytes(mass, pos, i);
        pos = time_pointToBytes(mass, pos, t);
        pos = int16_tToBytes(mass, pos, len);
        for (int j = 0; j < len; j++) {
            pos = int16_tToBytes(mass, pos, (std::rand() % (INT16_MAX - INT16_MIN) + INT16_MIN));
        }

        //Массив байтов в строку
        std::string msg = "";
        for (int j = 0; j <= pos; j++) {
            msg += (char)mass[j];
        }

        //Добавление хэша
        msg += md5(msg);

        //Отправка пакета (при невозможности соединения - сообщение об ошибке)
        int b = 0;
        while (b<1000) {
            try {
                sock.connect(sender_ep);
                sock.send(buffer(msg,4096));
                sock.close();
                b=1000;
            }
            catch(std::exception& e) {
                b++;
                if (b == 1000) {
                    std::cout<<"Connect err"<<std::endl;
                }
            }
        }
        time_t tt = std::chrono::system_clock::to_time_t(t);

        //Сообщение о отправленном пакете
        std::cout<<"Sent: " << i << "\t" << std::put_time(std::localtime(&tt), "%F %T") << "." << (std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count()%1000) << std::endl;

        i++;

        //Таймер задержки между отправками
        if(i == 1000){
            std::this_thread::sleep_until(t+std::chrono::milliseconds(10000));
        }
        else {
            std::this_thread::sleep_until(t+std::chrono::milliseconds(10));
        }
    }
    return 0;
}
