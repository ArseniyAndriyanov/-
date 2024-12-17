#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <queue>
#include <condition_variable>

using namespace std;

// Глобальные переменные
mutex fileMutex;
unordered_map<char, ofstream> outputFiles;

// Очередь для хранения строк
queue<string> lineQueue;
condition_variable condition;
mutex queueMutex;
bool finishedReading = false; // Флаг, указывающий на окончание чтения

// Функция для обработки строк и записи их в файлы
void consumer() {
    while (true) {
        string line;

        {
            unique_lock<mutex> lock(queueMutex);
            condition.wait(lock, [] { return !lineQueue.empty() || finishedReading; });

            if (lineQueue.empty() && finishedReading) {
                break; // Выход из цикла, если нет больше строк и чтение завершено
            }

            if (!lineQueue.empty()) {
                line = move(lineQueue.front());
                lineQueue.pop();
            }
        }

        if (!line.empty()) {
            char firstChar = tolower(line[0]);

            {
                lock_guard<mutex> lock(fileMutex);
                if (outputFiles.find(firstChar) == outputFiles.end()) {
                    string outputDir = "output/";
                    filesystem::create_directory(outputDir);
                    string outputFileName = outputDir + firstChar + ".txt";
                    outputFiles[firstChar].open(outputFileName, ios::app);

                    if (!outputFiles[firstChar].is_open()) {
                        cerr << "Ошибка открытия файла: " << outputFileName << endl;
                        continue;
                    }
                }
                outputFiles[firstChar] << line << endl;
            }
        }
    }
}

// Функция для чтения строк из файла и добавления их в очередь
void producer(const string& fileName, streampos start, streampos end) {
    ifstream inputFile(fileName, ios::binary);
    if (!inputFile.is_open()) {
        cerr << "Не удалось открыть файл: " << fileName << endl;
        return;
    }

    inputFile.seekg(start);

    if (start != 0) {
        string dummy;
        getline(inputFile, dummy); // Пропускаем неполную строку
    }

    string line;
    while (inputFile.tellg() < end && getline(inputFile, line)) {
        if (!line.empty()) {
            unique_lock<mutex> lock(queueMutex);
            lineQueue.push(move(line)); // Добавляем строку в очередь
            condition.notify_one(); // Уведомляем потребителя
        }
    }

    inputFile.close();
}

// Сортировка выходных файлов
void sortFile(const string& fileName) {
    ifstream inputFile(fileName);
    if (!inputFile.is_open()) {
        cerr << "Не удалось открыть файл для сортировки: " << fileName << endl;
        return;
    }

    vector<string> lines;
    string line;
    while (getline(inputFile, line)) {
        lines.push_back(line);
    }
    
    inputFile.close();
    
    sort(lines.begin(), lines.end());

    ofstream outputFile(fileName);
    if (!outputFile.is_open()) {
        cerr << "Не удалось открыть файл для записи: " << fileName << endl;
        return;
    }

    for (const auto& sortedLine : lines) {
        outputFile << sortedLine << endl;
    }
    
    outputFile.close();
}

// Сортировка сгенерированных файлов
void sortGeneratedFiles() {
    for (const auto& pair : outputFiles) {
        string fileName = "output/" + string(1, pair.first) + ".txt";
        sortFile(fileName);
    }
}

int main() {
    string inputFileName = "input.txt";
    
    ifstream inputFile(inputFileName, ios::binary | ios::ate);
    
    if (!inputFile.is_open()) {
        cerr << "Не удалось открыть файл: " << inputFileName << endl;
        return 1;
    }

    streampos fileSize = inputFile.tellg();
    
    inputFile.close();
    
    if (fileSize == 0) {
        cout << "Пустой файл" << endl;
        return 0;
    }

    const int threadCount = 4; // Количество потоков
    vector<thread> consumers;

    // Создание потребителей
    for (int i = 0; i < threadCount; ++i) {
        consumers.emplace_back(consumer);
    }

    // Определение размера блока для каждого потока
    streampos chunkSize = fileSize / threadCount;

    // Создание производителей для каждой части файла
    for (int i = 0; i < threadCount; ++i) {
        streampos start = i * chunkSize;
        streampos end = (i == threadCount - 1) ? fileSize : start + chunkSize;

        producer(inputFileName, start, end); // Производитель читает строки из файла
    }

   // Установка флага завершения чтения и уведомление потребителей
   {
       unique_lock<mutex> lock(queueMutex);
       finishedReading = true; 
   }
   condition.notify_all(); // Уведомляем всех потребителей о завершении чтения

   // Ожидание завершения всех потоков-потребителей
   for (auto& consumerThread : consumers) {
       consumerThread.join();
   }

   // Закрываем все файлы
   for (auto& pair : outputFiles) {
       if (pair.second.is_open()) {
           pair.second.close();
       }
   }

   // Сортируем сгенерированные файлы в многопоточном режиме
   sortGeneratedFiles();

   cout << "Обработка и сортировка завершены." << endl;

   return 0;
}