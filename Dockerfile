# Используем готовый образ Linux со встроенным компилятором g++
FROM gcc:latest

# Копируем файлы твоего протокола внутрь сервера
WORKDIR /app
COPY protocol.cpp .
COPY crypto_variants.cpp .
COPY crypto_variants.h .

# Компилируем твой сервер под Linux
RUN g++ -O3 protocol.cpp crypto_variants.cpp -o protocol_server

# Открываем порт для подключения твоего домашнего клиента
EXPOSE 54321

# Запускаем твой протокол
CMD ["./protocol_server"]
